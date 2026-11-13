#include "huffman.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

using namespace std;

namespace {
constexpr char ZNACZNIK_FORMATU[] = {'H', 'U', 'F', 'C', 'P', 'P', '5', '\n'};

bool odczytaj_liczbe_opisana(
    istream& plik, const string& etykieta, uint64_t& wartosc) {
    string linia;
    if (!getline(plik, linia) ||
        linia.compare(0, etykieta.size(), etykieta) != 0) {
        return false;
    }
    if (!linia.empty() && linia.back() == '\r') {
        linia.pop_back();
    }
    if (linia.compare(0, etykieta.size(), etykieta) != 0) {
        return false;
    }

    istringstream parser(linia.substr(etykieta.size()));
    parser >> wartosc;
    if (!parser) {
        return false;
    }
    parser >> ws;
    return parser.eof();
}

bool dekompresuj_format_v5(
    istream& archiwum, const filesystem::path& sciezka_archiwum,
    filesystem::path& nazwa_pliku_dekompresji, string& komunikat_bledu) {
    uint64_t liczba_znakow = 0;
    uint64_t liczba_wezlow_64 = 0;
    if (!odczytaj_liczbe_opisana(archiwum, "", liczba_znakow) ||
        !odczytaj_liczbe_opisana(archiwum, "", liczba_wezlow_64) ||
        liczba_wezlow_64 > 255) {
        komunikat_bledu = "Niepoprawny tekstowy naglowek archiwum.";
        return false;
    }

    const auto liczba_wezlow = static_cast<size_t>(liczba_wezlow_64);
    if ((liczba_znakow == 0) != (liczba_wezlow == 0)) {
        komunikat_bledu = "Dlugosc pliku i liczba wezlow nie sa zgodne.";
        return false;
    }

    vector<array<uint64_t, 3>> wpisy_drzewa;
    wpisy_drzewa.reserve(liczba_wezlow);
    for (size_t indeks = 0; indeks < liczba_wezlow; ++indeks) {
        string linia;
        if (!getline(archiwum, linia)) {
            komunikat_bledu = "Niekompletne drzewo w naglowku archiwum.";
            return false;
        }
        if (!linia.empty() && linia.back() == '\r') {
            linia.pop_back();
        }
        istringstream parser(linia);
        array<uint64_t, 3> wiersz{};
        if (!(parser >> wiersz[0] >> wiersz[1] >> wiersz[2])) {
            komunikat_bledu = "Niepoprawny wiersz drzewa w archiwum.";
            return false;
        }
        parser >> ws;
        if (!parser.eof()) {
            komunikat_bledu = "Niepoprawny wiersz drzewa w archiwum.";
            return false;
        }
        wpisy_drzewa.push_back(wiersz);
    }

    uint64_t dlugosc_nazwy_64 = 0;
    if (!odczytaj_liczbe_opisana(archiwum, "", dlugosc_nazwy_64) ||
        dlugosc_nazwy_64 == 0 || dlugosc_nazwy_64 > 65535) {
        komunikat_bledu = "Niepoprawna dlugosc nazwy pliku w archiwum.";
        return false;
    }
    string nazwa_utf8(static_cast<size_t>(dlugosc_nazwy_64), '\0');
    archiwum.read(
        nazwa_utf8.data(), static_cast<streamsize>(nazwa_utf8.size()));
    if (!archiwum) {
        komunikat_bledu = "Niekompletna nazwa pliku w archiwum.";
        return false;
    }
    int znak_konca_nazwy = archiwum.get();
    if (znak_konca_nazwy == '\r') {
        znak_konca_nazwy = archiwum.get();
    }
    if (znak_konca_nazwy != '\n') {
        komunikat_bledu = "Niepoprawny koniec naglowka archiwum.";
        return false;
    }

    const auto nazwa_oryginalna = filesystem::u8path(nazwa_utf8);
    if (nazwa_oryginalna.empty() ||
        nazwa_oryginalna.filename() != nazwa_oryginalna ||
        nazwa_oryginalna == "." || nazwa_oryginalna == "..") {
        komunikat_bledu = "Niebezpieczna nazwa pliku w archiwum.";
        return false;
    }

    vector<galaz_drzewa> drzewo(256 + liczba_wezlow);
    for (size_t symbol = 0; symbol < 256; ++symbol) {
        drzewo[symbol] = {0, -1, -1, -1};
    }
    vector<unsigned int> liczba_rodzicow(256 + liczba_wezlow, 0);
    array<bool, 256> znalezione_symbole{};
    size_t liczba_lisci = 0;
    int korzen = -1;

    if (liczba_wezlow == 1 && wpisy_drzewa[0][0] == 256 &&
        wpisy_drzewa[0][1] <= 255 && wpisy_drzewa[0][2] == 257) {
        // Dla jednego symbolu .tree opisuje sztucznego rodzica 256.
        const auto symbol = static_cast<size_t>(wpisy_drzewa[0][1]);
        drzewo[256] = {0, static_cast<int>(symbol), -1, -1};
        znalezione_symbole[symbol] = true;
        liczba_lisci = 1;
        korzen = 256;
    } else if (liczba_wezlow != 0) {
        for (size_t indeks = 0; indeks < liczba_wezlow; ++indeks) {
            const auto& wiersz = wpisy_drzewa[indeks];
            const uint64_t rodzic = 256 + indeks;
            if (wiersz[0] != rodzic ||
                wiersz[1] >= rodzic || wiersz[2] >= rodzic ||
                wiersz[1] == wiersz[2]) {
                komunikat_bledu =
                    "Niepoprawne indeksy wezlow w drzewie archiwum.";
                return false;
            }

            for (size_t kolumna = 1; kolumna <= 2; ++kolumna) {
                const auto dziecko = static_cast<size_t>(wiersz[kolumna]);
                if (dziecko < 256) {
                    if (znalezione_symbole[dziecko]) {
                        komunikat_bledu =
                            "Powtorzony symbol w drzewie archiwum.";
                        return false;
                    }
                    znalezione_symbole[dziecko] = true;
                    drzewo[dziecko] = {
                        0, static_cast<int>(dziecko), -1, -1};
                    ++liczba_lisci;
                }
                if (++liczba_rodzicow[dziecko] != 1) {
                    komunikat_bledu =
                        "Wezel ma wiecej niz jednego rodzica.";
                    return false;
                }
            }

            drzewo[static_cast<size_t>(rodzic)] = {
                0, -1, static_cast<int>(wiersz[1]),
                static_cast<int>(wiersz[2])};
        }

        korzen = static_cast<int>(256 + liczba_wezlow - 1);
        if (liczba_lisci != liczba_wezlow + 1 ||
            liczba_rodzicow[static_cast<size_t>(korzen)] != 0) {
            komunikat_bledu = "Drzewo Huffmana w archiwum jest niepelne.";
            return false;
        }
        for (size_t indeks = 0; indeks < 256 + liczba_wezlow - 1; ++indeks) {
            if (drzewo[indeks].indeks_znaku >= 0 &&
                !znalezione_symbole[indeks]) {
                continue;
            }
            if ((indeks < 256 && znalezione_symbole[indeks] &&
                 liczba_rodzicow[indeks] != 1) ||
                (indeks >= 256 && liczba_rodzicow[indeks] != 1)) {
                komunikat_bledu =
                    "Drzewo Huffmana zawiera niepodlaczony wezel.";
                return false;
            }
        }
    }

    auto sciezka_wynikowa = nazwa_pliku_dekompresji;
    if (sciezka_wynikowa.empty()) {
        sciezka_wynikowa =
            sciezka_archiwum.parent_path() / nazwa_oryginalna;
    }
    if (czy_te_same_pliki(sciezka_archiwum, sciezka_wynikowa)) {
        komunikat_bledu =
            "Plik wynikowy nie moze byc tym samym plikiem co archiwum.";
        return false;
    }
    ofstream wynik(sciezka_wynikowa, ios::binary | ios::trunc);
    if (!wynik) {
        komunikat_bledu = "Nie mozna utworzyc pliku wynikowego: " +
                          sciezka_wynikowa.string();
        return false;
    }

    if (liczba_lisci == 1) {
        const uint64_t liczba_bajtow_danych =
            liczba_znakow / 8 + (liczba_znakow % 8 != 0 ? 1 : 0);
        for (uint64_t indeks = 0; indeks < liczba_bajtow_danych; ++indeks) {
            const int bajt = archiwum.get();
            if (bajt == char_traits<char>::eof() || bajt != 0) {
                komunikat_bledu =
                    "Niepoprawne dane bitowe dla drzewa jednego symbolu.";
                return false;
            }
        }
        const char symbol = static_cast<char>(
            drzewo[static_cast<size_t>(korzen)].indeks_znaku);
        for (uint64_t indeks = 0; indeks < liczba_znakow; ++indeks) {
            wynik.put(symbol);
        }
    } else if (liczba_znakow != 0) {
        unsigned char bajt_wejsciowy = 0;
        unsigned int liczba_bitow_w_bajcie = 0;
        int indeks_galezi = korzen;
        for (uint64_t indeks = 0; indeks < liczba_znakow; ++indeks) {
            while (drzewo[static_cast<size_t>(indeks_galezi)]
                       .indeks_znaku < 0) {
                if (liczba_bitow_w_bajcie == 0) {
                    const int nastepny_bajt = archiwum.get();
                    if (nastepny_bajt == char_traits<char>::eof()) {
                        komunikat_bledu =
                            "Archiwum jest niekompletne: brakuje danych.";
                        return false;
                    }
                    bajt_wejsciowy =
                        static_cast<unsigned char>(nastepny_bajt);
                    liczba_bitow_w_bajcie = 8;
                }
                const unsigned int bit =
                    (bajt_wejsciowy >> (liczba_bitow_w_bajcie - 1)) & 1;
                --liczba_bitow_w_bajcie;
                const auto& galaz =
                    drzewo[static_cast<size_t>(indeks_galezi)];
                indeks_galezi =
                    bit == 0 ? galaz.potomek1 : galaz.potomek2;
                if (indeks_galezi < 0 ||
                    static_cast<size_t>(indeks_galezi) >= drzewo.size()) {
                    komunikat_bledu =
                        "Dane wskazuja na niepoprawne drzewo Huffmana.";
                    return false;
                }
            }
            wynik.put(static_cast<char>(
                drzewo[static_cast<size_t>(indeks_galezi)].indeks_znaku));
            indeks_galezi = korzen;
        }

        if (liczba_bitow_w_bajcie != 0) {
            const unsigned int maska_dopelnienia =
                (1u << liczba_bitow_w_bajcie) - 1u;
            if ((bajt_wejsciowy & maska_dopelnienia) != 0) {
                komunikat_bledu =
                    "Niepoprawne bity dopelniajace w archiwum.";
                return false;
            }
        }
    }

    if (archiwum.peek() != char_traits<char>::eof()) {
        komunikat_bledu = "Archiwum zawiera nadmiarowe dane.";
        return false;
    }
    if (archiwum.bad() || !wynik) {
        komunikat_bledu =
            "Blad podczas odczytu archiwum lub zapisu pliku wynikowego.";
        return false;
    }

    nazwa_pliku_dekompresji = sciezka_wynikowa;
    return true;
}
}

bool dekompresja_Huffmana(
    const filesystem::path& nazwa_pliku_skompresowanego,
    filesystem::path& nazwa_pliku_dekompresji,
    string& komunikat_bledu) {
    ifstream plik_skompresowany(
        nazwa_pliku_skompresowanego, ios::binary);
    if (!plik_skompresowany) {
        komunikat_bledu = "Nie mozna otworzyc archiwum: " +
                          nazwa_pliku_skompresowanego.string();
        return false;
    }

    char znacznik[sizeof(ZNACZNIK_FORMATU)]{};
    plik_skompresowany.read(znacznik, sizeof(znacznik));
    if (!plik_skompresowany ||
        !equal(begin(ZNACZNIK_FORMATU), end(ZNACZNIK_FORMATU),
               begin(znacznik))) {
        komunikat_bledu =
            "Niepoprawny format archiwum Huffmana (wymagana wersja 5).";
        return false;
    }

    return dekompresuj_format_v5(
        plik_skompresowany, nazwa_pliku_skompresowanego,
        nazwa_pliku_dekompresji, komunikat_bledu);
}
