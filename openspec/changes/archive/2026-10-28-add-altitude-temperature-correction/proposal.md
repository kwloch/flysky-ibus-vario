# Proposal

## Why

Obecna wysokość jest liczona wzorem atmosfery standardowej `h = 44330 * (1 - (P/P0)^(1/5.255))`, który zakłada temperaturę odniesienia 15 °C. W długim locie albo w dniu znacznie cieplejszym/zimniejszym niż ISA błąd wysokości rośnie proporcjonalnie do odchylenia temperatury: około 3,5 m na każde 1 °C różnicy od ISA przy 1000 m (czyli ~35 m przy 10 °C i ~70 m przy 20 °C). MS5611 mierzy temperaturę przy każdym odczycie, więc można ją wykorzystać bez dodatkowego sprzętu. **Czy warto?** Tak, jako konserwatywne ulepszenie dokładności wysokości dla długich lotów — ale nie jako zamiennik filtracji vario i nie przez bezpośrednie użycie bieżącej temperatury układu, która zawiera self-heating i dryf elektroniki.

**Jak robią to inni:**

- `msrc` (RP2040, MS5611, i-Bus) liczy wysokość z bieżącej zmierzonej temperatury: `(T + 273.15) * (1 - (P/P0)^(1/5.256)) / 0.0065`.
- `oXs_on_RP2040` nie koryguje wysokości temperaturą — używa stałej `44330 * (1 - (P/101325)^0.1903)`; temperaturę wykorzystuje tylko do kompensacji prędkości/DTE.
- `XCSoar` liczy wysokość w atmosferze ICAO/QNH i nie koryguje jej temperaturą; temperaturę i wilgotność używa do prognozy konwekcji, a gęstość powietrza do przeliczania prędkości.
- Meteorologia i radiosondy używają równania hipsometrycznego z **uśrednioną** temperaturą warstwy, a nie pojedynczym odczytem.

Wniosek: wśród hobbystycznych vario korekcja nie jest standardem, a tam gdzie jest (`msrc`), opiera się na bieżącej temperaturze czujnika. Dla tego projektu bezpieczniejsza jest temperatura odniesienia zmierzona w oknie startowym, bo nie sprzęga wysokości z dryfem self-heatingu w trakcie lotu.

## What Changes

- Dodać do `altitude-vario` korekcję temperaturową wysokości względnej opartą na **temperaturze odniesienia** z tego samego okna 20 pomiarów co ciśnienie odniesienia.
- Użyć wzoru z gradientem temperatury: `h = (T_ref_K / L) * (1 - (P/P_ref)^(R*L/g))`, gdzie `L = 0.0065 K/m`, `R = 287.05 J/(kg*K)`, `g = 9.80665 m/s²`; dla `T_ref = 288.15 K` wzór pokrywa się z obecnym wzorem standardowym w tolerancji 0,1 m.
- Przekazywać temperaturę z `MS5611::sample()` do `VarioEstimator` przy każdym prawidłowym odczycie; przy nieprawidłowej lub niedostępnej temperaturze w oknie referencyjnym użyć standardowych 15 °C (dotychczasowe zachowanie).
- Zachować bez zmian: referencję startową, brak ponownego zerowania w locie, filtr alpha-beta, jednostki cm/cm/s, częstotliwości 20 Hz/5 Hz oraz format logu `VARIO:`.
- Dodać opcjonalny przełącznik kompilacyjny do porównania wersji skorygowanej i standardowej na stanowisku (domyślnie korekcja włączona).
- Rozszerzyć testy hostowe o: uśrednianie temperatury odniesienia, zgodność ze wzorem standardowym przy 15 °C, przypadek ciepły/zimny, fallback przy braku temperatury oraz zachowanie filtra przy skorygowanej skali wysokości.
- Zaktualizować `README.md` (roadmapa, opis korekcji, ograniczenia związane z self-heatingiem).

Poza zakresem: wysokość bezwzględna/QNH, fuzja z GPS, kompensacja wilgotności (temperatura wirtualna), kalibracja offsetu self-heatingu, konfiguracja w locie i zmiany w emisji i-Bus.

## Capabilities

### New Capabilities

Brak.

### Modified Capabilities

- `altitude-vario`: rozszerzenie wymagania „Przeliczanie ciśnienia na wysokość” o temperaturę odniesienia oraz wymagania „Wysokość względna i punkt odniesienia” o uśrednianie temperatury w oknie referencyjnym i fallback do atmosfery standardowej.

## Impact

- `include/vario_math.h`: nowa sygnatura/aktualizacja `VarioEstimator::update()` przyjmująca temperaturę, przechowywanie `T_ref`, nowa funkcja przeliczania z temperaturą i zachowany wariant standardowy dla kompatybilności testów.
- `src/main.cpp`: przekazanie `sensor.sample().temperatureCentiC` do estymatora; bez zmian w logach i emisji i-Bus.
- `test/test_vario_math/test_main.cpp`: nowe i zaktualizowane testy wysokości oraz filtra.
- `README.md`: opis korekcji, jej ograniczeń i sposobu porównania.
- Brak nowych zależności runtime, zmian w `platformio.ini` i zmian w `ms5611-sensor`; odczyt i walidacja temperatury już istnieją.
- Ryzyko: temperatura matrycy MS5611 może różnić się od temperatury powietrza (self-heating, obudowa), dlatego korekcja poprawia głównie błąd wynikający z nie-ISA temperatury startu, a nie zastępuje dobrej wentylacji czujnika ani fuzji z GPS.
