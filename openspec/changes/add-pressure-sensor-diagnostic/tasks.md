# Tasks

## 1. Kompensacja MS5607 i testy

- [x] 1.1 Dodać do `include/ms5611_math.h` funkcję `compensateMs5607(const Prom&, uint32_t d1, uint32_t d2)` z pierwszym i drugim rzędem z datasheetu MS5607 (`OFF = C2*2^17 + C4*dT/2^6`, `SENS = C1*2^16 + C3*dT/2^7`, `P = ((D1*SENS/2^21) - OFF)/2^15`, drugi rząd `61/16`, `2`, `15`, `8`), używającą `int64_t`; zweryfikować przez `pio test -e native`, że istniejące testy MS5611 nadal przechodzą i nagłówek kompiluje się bez Arduino.
- [x] 1.2 Dodać test pierwszeństwa na istniejącym wektorze datasheetu MS5611 (`PROM=0001 9CBF 903C 5B15 5AF2 82B8 6E98 123B`, `D1=9085466`, `D2=8569150`): `compensate()` ma dać `100009 Pa`/`2007`, a `compensateMs5607()` `200018 Pa`/`2007`; zweryfikować przez `pio test -e native`, że test przechodzi i że funkcja nie modyfikuje PROM.
- [x] 1.3 Jeśli uda się ustalić oficjalny wektor datasheetu MS5607 dla `TEMP < 2000`, dodać test drugiego rzędu; w przeciwnym razie odnotować w `design.md`/`README.md`, że testy obejmują pierwszy rząd, a drugi rząd jest implementowany z datasheetu. Zweryfikować przez `pio test -e native`, że wszystkie testy przechodzą.

## 2. Surowe D1/D2, linia DIAG i dokumentacja

- [x] 2.1 Dodać do `src/ms5611.h`/`src/ms5611.cpp` przechowywanie ostatnich `D1`/`D2`, gettery `rawD1()`/`rawD2()` i zerowanie w `begin()`; zapisywać surowe wartości po udanych odczytach ADC, przed kompensacją i kontrolą zakresu. Zweryfikować przez `pio run -e esp32-c6-zero`, że firmware się buduje, a istniejące API `Sample`/`hasSample()` pozostaje bez zmian.
- [x] 2.2 Dodać w `src/main.cpp` `logDiagnostic()` wywoływane w istniejącym bloku logu ciśnienia co ~1 s oraz flagę `PRESSURE_SENSOR_DIAGNOSTIC` (domyślnie 1); linia ma format `DIAG: t=<ms> D1=<raw> D2=<raw> P5611=<Pa> P5607=<Pa> T5611=<°C> T5607=<°C>`, a `VARIO:`, `MS5611:` i i-Bus mają pozostać na ścieżce MS5611. Zweryfikować przez `pio run -e esp32-c6-zero` oraz inspekcję formatu w kodzie, że linia jest zgodna ze specyfikacją i nie zmienia pozostałych logów.
- [x] 2.3 Zaktualizować `README.md`: opisać tymczasową diagnostykę, przełącznik `PRESSURE_SENSOR_DIAGNOSTIC`, format `DIAG:`, tabelę interpretacji (`P5607` vs `P5611` vs Aqara) i obowiązek wyłączenia/usunięcia diagnostyki po rozstrzygnięciu. Zweryfikować, że README zawiera te elementy, a opisy i-Bus/targetów pozostały bez zmian.

## 3. Weryfikacja i diagnostyka sprzętowa

- [x] 3.1 Uruchomić `pio test -e native`, `pio run -e esp32-c6-zero`, `pio run` bez `-e` oraz `openspec validate add-pressure-sensor-diagnostic --type change --strict`; zweryfikować, że testy i buildy przechodzą, domyślnym targetem pozostaje `esp32-c6-zero`, a walidacja jest bez błędów i ostrzeżeń.
- [x] 3.2 Zbudować wariant z `PLATFORMIO_BUILD_FLAGS="-DPRESSURE_SENSOR_DIAGNOSTIC=0"` (testy `native` i firmware) i zweryfikować, że kompiluje się bez linii diagnostycznej oraz że nie zmienia to ścieżki vario/i-Bus.
- [x] 3.3 Wgrać firmware, otworzyć `pio device monitor -e esp32-c6-zero` i porównać `P5611`/`P5607` z Aqarą (~97 969 Pa na ~300 m n.p.m.); zapisać wynik i wniosek: `P5607` zgodne z referencją → MS5607, `P5611` zgodne → MS5611, oba niezgodne → problem z PROM/odczytem. Jeśli sprzęt jest chwilowo niedostępny, pozostawić zadanie niezaznaczone i zapisać wynik jako oczekujący.
  - Wynik: MS5607 — `P5607=98 206 Pa` zgodne z Aqarą `97 969 Pa` (±0,24%), `P5611=49 103 Pa` (~50%), `T5611=T5607=25,41 °C`. Czujnik jest MS5607, a firmware używa wzoru MS5611.

## Workflow follow-up

- Po rozstrzygnięciu typu czujnika wyłączyć lub usunąć diagnostykę i zaplanować osobną zmianę: wsparcie MS5607 albo wymiana modułu na MS5611.
- Zarchiwizować tę zmianę po zapisaniu wyniku diagnostyki.
- Dopiero potem wrócić do osobnych problemów: modulacji buzera (szum Vspd/próg ±0,5 m/s) i dryfu 1 m / 10 min (warm-up vs pogoda).
