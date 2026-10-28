# Design

## Context

Motywację i wyniki diagnostyki zawiera `proposal.md`. Wymagania behawioralne opisuje delta `specs/ms5611-sensor/spec.md`.

Stan kodu po zmianie diagnostycznej:

- `include/ms5611_math.h` ma `compensate()` (MS5611) i `compensateMs5607()`; testy pokrywają wektor MS5611 (`100009 Pa`/`20,07 °C`) i różnicę 2x.
- `src/ms5611.cpp` nadal używa `compensate()` w głównej ścieżce, więc na module MS5607 raportuje ~49 kPa zamiast ~98 kPa.
- `src/main.cpp` ma tymczasową linię `DIAG:` i flagę `PRESSURE_SENSOR_DIAGNOSTIC`; `src/ms5611.h` ma gettery `rawD1()`/`rawD2()` używane tylko przez diagnostykę.
- Diagnostyka potwierdziła MS5607: dla `PROM=089C 998C 8E4E 5E3A 585F 881E 6AC1 6663`, `D1=6476028`, `D2=9086870` wynik MS5607 to `98 206 Pa`/`25,41 °C`, zgodny z Aqarą `97 969 Pa`.
- `ms5611-sensor` to historyczna nazwa capability; klasa `MS5611` i log `MS5611:` są historycznymi nazwami. Zmiana nie wymaga ich zmiany, żeby nie psuć logów i dokumentacji.

## Goals / Non-Goals

**Goals:**

- Używać poprawnej kompensacji MS5607 w domyślnym buildzie, z możliwością zbudowania wariantu MS5611.
- Usunąć tymczasową diagnostykę i jej wymaganie ze specyfikacji.
- Zachować istniejące API `Sample`, vario oraz emisję i-Bus.
- Dodać regresję na rzeczywistym wektorze użytkownika.

**Non-Goals:**

- Automatyczne wykrywanie typu czujnika w runtime.
- Zmiana nazwy klasy, plików, capability lub prefiksu logu `MS5611:`.
- Deadband buzera, re-zero, kompensacja dryfu, QNH/GPS.
- Oficjalny wektor drugiego rzędu MS5607, jeśli nie jest dostępny.

## Decisions

### Wybór wariantu przy kompilacji, domyślnie MS5607

W `include/ms5611_math.h` dodać:

```cpp
#ifndef PRESSURE_SENSOR_MS5607
#define PRESSURE_SENSOR_MS5607 1
#endif
constexpr bool kPressureSensorMs5607 = PRESSURE_SENSOR_MS5607 != 0;

inline Sample compensateSelected(const Prom& prom, uint32_t d1, uint32_t d2) {
  return kPressureSensorMs5607 ? compensateMs5607(prom, d1, d2)
                               : compensate(prom, d1, d2);
}
```

`MS5611::read()` wywołuje `compensateSelected()`. Domyślnie 1, bo moduł użytkownika został potwierdzony jako MS5607; MS5611 wybiera się przez `PLATFORMIO_BUILD_FLAGS="-DPRESSURE_SENSOR_MS5607=0"`.

Alternatywa: automatyczne wykrywanie po PROM lub przez porównanie ciśnienia z zakresem. Odrzucona — PROM nie identyfikuje typu, a oba warianty mieszczą się w zakresie; diagnostyka pokazała, że potrzebna jest jawna decyzja.
Alternatywa: zawsze MS5607. Odrzucona — projekt ma działać także z MS5611; flaga pozwala zachować zgodność.
Alternatywa: domyślnie MS5611 i flaga dla MS5607. Odrzucona — potwierdzony sprzęt to MS5607, więc domyślny build musi być poprawny dla tego modułu.

### Usunięcie diagnostyki, zachowanie funkcji kompensacji

Usunąć z `src/main.cpp` flagę `PRESSURE_SENSOR_DIAGNOSTIC`, `logDiagnostic()` i wywołanie `DIAG:`; usunąć z `src/ms5611.h/.cpp` `d1_`, `d2_`, `rawD1()`, `rawD2()` i ich zerowanie. `compensateMs5607()` i `compensate()` zostają, bo pierwsza jest teraz główną ścieżką (lub alternatywną), a obie są testowane.

Alternatywa: zostawić diagnostykę za flagą domyślnie 0. Odrzucona — typ jest potwierdzony, a dodatkowy kod i wymaganie specyfikacji nie są potrzebne; w razie potrzeby można wrócić do archiwalnej zmiany.
Alternatywa: zostawić surowe D1/D2 na przyszłość. Odrzucona — bez diagnostyki są martwym API; można je dodać ponownie, gdy będzie potrzebne.

### Historyczne nazwy bez zmian

Pozostawić klasę `MS5611`, pliki `ms5611.*`, capability `ms5611-sensor` i prefiks logu `MS5611:`. Zmiana nazw to osobny, większy refaktor, a użytkownik ma już logi i dokumentację oparte na tych nazwach. README wyjaśni, że nazwa obejmuje rodzinę MS5611/MS5607.

Alternatywa: zmienić prefiks na `BARO:` i nazwę klasy na `PressureSensor`. Odrzucona w tej zmianie — brak potrzeby funkcjonalnej, ryzyko rozjechania logów i testów.

### Domyślny wariant w nagłówku, bez duplikatu w `platformio.ini`

Domyślne `PRESSURE_SENSOR_MS5607=1` jest w `ms5611_math.h`; `platformio.ini` nie dostaje drugiego `-D`, żeby uniknąć ostrzeżeń przy nadpisywaniu przez `PLATFORMIO_BUILD_FLAGS`. README dokumentuje oba warianty.

Alternatywa: `build_flags` w `platformio.ini`. Odrzucona — duplikat macro przy przełączaniu wariantu powoduje ostrzeżenia; nagłówek jest wystarczający.

### Testy

Dodać do `test/test_ms5611_math/test_main.cpp`:

- test rzeczywistego wektora MS5607: `PROM=089C 998C 8E4E 5E3A 585F 881E 6AC1 6663`, `D1=6476028`, `D2=9086870` → `98 206 Pa`/`2541`;
- test `compensateSelected()` zależny od build flagi (`#if PRESSURE_SENSOR_MS5607`): dla 1 używa wektora MS5607, dla 0 wektora MS5611;
- zachować istniejące testy MS5611 i różnicy 2x.

Drugi rząd MS5607 pozostaje bez oficjalnego wektora; implementacja jest z datasheetu, a testy pokrywają pierwszy rząd. Odnotować to w README. Testy uruchomić dla obu wartości flagi.

Alternatywa: testować tylko funkcje, bez `compensateSelected()`. Odrzucona — nie sprawdziłoby wyboru używanego przez sterownik.

### README

Zaktualizować opis projektu i sterownika na MS5611/MS5607. Usunąć sekcję o tymczasowej diagnostyce. Dodać sekcję o wyborze wariantu:

```bash
# domyślnie MS5607
pio run -e esp32-c6-zero
# wariant MS5611
PLATFORMIO_BUILD_FLAGS="-DPRESSURE_SENSOR_MS5607=0" pio run -e esp32-c6-zero
```

Wyjaśnić, że `MS5611:` w logu to historyczna nazwa rodziny, a wartości są już z wybranego wariantu. Zaktualizować pułapkę „GY-63 = MS5611” na „GY-63 bywa MS5611 albo MS5607”. Nie zmieniać sekcji i-Bus ani targetów.

## Risks / Trade-offs

- **Domyślny MS5607 może zepsuć odczyt na module MS5611** → README i flaga 0; build/test obu wariantów.
- **Drugi rząd MS5607 bez wektora datasheetu** → formuła z datasheetu; przy 25 °C nie ma to wpływu, ale w zimnie wymaga ostrożności. Mitygacja: dodać wektor, gdy będzie dostępny; nie deklarować pełnej walidacji drugiego rzędu.
- **Historyczne nazwy `MS5611`/`MS5611:` mylą** → README wyjaśnia rodzinę MS5611/MS5607; zmiana nazw odłożona.
- **Usunięcie diagnostyki utrudnia porównanie w przyszłości** → archiwalna zmiana `add-pressure-sensor-diagnostic` pozostaje w historii; można ją przywrócić.
- **Relative altitude/buzzer/drift bez zmian** → stosunek `P/P_ref` jest zachowany; naprawa typu poprawia ciśnienie bezwzględne i zimno, ale nie modulację buzera ani dryf. To osobne zmiany.
- **Brak `platformio.ini` flagi** → domyślna wartość w nagłówku; mniej widoczna, ale README ją opisuje.

## Migration Plan

1. Dodać `PRESSURE_SENSOR_MS5607`, `compensateSelected()` i testy (MS5607 user vector + selected path).
2. Przełączyć `MS5611::read()` na `compensateSelected()`.
3. Usunąć surowe D1/D2 i diagnostykę z `ms5611.*` oraz `main.cpp`.
4. Zaktualizować README.
5. Uruchomić testy/buildy dla `PRESSURE_SENSOR_MS5607=1` i `0`, `pio run -e esp32-c6-zero`, `pio run`, `openspec validate ... --strict`.
6. Smoke test: ciśnienie ~98 kPa zgodne z Aqarą, vario działa, brak `DIAG:`.
7. Zarchiwizować zmianę po weryfikacji.

Wycofanie: ustawić `PRESSURE_SENSOR_MS5607=0` dla MS5611 albo przywrócić poprzedni commit; diagnostykę można odtworzyć z archiwum. Główna ścieżka vario/i-Bus pozostaje bez zmian.

## Open Questions

- Czy zdobyć oficjalny wektor datasheetu MS5607 dla drugiego rzędu? Nie blokuje wdrożenia przy potwierdzonym module i 25 °C; można dodać później.
- Czy docelowo zmienić nazwę capability/klasy/logu na `pressure-sensor`/`BARO:`? Osobny refaktor; teraz nie.
- Czy po naprawie typu ponownie zmierzyć buzzer/dryf? Tak, ale w osobnych zmianach.
