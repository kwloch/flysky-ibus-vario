# Proposal

## Why

Projekt potrafi już odczytać ciśnienie i temperaturę z MS5611, ale nie zamienia ich jeszcze na użyteczną telemetrię lotniczą. Bez wysokości względnej i prędkości wznoszenia nie da się zbudować wariometru, a surowy szum barometru sprawia, że vspd jest bezużyteczne. Potrzebny jest filtr, który przy realnym szumie MS5611 daje stabilną prędkość wznoszenia przy rozsądnym czasie odpowiedzi.

## What Changes

- Dodać czystą, testowalną warstwę `altitude-vario`: przeliczanie ciśnienia na wysokość względną, zerowanie referencji przy starcie i filtrowana prędkość wznoszenia.
- Wyznaczać referencję wysokości jako średnią z pierwszego okna 20 prawidłowych pomiarów ciśnienia (~1 s) po udanej inicjalizacji MS5611; wysokość 0 po wypełnieniu okna, bez ponownego zerowania w locie.
- Próbkować ciśnienie z częstotliwością 20 Hz (co 50 ms) i publikować/logować wysokość oraz vspd z częstotliwością 5 Hz (co 200 ms).
- Filtrować szum barometru filtrem stałej prędkości (alpha-beta) tak, aby skok prędkości wznoszenia osiągał ≥90% wartości w ciągu ~700 ms, a pojedyncze zakłócenia i szum nie generowały fałszywego wznoszenia. Stabilny vspd jest ważniejszy niż opóźnienie ~200 ms.
- Wyrażać wysokość w centymetrach i vspd w centymetrach na sekundę jako liczby całkowite ze znakiem — format gotowy pod przyszłe ramki i-Bus Alt (`0x83`) i Vspd (`0x09`).
- Zintegrować warstwę z `src/main.cpp`: 20 Hz odczytów, 5 Hz linia `VARIO: t=<ms> alt=<m> m vspd=<m/s> m/s` oraz zachowana dotychczasowa linia `MS5611: t=<ms> P=<Pa> Pa T=<°C> C` co ~1 s.
- Ignorować nieudane lub poza-zakresowe pomiary w estymatorze; po przerwie >250 ms wznowić filtrację bez fałszywego skoku prędkości.
- Dodać testy hostowe (Unity, środowisko `native`) dla referencji, wzoru barometrycznego, filtracji, opóźnienia, jednostek i obsługi przerw.
- Zaktualizować `README.md`: oznaczyć punkt roadmapy, opisać nowe jednostki, format logu i parametry filtra.
- Poza zakresem: emisja ramek i-Bus, ponowne zerowanie w locie, kompensacja temperaturowa poza MS5611, adaptacyjne strojenie filtra, target nRF52840.

## Capabilities

### New Capabilities

- `altitude-vario`: wysokość względna z uśrednioną referencją startową, przeliczanie ciśnienia na wysokość, filtrowana prędkość wznoszenia o czasie odpowiedzi ~0,5–1 s, częstotliwość próbkowania i publikacji oraz odporność na błędy odczytu.

### Modified Capabilities

Brak — zmiana dodaje nową warstwę i nie zmienia wymagań istniejących specyfikacji. Dotychczasowa linia `MS5611:` z ciśnieniem i temperaturą co ~1 s pozostaje zachowana.

## Impact

- Nowe pliki: `include/vario_math.h` (czysta matematyka i estymator), testy w `test/test_vario_math/`.
- Zmieniane pliki: `src/main.cpp` (20 Hz próbkowanie, 5 Hz log vario, obsługa przerw), `README.md` (roadmapa, log, jednostki).
- Brak nowych zależności runtime i zmian w `platformio.ini`; testy korzystają z istniejącego środowiska `native`.
- Zmiana bazuje na sterowniku MS5611 z `add-ms5611-i2c-read`, którego sprzętowy smoke test nadal oczekuje; finalna weryfikacja vario na sprzęcie wymaga ESP32-C6-Zero i GY-63.
- Przyszła emisja i-Bus może użyć wyjścia w cm i cm/s bez konwersji jednostek.
