# Proposal

## Why

Czujnik ciśnienia raportuje `49 103 Pa` na wysokości ~300 m n.p.m., podczas gdy referencyjny Aqara pokazuje `979,69 hPa` (`97 969 Pa`) — praktycznie dokładnie połowę. To nie szum ani dryf: wygląda na **MS5607 z firmware'em liczącym wzorem MS5611**. Oba układy mają ten sam wzór temperatury, ale inne `OFF`/`SENS`, więc przy `T > 20 °C` ciśnienie wychodzi ~2x mniejsze. Bez mikroskopu nie da się tego potwierdzić oznaczeniem układu, a PROM (CRC `0x03`, `089C 998C 8E4E 5E3A 585F 881E 6AC1 6663`) jest poprawny i nie identyfikuje typu.

Wysokość względna nadal działa, bo używa stosunku `P/P_ref`; oba ciśnienia są o połowę mniejsze, więc stosunek jest zachowany. Problem pozostaje jednak poważny: ciśnienie bezwzględne jest bezużyteczne, a przy niskich temperaturach MS5607 ma inny drugi rząd korekcji niż MS5611. Dlatego potrzebna jest **tymczasowa diagnostyka programowa**, która policzy oba warianty z tych samych surowych D1/D2 i pozwoli porównać je z Aqarą. To osobna zmiana od docelowego wsparcia MS5607 i od strojenia filtra/buzera.

## What Changes

- Udostępnić surowe `D1`/`D2` z `MS5611::read()` (gettery i zapis ostatnich wartości), bez zmiany istniejącego `Sample`.
- Dodać czystą funkcję kompensacji **MS5607** (pierwszy i drugi rząd) obok istniejącej kompensacji MS5611, zwracającą `Sample` w Pa i °C.
- Dodać tymczasową linię diagnostyczną logowaną co ~1 s, np. `DIAG: t=<ms> D1=<raw> D2=<raw> P5611=<Pa> P5607=<Pa> T5611=<C> T5607=<C>`, za przełącznikiem kompilacyjnym `PRESSURE_SENSOR_DIAGNOSTIC` (domyślnie włączonym w tej zmianie).
- **Nie zmieniać** ścieżki vario/i-Bus: `VARIO:`, `MS5611:` oraz wysokość/Vspd nadal liczone są istniejącą kompensacją MS5611. Diagnostyka ma tylko obserwować, nie przełączać sensora.
- Dodać testy hostowe kompensacji MS5607 z wektorem z datasheetu oraz test, że diagnostyka nie zmienia wyniku MS5611.
- Zaktualizować `README.md`: jak włączyć/wyłączyć diagnostykę, jak czytać `DIAG:` i jak porównać `P5611`/`P5607` z referencją (Aqara).
- Poza zakresem: finalne wsparcie MS5607, automatyczne wykrywanie typu, zmiana kompensacji używanej przez vario, wymiana sprzętu, kompensacja dryfu i deadband buzera.

## Capabilities

### New Capabilities

Brak.

### Modified Capabilities

- `ms5611-sensor`: nowe wymaganie diagnostyczne — logowanie surowych D1/D2 oraz niezależnych wyników kompensacji MS5611 i MS5607, bez wpływu na dotychczasowy pomiar używany przez vario.

## Impact

- `include/ms5611_math.h`: nowa funkcja kompensacji MS5607 i stałe pomocnicze; istniejąca `compensate()` pozostaje bez zmian.
- `src/ms5611.h` / `src/ms5611.cpp`: przechowywanie i udostępnianie ostatnich `D1`/`D2`.
- `src/main.cpp`: warunkowa linia `DIAG:` co ~1 s; brak zmian w logach `MS5611:`/`VARIO:` i w emisji i-Bus.
- `test/test_ms5611_math/test_main.cpp`: wektor MS5607 i testy pierwszeństwa kompensacji.
- `README.md`: opis tymczasowej diagnostyki i interpretacji wyników.
- Brak nowych zależności runtime i zmian w `platformio.ini` poza ewentualnym `build_flags` dla diagnostyki.
- Diagnostyka jest tymczasowa: po potwierdzeniu typu czujnika należy ją usunąć lub domyślnie wyłączyć, a właściwą poprawkę (MS5607 albo wymiana modułu) zaplanować jako osobną zmianę.
