#include "huffman.hpp"
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

namespace {
// Generuje tablice przyspieszajaca obliczenia CRC-32 (wariant IEEE).
constexpr array<uint32_t, 256> utworz_tabele_CRC32() {
    array<uint32_t, 256> crc32_tab{};
    for (uint32_t indeks = 0; indeks < crc32_tab.size(); ++indeks) {
        uint32_t wartosc = indeks;
        for (int bit = 0; bit < 8; ++bit) {
            wartosc = (wartosc >> 1) ^
                      ((wartosc & 1u) != 0 ? 0xedb88320u : 0u);
        }
        crc32_tab[indeks] = wartosc;
    }
    return crc32_tab;
}

constexpr auto crc32_tab = utworz_tabele_CRC32();

filesystem::path pobierz_nazwe_pliku(const string& komunikat) {
    cout << komunikat;
    string nazwa_pliku;
    getline(cin, nazwa_pliku);
    return filesystem::path(nazwa_pliku);
}

filesystem::path utworz_nazwe_pliku_CRC32(
    const filesystem::path& nazwa_pliku) {
    // Plik sumy ma te sama nazwe bazowa i rozszerzenie .CRC32.
    auto nazwa_pliku_crc = nazwa_pliku;
    nazwa_pliku_crc.replace_extension(".CRC32");
    return nazwa_pliku_crc;
}

bool oblicz_CRC32(const filesystem::path& nazwa_pliku,
                  uint32_t& crc32,
                  string& komunikat_bledu) {
    // Przetwarza plik blokami, aktualizujac sume dla kazdego bajtu.
    ifstream plik(nazwa_pliku, ios::binary);
    if (!plik) {
        komunikat_bledu = "Nie mozna otworzyc pliku z danymi: " +
                          nazwa_pliku.string();
        return false;
    }

    crc32 = 0xffffffffu;
    array<unsigned char, 1024> bufor{};
    while (plik) {
        plik.read(reinterpret_cast<char*>(bufor.data()),
                  static_cast<streamsize>(bufor.size()));
        const auto liczba_odczytanych_bajtow = plik.gcount();
        for (streamsize indeks = 0;
             indeks < liczba_odczytanych_bajtow; ++indeks) {
            const auto indeks_tabeli =
                (crc32 ^ bufor[static_cast<size_t>(indeks)]) & 0xffu;
            crc32 = (crc32 >> 8) ^ crc32_tab[indeks_tabeli];
        }
    }
    if (!plik.eof()) {
        komunikat_bledu = "Blad podczas odczytu pliku: " +
                          nazwa_pliku.string();
        return false;
    }
    crc32 ^= 0xffffffffu;
    return true;
}

bool odczytaj_zapisane_CRC32(const filesystem::path& nazwa_pliku_crc,
                             uint32_t& zapisane_crc32,
                             string& komunikat_bledu) {
    // Wczytuje dokladnie jedna 8-znakowa wartosc szesnastkowa.
    ifstream plik_crc(nazwa_pliku_crc);
    if (!plik_crc) {
        komunikat_bledu = "Nie znaleziono pliku CRC32: " +
                          nazwa_pliku_crc.string();
        return false;
    }

    string tekst_crc;
    string nadmiarowy_tekst;
    if (!(plik_crc >> tekst_crc) || (plik_crc >> nadmiarowy_tekst) ||
        tekst_crc.size() != 8) {
        komunikat_bledu = "Plik CRC32 ma niepoprawny format.";
        return false;
    }

    istringstream strumien_crc(tekst_crc);
    strumien_crc >> hex >> zapisane_crc32;
    if (!strumien_crc || !strumien_crc.eof()) {
        komunikat_bledu = "Plik CRC32 nie zawiera poprawnej wartosci.";
        return false;
    }
    return true;
}
}

void oblicz_CRC32_interaktywnie() {
    // Oblicza sume i pyta, czy zapisac ja do pliku obok danych.
    const auto nazwa_pliku = pobierz_nazwe_pliku(
        "Podaj nazwe pliku do obliczenia CRC32 (np. plik.txt): ");
    if (nazwa_pliku.empty()) {
        cout << "Nie podano pliku.\n";
        return;
    }

    uint32_t crc32 = 0;
    string komunikat_bledu;
    if (!oblicz_CRC32(nazwa_pliku, crc32, komunikat_bledu)) {
        cerr << komunikat_bledu << "\n";
        return;
    }

    cout << "CRC32 = " << uppercase << hex
         << setw(8) << setfill('0') << crc32 << dec
         << setfill(' ') << "\n";
    cout << "Czy chcesz zapisac CRC32 do pliku? (t/n): ";
    string odpowiedz;
    getline(cin, odpowiedz);
    if (odpowiedz != "t" && odpowiedz != "T" &&
        odpowiedz != "tak" && odpowiedz != "TAK") {
        cout << "Nie zapisano pliku CRC32.\n";
        return;
    }

    const auto nazwa_pliku_crc = utworz_nazwe_pliku_CRC32(nazwa_pliku);
    ofstream plik_crc(nazwa_pliku_crc, ios::trunc);
    if (!plik_crc) {
        cerr << "Nie mozna utworzyc pliku CRC32: "
             << nazwa_pliku_crc.string() << "\n";
        return;
    }
    plik_crc << uppercase << hex << setw(8)
             << setfill('0') << crc32 << '\n';
    if (!plik_crc) {
        cerr << "Blad zapisu pliku CRC32.\n";
        return;
    }
    cout << "Zapisano CRC32 w pliku: "
         << nazwa_pliku_crc.string() << "\n";
}

void sprawdz_CRC32() {
    // Liczy sume ponownie i porownuje ja z wczesniej zapisana wartoscia.
    const auto nazwa_pliku = pobierz_nazwe_pliku(
        "Podaj nazwe pliku do sprawdzenia CRC32 (np. plik.txt): ");
    if (nazwa_pliku.empty()) {
        cout << "Nie podano pliku.\n";
        return;
    }

    const auto nazwa_pliku_crc = utworz_nazwe_pliku_CRC32(nazwa_pliku);
    uint32_t zapisane_crc32 = 0;
    string komunikat_bledu;
    if (!odczytaj_zapisane_CRC32(nazwa_pliku_crc, zapisane_crc32,
                                 komunikat_bledu)) {
        cerr << komunikat_bledu << "\n";
        return;
    }

    uint32_t obliczone_crc32 = 0;
    if (!oblicz_CRC32(nazwa_pliku, obliczone_crc32, komunikat_bledu)) {
        cerr << komunikat_bledu << "\n";
        return;
    }

    if (zapisane_crc32 == obliczone_crc32) {
        cout << "Plik nie ulegl zmianie. CRC32 = "
             << uppercase << hex << setw(8)
             << setfill('0') << obliczone_crc32 << dec
             << setfill(' ') << "\n";
    } else {
        cout << "Plik ulegl modyfikacji.\n"
             << "CRC32 zapisane: " << uppercase << hex
             << setw(8) << setfill('0') << zapisane_crc32
             << "\nCRC32 obliczone: " << setw(8)
             << obliczone_crc32 << dec << setfill(' ')
             << "\n";
    }
}
