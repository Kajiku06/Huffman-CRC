#ifndef HUFFMAN_HPP
#define HUFFMAN_HPP
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace std;

struct element_zrodla {
    int indeks_znaku;
    uint64_t ilosc_wystapien;
};

// Wezel drzewa: lisc ma indeks_znaku >= 0, a wezel wewnetrzny wskazuje dzieci.
struct galaz_drzewa {
    uint64_t ilosc_wystapien;
    int indeks_znaku;
    int potomek1;
    int potomek2;
};

struct wynik_kompresji {
    uint64_t liczba_bajtow_zrodlowych;
    uint64_t liczba_bajtow_skompresowanych;
};

// Zlicza wystapienia wszystkich 256 wartosci bajtu w pliku zrodlowym.
bool oblicz_model_Huffman(
    const filesystem::path& nazwa_pliku,
    array<uint64_t, 256>& model,
    uint64_t& liczba_znakow,
    string& komunikat_bledu);

// Buduje deterministyczne drzewo Huffmana z modelu czestosci.
vector<galaz_drzewa> utworz_drzewo(
    const array<uint64_t, 256>& model, int& korzen);

// Sprawdza tozsamosc plikow, uwzgledniajac istniejace i kanoniczne sciezki.
bool czy_te_same_pliki(const filesystem::path& plik1,
                       const filesystem::path& plik2);

// Koduje plik i zapisuje archiwum z naglowkiem oraz drzewem do dekodowania.
bool kompresja_Huffman(const filesystem::path& nazwa_pliku_wejsciowego,
                       const filesystem::path& nazwa_pliku_wynikowego,
                       wynik_kompresji& wynik,
                       string& komunikat_bledu);

// Odtwarza plik z archiwum; pusta sciezka wyniku oznacza nazwe z naglowka.
bool dekompresja_Huffmana(const filesystem::path& nazwa_pliku_skompresowanego,
                          filesystem::path& nazwa_pliku_dekompresji,
                          string& komunikat_bledu);

// Interaktywnie oblicza CRC32 i opcjonalnie zapisuje sume obok pliku.
void oblicz_CRC32_interaktywnie();

// Porownuje CRC32 pliku z suma zapisana w pliku .CRC32.
void sprawdz_CRC32();

#endif
