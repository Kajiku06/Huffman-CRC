#include "huffman.hpp"
#include <algorithm>
#include <fstream>
#include <functional>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

using namespace std;

// Odczytuje zrodlo binarnie, aby kazdy z 256 mozliwych bajtow byl liczony
// jako osobny symbol. Dzieki temu kodowanie dziala takze dla plikow binarnych.
bool oblicz_model_Huffman(
    const filesystem::path& nazwa_pliku,
    array<uint64_t, 256>& model,
    uint64_t& liczba_znakow,
    string& komunikat_bledu) {
    // Zerujemy wyniki, zeby funkcja mogla bezpiecznie zostac uzyta ponownie.
    model.fill(0);
    liczba_znakow = 0;

    // Tryb binarny zapobiega zmianom bajtow, np. konwersji koncow linii.
    ifstream plik(nazwa_pliku, ios::binary);
    if (!plik) {
        komunikat_bledu = "Nie mozna otworzyc pliku wejsciowego: " +
                          nazwa_pliku.string();
        return false;
    }

    // Strumien zwraca char, wiec konwertujemy go na unsigned char:
    // wartosci powyzej 127 nie moga wtedy stac sie indeksami ujemnymi.
    char znak = 0;
    while (plik.get(znak)) {
        const auto indeks_znaku = static_cast<unsigned char>(znak);

        // Jawnie wykrywamy przepelnienie, zanim zwiekszymy liczniki.
        if (model[indeks_znaku] == UINT64_MAX ||
            liczba_znakow == UINT64_MAX) {
            komunikat_bledu =
                "Plik jest zbyt duzy, aby obliczyc model Huffmana.";
            return false;
        }

        // Model zapamietuje czestosc kazdego symbolu, a drugi licznik
        // przechowuje rozmiar zrodla wyrazony w bajtach.
        ++model[indeks_znaku];
        ++liczba_znakow;
    }

    // EOF jest prawidlowym koncem; kazdy inny stan oznacza blad odczytu.
    if (!plik.eof()) {
        komunikat_bledu = "Blad podczas odczytu pliku wejsciowego.";
        return false;
    }
    return true;
}

// Tworzy drzewo, laczac za kazdym razem dwa wezly o najmniejszej czestosci.
// Indeksy w vectorze sa uzywane jako stabilne odwolania do lisci i rodzicow.
vector<galaz_drzewa> utworz_drzewo(
    const array<uint64_t, 256>& model, int& korzen) {
    vector<galaz_drzewa> drzewo;
    // Dla maksymalnie 256 lisci drzewo binarne ma najwyzej 511 wezlow.
    drzewo.reserve(511);

    // Para przechowuje (czestosc, indeks wezla). Porzadek par sprawia,
    // ze przy rownej czestosci rozstrzyga mniejszy indeks - wynik jest
    // deterministyczny dla tego samego modelu.
    using Element_kolejki = pair<uint64_t, int>;
    priority_queue<Element_kolejki, vector<Element_kolejki>,
                   greater<Element_kolejki>> kolejka;

    // Kazdy bajt, ktory wystapil w zrodle, staje sie lisciem drzewa.
    for (int indeks_znaku = 0; indeks_znaku < 256; ++indeks_znaku) {
        const auto liczba_wystapien = model[static_cast<size_t>(indeks_znaku)];
        if (liczba_wystapien == 0) {
            continue;
        }

        // Lisc nie ma dzieci; jego indeks_znaku identyfikuje bajt zrodla.
        const int indeks_galezi = static_cast<int>(drzewo.size());
        drzewo.push_back({liczba_wystapien, indeks_znaku, -1, -1});
        kolejka.push({liczba_wystapien, indeks_galezi});
    }

    // Puste zrodlo nie ma lisci ani korzenia.
    if (kolejka.empty()) {
        korzen = -1;
        return drzewo;
    }

    // Kazde polaczenie zdejmuje dwa najrzadsze wezly i dodaje ich rodzica.
    // Wewnetrzny wezel oznaczamy indeksem znaku -1.
    while (kolejka.size() > 1) {
        const auto pierwszy = kolejka.top();
        kolejka.pop();
        const auto drugi = kolejka.top();
        kolejka.pop();

        const int indeks_ojca = static_cast<int>(drzewo.size());
        drzewo.push_back(
            {pierwszy.first + drugi.first, -1, pierwszy.second, drugi.second});

        // Rodzic wraca do kolejki z suma czestosci obu potomkow.
        kolejka.push({drzewo.back().ilosc_wystapien, indeks_ojca});
    }

    // Gdy zostaje jeden element, jest on korzeniem calego drzewa.
    korzen = kolejka.top().second;
    return drzewo;
}

// Sprawdza, czy pliki wskazuja na ten sam zasob - takze przez aliasy sciezek.
bool czy_te_same_pliki(const filesystem::path& plik1,
                       const filesystem::path& plik2) {
    // equivalent wykrywa ten sam istniejacy plik, np. wskazany przez link.
    error_code blad;
    if (filesystem::equivalent(plik1, plik2, blad) && !blad) {
        return true;
    }

    // Dla nieistniejacego pliku wynikowego porownujemy sciezki kanoniczne,
    // ktore rozwiazuja m.in. elementy "." i "..".
    blad.clear();
    const auto sciezka1 = filesystem::weakly_canonical(plik1, blad);
    if (blad) {
        return false;
    }
    const auto sciezka2 = filesystem::weakly_canonical(plik2, blad);
    if (blad) {
        return false;
    }
    return sciezka1 == sciezka2;
}

namespace {
constexpr char ZNACZNIK_FORMATU[] = {'H', 'U', 'F', 'C', 'P', 'P', '5', '\n'};

void utworz_kody(
    int indeks_galezi, const vector<galaz_drzewa>& drzewo,
    vector<vector<unsigned char>>& tablica_kodowania,
    array<int, 256>& indeksy_lisci, vector<unsigned char>& kod) {
    const auto& galaz = drzewo[static_cast<size_t>(indeks_galezi)];
    tablica_kodowania[static_cast<size_t>(indeks_galezi)] = kod;
    if (galaz.indeks_znaku >= 0) {
        indeksy_lisci[static_cast<size_t>(galaz.indeks_znaku)] = indeks_galezi;
        if (kod.empty()) {
            tablica_kodowania[static_cast<size_t>(indeks_galezi)].push_back(0);
        }
        return;
    }
    kod.push_back(0);
    utworz_kody(galaz.potomek1, drzewo, tablica_kodowania, indeksy_lisci, kod);
    kod.back() = 1;
    utworz_kody(galaz.potomek2, drzewo, tablica_kodowania, indeksy_lisci, kod);
    kod.pop_back();
}

void zapisz_drzewo(
    ostream& plik, const vector<galaz_drzewa>& drzewo,
    size_t liczba_lisci, uint64_t liczba_znakow) {
    const auto liczba_galezi = liczba_lisci == 1
        ? size_t{1} : drzewo.size() - liczba_lisci;
    plik << liczba_znakow << '\n' << liczba_galezi << '\n';
    if (liczba_lisci == 1) {
        plik << "256 " << drzewo.front().indeks_znaku << " 257\n";
        return;
    }
    const auto indeks_potomka = [liczba_lisci, &drzewo](int indeks) {
        const auto& potomek = drzewo[static_cast<size_t>(indeks)];
        return potomek.indeks_znaku >= 0
            ? potomek.indeks_znaku
            : 256 + indeks - static_cast<int>(liczba_lisci);
    };
    for (size_t indeks = 0; indeks < liczba_galezi; ++indeks) {
        const auto& galaz = drzewo[liczba_lisci + indeks];
        plik << 256 + indeks << ' ' << indeks_potomka(galaz.potomek1)
             << ' ' << indeks_potomka(galaz.potomek2) << '\n';
    }
}

bool zapisz_pliki_pomocnicze(
    const filesystem::path& nazwa_pliku_wejsciowego,
    const array<uint64_t, 256>& model,
    const vector<galaz_drzewa>& drzewo,
    const vector<vector<unsigned char>>& tablica_kodowania,
    const array<int, 256>& indeksy_lisci,
    uint64_t liczba_znakow,
    string& komunikat_bledu) {
    // W zadanym formacie obu tabel kod zajmuje dokladnie cztery bajty.
    // Nie zapisujemy po cichu niepelnego kodu, gdy jego dlugosc przekracza 32 bity.
    for (const auto& kod : tablica_kodowania) {
        if (kod.size() > 32) {
            komunikat_bledu =
                "Nie mozna zapisac plikow table/modtab: kod Huffmana "
                "przekracza limit 32 bitow.";
            return false;
        }
    }

    // Wszystkie pliki pomocnicze trafiaja do jednego katalogu obok zrodla.
    const auto katalog_tymczasowy =
        nazwa_pliku_wejsciowego.parent_path() / "tmp";
    error_code blad;
    filesystem::create_directories(katalog_tymczasowy, blad);
    if (blad) {
        komunikat_bledu = "Nie mozna utworzyc katalogu plikow tymczasowych: " +
                          blad.message();
        return false;
    }

    const auto nazwa_bazowa = nazwa_pliku_wejsciowego.stem().string();
    const auto nazwa_pliku_modelu =
        katalog_tymczasowy / (nazwa_bazowa + ".model");
    const auto nazwa_pliku_sort =
        katalog_tymczasowy / (nazwa_bazowa + ".sort");
    const auto nazwa_pliku_drzewa =
        katalog_tymczasowy / (nazwa_bazowa + ".tree");
    const auto nazwa_pliku_tablicy =
        katalog_tymczasowy / (nazwa_bazowa + ".table");
    const auto nazwa_pliku_modelu_zmodyfikowanego =
        katalog_tymczasowy / (nazwa_bazowa + ".modmodel");
    const auto nazwa_pliku_tablicy_zmodyfikowanej =
        katalog_tymczasowy / (nazwa_bazowa + ".modtab");
    const auto nazwa_pliku_liczby_znakow =
        katalog_tymczasowy / (nazwa_bazowa + ".chars");
    filesystem::remove(nazwa_pliku_liczby_znakow, blad);
    if (blad) {
        komunikat_bledu = "Nie mozna usunac starego pliku .chars: " +
                          blad.message();
        return false;
    }

    // Model zawiera tylko symbole obecne w pliku; zerowe czestosci pomijamy.
    vector<element_zrodla> posortowany_model;
    for (size_t indeks = 0; indeks < model.size(); ++indeks) {
        if (model[indeks] != 0) {
            posortowany_model.push_back(
                {static_cast<int>(indeks), model[indeks]});
        }
    }
    sort(posortowany_model.begin(), posortowany_model.end(),
         [](const element_zrodla& pierwszy,
            const element_zrodla& drugi) {
             if (pierwszy.ilosc_wystapien != drugi.ilosc_wystapien) {
                 return pierwszy.ilosc_wystapien >
                        drugi.ilosc_wystapien;
             }
             return pierwszy.indeks_znaku < drugi.indeks_znaku;
         });

    ofstream plik_modelu(nazwa_pliku_modelu, ios::binary | ios::trunc);
    if (!plik_modelu) {
        komunikat_bledu = "Nie mozna zapisac pliku modelu: " +
                          nazwa_pliku_modelu.string();
        return false;
    }
    for (size_t indeks = 0; indeks < model.size(); ++indeks) {
        if (model[indeks] != 0) {
            plik_modelu << indeks << ' ' << model[indeks] << '\n';
        }
    }
    if (!plik_modelu) {
        komunikat_bledu = "Blad zapisu pliku modelu.";
        return false;
    }

    // Zapisuje niepelny model w naturalnej kolejnosci kodow bajtow.
    ofstream plik_sort(nazwa_pliku_sort, ios::binary | ios::trunc);
    if (!plik_sort) {
        komunikat_bledu = "Nie mozna zapisac posortowanego modelu: " +
                          nazwa_pliku_sort.string();
        return false;
    }
    for (const auto& element : posortowany_model) {
        plik_sort << element.indeks_znaku << ' '
                  << element.ilosc_wystapien << '\n';
    }
    if (!plik_sort) {
        komunikat_bledu = "Blad zapisu posortowanego modelu.";
        return false;
    }

    // .sort przechowuje te same symbole, posortowane malejaco po czestosci.
    // Przy rownej czestosci zachowujemy deterministyczna kolejnosc kodow.
    // Liscie odpowiadaja symbolom zrodlowym, pozostale elementy sa wezlami.
    const auto liczba_lisci = posortowany_model.size();
    ofstream plik_modelu_zmodyfikowanego(
        nazwa_pliku_modelu_zmodyfikowanego, ios::binary | ios::trunc);
    if (!plik_modelu_zmodyfikowanego) {
        komunikat_bledu = "Nie mozna zapisac zmodyfikowanego modelu: " +
                          nazwa_pliku_modelu_zmodyfikowanego.string();
        return false;
    }
    // Oprocz symboli zrodla model zawiera czestosci nowych wezlow drzewa.
    for (size_t indeks = 0; indeks < model.size(); ++indeks) {
        if (model[indeks] != 0) {
            plik_modelu_zmodyfikowanego << indeks << ' ' << model[indeks]
                                        << '\n';
        }
    }
    for (size_t indeks = liczba_lisci; indeks < drzewo.size(); ++indeks) {
        const int symbol =
            256 + static_cast<int>(indeks - liczba_lisci);
        plik_modelu_zmodyfikowanego << symbol << ' '
                                    << drzewo[indeks].ilosc_wystapien << '\n';
    }
    if (!plik_modelu_zmodyfikowanego) {
        komunikat_bledu = "Blad zapisu zmodyfikowanego modelu.";
        return false;
    }

    ofstream plik_drzewa(nazwa_pliku_drzewa, ios::binary | ios::trunc);
    if (!plik_drzewa) {
        komunikat_bledu = "Nie mozna zapisac drzewa Huffmana: " +
                          nazwa_pliku_drzewa.string();
        return false;
    }
    // Pierwszy wiersz to liczba bajtow zrodla, drugi - liczba dodanych wezlow.
    zapisz_drzewo(plik_drzewa, drzewo, liczba_lisci, liczba_znakow);
    if (!plik_drzewa) {
        komunikat_bledu = "Blad zapisu drzewa Huffmana.";
        return false;
    }

    // Kodujemy maksymalnie 32 bity w czterech bajtach, od bitu najstarszego.
    const auto spakuj_kod = [](const vector<unsigned char>& kod) {
        array<unsigned char, 4> bajty{};
        for (size_t indeks = 0; indeks < kod.size(); ++indeks) {
            const size_t indeks_bajtu = indeks / 8;
            bajty[indeks_bajtu] = static_cast<unsigned char>(
                (bajty[indeks_bajtu] << 1) | kod[indeks]);
        }
        const size_t reszta_bitow = kod.size() % 8;
        if (reszta_bitow != 0) {
            bajty[kod.size() / 8] = static_cast<unsigned char>(
                bajty[kod.size() / 8] << (8 - reszta_bitow));
        }
        return bajty;
    };

    // .table pokazuje symbol, cztery bajty kodu jako liczby i dlugosc w bitach.
    ofstream plik_tablicy(nazwa_pliku_tablicy, ios::trunc);
    if (!plik_tablicy) {
        komunikat_bledu = "Nie mozna zapisac tablicy kodowania: " +
                          nazwa_pliku_tablicy.string();
        return false;
    }
    const auto zapisz_wiersz_tablicy =
        [&spakuj_kod](ostream& plik, int symbol,
                      const vector<unsigned char>& kod) {
            const auto bajty = spakuj_kod(kod);
            plik << symbol << ' ';
            for (const auto bajt : bajty) {
                plik << static_cast<unsigned int>(bajt) << ' ';
            }
            plik << kod.size() << '\n';
        };
    for (const auto& element : posortowany_model) {
        const auto symbol = static_cast<size_t>(element.indeks_znaku);
        const int indeks_liscia = indeksy_lisci[symbol];
        if (indeks_liscia < 0) {
            continue;
        }
        const auto& kod =
            tablica_kodowania[static_cast<size_t>(indeks_liscia)];
        zapisz_wiersz_tablicy(plik_tablicy, static_cast<int>(symbol), kod);
    }
    for (size_t indeks = liczba_lisci; indeks < drzewo.size(); ++indeks) {
        const int symbol =
            256 + static_cast<int>(indeks - liczba_lisci);
        zapisz_wiersz_tablicy(plik_tablicy, symbol, tablica_kodowania[indeks]);
    }
    if (!plik_tablicy) {
        komunikat_bledu = "Blad zapisu tablicy kodowania.";
        return false;
    }

    ofstream plik_tablicy_zmodyfikowanej(
        nazwa_pliku_tablicy_zmodyfikowanej, ios::binary | ios::trunc);
    if (!plik_tablicy_zmodyfikowanej) {
        komunikat_bledu =
            "Nie mozna zapisac zmodyfikowanej tablicy kodowania: " +
            nazwa_pliku_tablicy_zmodyfikowanej.string();
        return false;
    }
    for (const auto& element : posortowany_model) {
        const auto symbol = static_cast<size_t>(element.indeks_znaku);
        const int indeks_liscia = indeksy_lisci[symbol];
        if (indeks_liscia < 0) {
            continue;
        }
        const auto& kod =
            tablica_kodowania[static_cast<size_t>(indeks_liscia)];
        zapisz_wiersz_tablicy(
            plik_tablicy_zmodyfikowanej, static_cast<int>(symbol), kod);
    }
    if (!plik_tablicy_zmodyfikowanej) {
        komunikat_bledu = "Blad zapisu zmodyfikowanej tablicy kodowania.";
        return false;
    }
    return true;
}
}

bool kompresja_Huffman(
    const filesystem::path& nazwa_pliku_wejsciowego,
    const filesystem::path& nazwa_pliku_wynikowego,
    wynik_kompresji& wynik,
    string& komunikat_bledu) {
    // Najpierw tworzy model i drzewo, potem zapisuje naglowek oraz kody bitowe.
    if (czy_te_same_pliki(nazwa_pliku_wejsciowego, nazwa_pliku_wynikowego)) {
        komunikat_bledu =
            "Plik wynikowy nie moze byc tym samym plikiem co zrodlowy.";
        return false;
    }

    array<uint64_t, 256> model{};
    uint64_t liczba_znakow = 0;
    if (!oblicz_model_Huffman(nazwa_pliku_wejsciowego, model, liczba_znakow,
                              komunikat_bledu)) {
        return false;
    }

    int korzen = -1;
    const auto drzewo = utworz_drzewo(model, korzen);
    vector<vector<unsigned char>> tablica_kodowania(drzewo.size());
    array<int, 256> indeksy_lisci{};
    indeksy_lisci.fill(-1);
    if (korzen >= 0) {
        // Przejscie DFS przypisuje lewym krawedziom 0, a prawym 1.
        vector<unsigned char> kod;
        utworz_kody(korzen, drzewo, tablica_kodowania, indeksy_lisci, kod);
    }
    // Raport diagnostyczny i naglowek archiwum korzystaja z tego samego licznika.
    if (!zapisz_pliki_pomocnicze(nazwa_pliku_wejsciowego, model, drzewo,
                                 tablica_kodowania, indeksy_lisci,
                                 liczba_znakow,
                                 komunikat_bledu)) {
        return false;
    }

    // Plik otwieramy ponownie, aby kodowac strumieniowo bez trzymania go w pamieci.
    ifstream plik_wejsciowy(nazwa_pliku_wejsciowego, ios::binary);
    if (!plik_wejsciowy) {
        komunikat_bledu = "Nie mozna ponownie otworzyc pliku wejsciowego.";
        return false;
    }
    ofstream plik_wynikowy(
        nazwa_pliku_wynikowego, ios::binary | ios::trunc);
    if (!plik_wynikowy) {
        komunikat_bledu = "Nie mozna utworzyc pliku wynikowego: " +
                          nazwa_pliku_wynikowego.string();
        return false;
    }

    const auto nazwa_oryginalna_utf8 =
        nazwa_pliku_wejsciowego.filename().u8string();
    if (nazwa_oryginalna_utf8.size() >
        numeric_limits<uint32_t>::max()) {
        komunikat_bledu = "Nazwa pliku jest zbyt dluga, aby zapisac ja w archiwum.";
        return false;
    }
    plik_wynikowy.write(ZNACZNIK_FORMATU, sizeof(ZNACZNIK_FORMATU));
    const auto liczba_lisci = static_cast<size_t>(
        count_if(model.begin(), model.end(),
                 [](uint64_t liczba) { return liczba != 0; }));
    // Pierwsze liczby i wiersze drzewa maja ten sam uklad co plik .tree.
    zapisz_drzewo(plik_wynikowy, drzewo, liczba_lisci, liczba_znakow);

    // Dlugosc nazwy pozwala zachowac nazwy zawierajace znaki spoza ASCII.
    plik_wynikowy << nazwa_oryginalna_utf8.size() << '\n';
    plik_wynikowy.write(nazwa_oryginalna_utf8.data(),
                        static_cast<streamsize>(
                            nazwa_oryginalna_utf8.size()));
    plik_wynikowy.put('\n');
    if (!plik_wynikowy) {
        komunikat_bledu = "Blad zapisu naglowka lub drzewa archiwum.";
        return false;
    }

    // Wlasciwe dane znowu zapisujemy jako bajty, wiec pozostaja skompresowane.
    unsigned char bajt_wynikowy = 0;
    unsigned int liczba_bitow_w_bajcie = 0;
    array<uint64_t, 256> sprawdzenie_modelu{};
    char znak = 0;
    // Kody kolejnych symboli skladamy od najstarszego bitu w kazdym bajcie.
    while (plik_wejsciowy.get(znak)) {
        const auto indeks_znaku = static_cast<unsigned char>(znak);
        ++sprawdzenie_modelu[indeks_znaku];
        const int indeks_liscia =
            indeksy_lisci[static_cast<size_t>(indeks_znaku)];
        if (indeks_liscia < 0) {
            komunikat_bledu = "Nie znaleziono symbolu w drzewie Huffmana.";
            return false;
        }
        for (const auto bit :
             tablica_kodowania[static_cast<size_t>(indeks_liscia)]) {
            bajt_wynikowy =
                static_cast<unsigned char>((bajt_wynikowy << 1) | bit);
            if (++liczba_bitow_w_bajcie == 8) {
                plik_wynikowy.put(static_cast<char>(bajt_wynikowy));
                bajt_wynikowy = 0;
                liczba_bitow_w_bajcie = 0;
            }
        }
    }

    if (!plik_wejsciowy.eof()) {
        komunikat_bledu = "Blad podczas odczytu pliku wejsciowego.";
        return false;
    }
    if (sprawdzenie_modelu != model) {
        komunikat_bledu = "Plik zrodlowy zmienil sie podczas kompresji.";
        return false;
    }
    if (liczba_bitow_w_bajcie != 0) {
        // Pozostale mlodsze bity ostatniego bajtu dopelniamy zerami.
        bajt_wynikowy = static_cast<unsigned char>(
            bajt_wynikowy << (8 - liczba_bitow_w_bajcie));
        plik_wynikowy.put(static_cast<char>(bajt_wynikowy));
    }
    if (!plik_wynikowy) {
        komunikat_bledu = "Blad podczas zapisu skompresowanego pliku.";
        return false;
    }
    plik_wynikowy.close();
    if (!plik_wynikowy) {
        komunikat_bledu = "Blad podczas zamykania skompresowanego pliku.";
        return false;
    }

    // Rozmiar koncowy odczytujemy po zamknieciu pliku, gdy zapis jest kompletny.
    error_code blad;
    const auto rozmiar_archiwum =
        filesystem::file_size(nazwa_pliku_wynikowego, blad);
    if (blad) {
        komunikat_bledu = "Nie mozna odczytac rozmiaru archiwum: " +
                          blad.message();
        return false;
    }
    wynik.liczba_bajtow_zrodlowych = liczba_znakow;
    wynik.liczba_bajtow_skompresowanych = rozmiar_archiwum;
    return true;
}
