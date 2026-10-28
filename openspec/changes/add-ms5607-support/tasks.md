# Tasks

## 1. Wybór wariantu i testy kompensacji

- [x] 1.1 Dodać w `include/ms5611_math.h` `PRESSURE_SENSOR_MS5607` (domyślnie 1), `kPressureSensorMs5607` i `compensateSelected()` wybierające `compensateMs5607()` albo `compensate()`; zweryfikować przez `pio test -e native` oraz `PLATFORMIO_BUILD_FLAGS="-DPRESSURE_SENSOR_MS5607=0" pio test -e native`, że nagłówek kompiluje się w obu wariantach i istniejące testy przechodzą.
- [x] 1.2 Dodać test rzeczywistego wektora MS5607 (`PROM=089C 998C 8E4E 5E3A 585F 881E 6AC1 6663`, `D1=6476028`, `D2=9086870` → `98 206 Pa`/`2541`) oraz test `compensateSelected()` zależny od `#if PRESSURE_SENSOR_MS5607` (wariant 1 → wektor MS5607, wariant 0 → wektor MS5611); zweryfikować przez `pio test -e native` i wariant z `-DPRESSURE_SENSOR_MS5607=0`, że oba zestawy testów przechodzą.
- [x] 1.3 Zachować testy `compensate()` MS5611 (`100009 Pa`/`2007`) i różnicy 2x; zweryfikować przez `pio test -e native`, że wszystkie testy MS5611/MS5607 przechodzą.

## 2. Główna ścieżka, usunięcie diagnostyki i dokumentacja

- [x] 2.1 Usunąć z `src/main.cpp` flagę `PRESSURE_SENSOR_DIAGNOSTIC`, `logDiagnostic()` i wywołanie `DIAG:`; zweryfikować przez `pio run -e esp32-c6-zero`, że firmware się buduje, a `VARIO:`/`MS5611:` i emisja i-Bus pozostają bez zmian.
- [x] 2.2 Przełączyć `src/ms5611.cpp` na `compensateSelected()` i usunąć z `src/ms5611.h/.cpp` `d1_`, `d2_`, `rawD1()`, `rawD2()` oraz ich zerowanie; zweryfikować przez `pio run -e esp32-c6-zero`, że firmware się buduje i główna ścieżka używa wybranego wariantu.
- [x] 2.3 Zaktualizować `README.md`: domyślny MS5607, przełączenie na MS5611 przez `PLATFORMIO_BUILD_FLAGS="-DPRESSURE_SENSOR_MS5607=0"`, usunięcie sekcji o `DIAG:`/`PRESSURE_SENSOR_DIAGNOSTIC`, aktualizacja opisu GY-63 i historycznej nazwy `MS5611:`; zweryfikować, że README zawiera te elementy, a sekcje i-Bus/targetów pozostały bez zmian.

## 3. Weryfikacja

- [x] 3.1 Uruchomić `pio test -e native`, `PLATFORMIO_BUILD_FLAGS="-DPRESSURE_SENSOR_MS5607=0" pio test -e native`, `pio run -e esp32-c6-zero`, `PLATFORMIO_BUILD_FLAGS="-DPRESSURE_SENSOR_MS5607=0" pio run -e esp32-c6-zero`, `pio run` bez `-e` oraz `openspec validate add-ms5607-support --type change --strict`; zweryfikować, że oba warianty się budują, testy przechodzą, domyślnym targetem pozostaje `esp32-c6-zero`, a walidacja jest bez błędów i ostrzeżeń.
- [x] 3.2 Wgrać domyślny firmware (MS5607), otworzyć `pio device monitor -e esp32-c6-zero` i sprawdzić: ciśnienie `~98 kPa` zgodne z Aqarą (~97 969 Pa), brak linii `DIAG:`, `VARIO:`/Alt/Vspd działają, a wysokość względna startuje od 0. Jeśli sprzęt jest chwilowo niedostępny, pozostawić zadanie niezaznaczone i zapisać wynik jako oczekujący.
  - Wynik: OK — `MS5611: t=94170 P=98201 Pa T=26.19 C`; Aqara ~97 969 Pa (różnica ~0,24%), brak `DIAG:`, ścieżka VARIO/Alt/Vspd bez zmian.

## Workflow follow-up

- Zarchiwizować zmianę po weryfikacji sprzętowej; jeśli drugi rząd MS5607 nie ma oficjalnego wektora, odnotować to w README/design i dodać test później.
- Po naprawie typu czujnika osobno zmierzyć buzzer (szum Vspd/próg ±0,5 m/s) i dryf 1 m / 10 min; nie łączyć ich z tą zmianą.
