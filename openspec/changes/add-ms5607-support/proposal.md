# Proposal

## Why

Diagnostyka potwierdziła, że moduł GY-63 ma **MS5607**, a nie MS5611: dla tych samych D1/D2 `P5607=98 206 Pa` zgadza się z Aqarą `97 969 Pa`, podczas gdy `P5611=49 103 Pa` jest o połowę mniejsze. Główna ścieżka firmware nadal używa wzoru MS5611, więc ciśnienie bezwzględne jest błędne, a kompensacja w niskich temperaturach może być gorsza. Tymczasowa diagnostyka `DIAG:` spełniła swoje zadanie i nie powinna zostać w produkcie.

## What Changes

- Dodać wybór wariantu czujnika przy kompilacji: `PRESSURE_SENSOR_MS5607=1` (domyślnie, potwierdzony moduł) albo `0` dla MS5611; wybrany wariant ma być używany przez całą ścieżkę vario/i-Bus.
- Dodać `compensateSelected()` wybierające `compensateMs5607()` albo `compensate()` i przełączyć `MS5611::read()` na tę funkcję.
- Dodać test regresyjny dla rzeczywistego wektora MS5607 (`PROM=089C 998C 8E4E 5E3A 585F 881E 6AC1 6663`, `D1=6476028`, `D2=9086870` → `98 206 Pa`/`25,41 °C`) oraz zachować test wektora MS5611 (`100009 Pa`/`20,07 °C`).
- Usunąć tymczasową diagnostykę: linię `DIAG:`, flagę `PRESSURE_SENSOR_DIAGNOSTIC` i nieużywane gettery surowych D1/D2.
- Zachować `MS5611:` jako historyczną nazwę linii logu i klasy sterownika; wartości mają być już liczone poprawnym wariantem. Vario/i-Bus pozostają bez zmian funkcjonalnych.
- Zaktualizować `README.md`: domyślny MS5607, przełączenie na MS5611 przez `PLATFORMIO_BUILD_FLAGS="-DPRESSURE_SENSOR_MS5607=0"`, usunięcie sekcji o diagnostyce i wyjaśnienie historycznej nazwy `MS5611:`.
- Poza zakresem: automatyczne wykrywanie typu, zmiana nazwy klasy/pliku/logu, deadband buzera, re-zero/dryf, QNH/GPS, oficjalny wektor drugiego rzędu MS5607 (jeśli niedostępny).

## Capabilities

### New Capabilities

Brak.

### Modified Capabilities

- `ms5611-sensor`: kompensacja ma używać wybranego wariantu MS5611/MS5607 (domyślnie MS5607), a wymaganie o tymczasowej diagnostyce `DIAG:` ma zostać usunięte.

## Impact

- `include/ms5611_math.h`: `PRESSURE_SENSOR_MS5607`, `compensateSelected()`; istniejące `compensate()` i `compensateMs5607()` pozostają.
- `src/ms5611.cpp` / `src/ms5611.h`: główna ścieżka używa `compensateSelected()`; usunięcie surowych D1/D2 i getterów diagnostycznych.
- `src/main.cpp`: usunięcie `PRESSURE_SENSOR_DIAGNOSTIC`, `logDiagnostic()` i linii `DIAG:`; log `MS5611:` i `VARIO:` bez zmian formatu.
- `test/test_ms5611_math/test_main.cpp`: wektor użytkownika MS5607, test wybranej kompensacji, zachowane testy MS5611.
- `README.md`: instrukcja wyboru wariantu i usunięcie opisów diagnostyki.
- Brak nowych zależności; `platformio.ini` nie wymaga zmiany, bo domyślny wariant jest w nagłówku. Domyślnie firmware ma używać MS5607, bo taki moduł został potwierdzony; MS5611 pozostaje dostępny przez flagę.
