#include "huffman.hpp"
#include <filesystem>
#include <iostream>
#include <string>

using namespace std;

namespace {
filesystem::path pobierz_nazwe_pliku(const string& komunikat) {
    cout << komunikat;
    string nazwa_pliku;
    getline(cin, nazwa_pliku);
    return filesystem::path(nazwa_pliku);
}

void kompresuj_plik() {
    // Pobiera nazwe, wybiera rozszerzenie .huf i pokazuje wynik operacji.
    const auto nazwa_pliku_wejsciowego =
        pobierz_nazwe_pliku("Podaj nazwe pliku do kompresji: ");
    if (nazwa_pliku_wejsciowego.empty()) {
        cout << "Nie podano pliku.\n";
        return;
    }
    auto nazwa_pliku_wynikowego = nazwa_pliku_wejsciowego;
    nazwa_pliku_wynikowego.replace_extension(".huf");

    wynik_kompresji wynik{};
    string komunikat_bledu;
    if (!kompresja_Huffman(nazwa_pliku_wejsciowego,
                           nazwa_pliku_wynikowego, wynik, komunikat_bledu)) {
        cerr << "Blad kompresji: " << komunikat_bledu << "\n";
        return;
    }
    cout << "Skompresowano plik do: "
              << nazwa_pliku_wynikowego.string() << "\n"
              << "Liczba symboli (bajtow) pliku zrodlowego: "
              << wynik.liczba_bajtow_zrodlowych << "\n"
              << "Liczba bajtow po kompresji: "
              << wynik.liczba_bajtow_skompresowanych << "\n";
    if (wynik.liczba_bajtow_zrodlowych != 0) {
        const double stopien_kompresji =
            (1.0 - static_cast<double>(wynik.liczba_bajtow_skompresowanych) /
                       static_cast<double>(wynik.liczba_bajtow_zrodlowych)) *
            100.0;
        cout << "Zmiana rozmiaru: " << fixed << setprecision(1)
                  << stopien_kompresji << "%\n";
    }
    cout << "Pliki pomocnicze (model, sort, tree, modmodel, table i modtab) "
              << "zapisano w katalogu: "
              << (nazwa_pliku_wejsciowego.parent_path() / "tmp").string()
              << "\n";
}

void dekompresuj_plik() {
    // Pusta nazwa wyniku pozwala dekoderowi uzyc oryginalnej nazwy z archiwum.
    const auto nazwa_pliku_skompresowanego =
        pobierz_nazwe_pliku("Podaj nazwe archiwum Huffmana: ");
    if (nazwa_pliku_skompresowanego.empty()) {
        cout << "Nie podano pliku.\n";
        return;
    }
    auto nazwa_pliku_dekompresji =
        pobierz_nazwe_pliku("Plik wynikowy (Enter = nazwa oryginalna): ");

    string komunikat_bledu;
    if (!dekompresja_Huffmana(nazwa_pliku_skompresowanego,
                              nazwa_pliku_dekompresji, komunikat_bledu)) {
        cerr << "Blad dekompresji: " << komunikat_bledu << "\n";
        return;
    }
    cout << "Odtworzono plik: "
              << nazwa_pliku_dekompresji.string() << "\n";
}

}

int main() {
    cout << "Teoria Informacji i Kodowania - laboratoria\n"
                 "Kodowanie Huffmana i CRC32\n";

    // Pętla menu uruchamia wybrana operacje az do wyjscia lub EOF.
    while (true) {
        cout << "\n1. Kompresja metoda Huffmana\n"
                     "2. Dekompresja metoda Huffmana\n"
                     "3. Obliczenie CRC32 dla pliku\n"
                     "4. Sprawdzenie CRC32\n"
                     "Inny wybor - wyjscie\n"
                     "Wybor: ";
        string wybor;
        if (!getline(cin, wybor) || wybor.empty()) {
            break;
        }
        if (wybor == "1") {
            kompresuj_plik();
        } else if (wybor == "2") {
            dekompresuj_plik();
        } else if (wybor == "3") {
            oblicz_CRC32_interaktywnie();
        } else if (wybor == "4") {
            sprawdz_CRC32();
        } else {
            break;
        }
    }
    return 0;
}
