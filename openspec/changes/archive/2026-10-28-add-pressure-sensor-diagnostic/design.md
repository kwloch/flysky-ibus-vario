# Design

## Context

Motywację i obserwacje (`49 103 Pa` vs Aqara `97 969 Pa`, poprawny PROM, zachowany stosunek `P/P_ref`) zawiera `proposal.md`. Wymagania behawioralne opisuje delta `specs/ms5611-sensor/spec.md`.

Stan kodu:

- `include/ms5611_math.h` ma `compensate()` dla MS5611 i `Sample{pressurePa, temperatureCentiC}`; testy hostowe przechodzą na wektorze z datasheetu MS5611 (`P=100009 Pa`, `T=20.07 °C`).
- `src/ms5611.cpp` wykonuje konwersje D1/D2, ale **nie przechowuje** surowych wartości — po `compensate()` zostają tylko wynikowe Pa/°C.
- `src/main.cpp` co ~1 s loguje `MS5611: t=... P=... T=...`; vario i i-Bus korzystają z tego samego `Sample` (MS5611).
- PROM użytkownika ma CRC `0x03` i poprawne C1–C6, więc I²C i odczyt PROM działają. Typu czujnika nie da się odczytać z PROM ani z adresu I²C.

## Goals / Non-Goals

**Goals:**

- Policzyć z **tych samych** surowych D1/D2 zarówno wynik MS5611, jak i MS5607.
- Wypisać oba wyniki w jednej linii diagnostycznej, żeby porównać je z Aqarą i rozstrzygnąć typ czujnika bez mikroskopu.
- Nie zmieniać działającego vario/i-Bus ani istniejącego logu `MS5611:`.
- Zapewnić czystą, testowalną funkcję MS5607 w `ms5611_math.h`.

**Non-Goals:**

- Przełączenie vario na MS5607, automatyczne wykrywanie typu, finalna poprawka kompensacji.
- Kalibracja przez mnożenie ciśnienia przez 2.
- Zmiana filtra, deadbandu buzera, re-zero/dryfu.
- Wymiana sprzętu.

## Decisions

### Diagnostyka przez dwie kompensacje z surowych D1/D2

Przechowywać ostatnie `D1` i `D2` w sterowniku i udostępnić je getterami. Linia `DIAG:` liczy niezależnie:

- `P5611/T5611` istniejącym `compensate()`,
- `P5607/T5607` nowym `compensateMs5607()`.

Przy `T > 20 °C` różnica pierwszego rzędu MS5607/MS5611 to czyste 2x, więc jeśli czujnik jest MS5607, `P5607` powinno trafić w Aqarę (~98 kPa), a `P5611` będzie ~49 kPa. Jeśli jest MS5611, będzie odwrotnie.

Alternatywa: założyć MS5607 i od razu zmienić główną kompensację. Odrzucona — najpierw trzeba potwierdzić typ, a zmiana głównej ścieżki zepsułaby działające vario, gdyby czujnik jednak był MS5611.
Alternatywa: pomnożyć `P5611` przez 2. Odrzucona — to obejście, które nie działa poprawnie w niskich temperaturach (inny drugi rząd) i nie daje niezależnego dowodu.

### MS5607 jako osobna funkcja w `include/ms5611_math.h`

Dodać `Sample compensateMs5607(const Prom&, uint32_t d1, uint32_t d2)` obok istniejącej `compensate()`. Wzory pierwszego rzędu:

```text
dT   = D2 - C5 * 2^8
TEMP = 2000 + dT * C6 / 2^23
OFF  = C2 * 2^17 + C4 * dT / 2^6
SENS = C1 * 2^16 + C3 * dT / 2^7
P    = ((D1 * SENS / 2^21) - OFF) / 2^15
```

Drugi rząd (dla `TEMP < 2000`, z datasheetu MS5607):

```text
T2    = dT^2 / 2^31
OFF2  = 61 * (TEMP - 2000)^2 / 2^4
SENS2 = 2 * (TEMP - 2000)^2
jeśli TEMP < -1500:
  OFF2  += 15 * (TEMP + 1500)^2
  SENS2 += 8 * (TEMP + 1500)^2
TEMP -= T2; OFF -= OFF2; SENS -= SENS2
```

Implementacja używa `int64_t` tak jak istniejąca `compensate()`, żeby uniknąć przepełnień. Funkcja **nie** stosuje `inRange()` — diagnostyka ma pokazać także wynik poza normalnym zakresem (np. 200018 Pa na wektorze MS5611), a zakresem zajmuje się główna ścieżka MS5611.

Alternatywa: parametr `sensorType` w istniejącej `compensate()`. Odrzucona — zwiększa ryzyko regresji w działającej ścieżce i komplikuje testy; osobna funkcja jest czytelniejsza i łatwiejsza do usunięcia po diagnozie.

### Sterownik przechowuje surowe D1/D2

W `MS5611` dodać `d1_`, `d2_`, gettery `rawD1()`, `rawD2()` i zerowanie w `begin()`. W `read()` zapisywać wartości zaraz po udanych odczytach ADC, przed kompensacją i kontrolą zakresu. `Sample` i `hasSample()` pozostają bez zmian.

Alternatywa: zwracać z `read()` strukturę z surowymi i skompensowanymi danymi. Odrzucona — zmienia API sterownika i wszystkie wywołania; gettery są mniej inwazyjne.

### Linia `DIAG:` co 1 s, za przełącznikiem kompilacyjnym

W `main.cpp` dodać `logDiagnostic()` wywoływane w istniejącym bloku logu ciśnienia (co ~1 s), po `logSample()`. Format:

```text
DIAG: t=<ms> D1=<raw> D2=<raw> P5611=<Pa> P5607=<Pa> T5611=<°C> T5607=<°C>
```

Liczby całkowite, temperatura z dwoma miejscami po przecinku i znakiem, bez `%f`. Przełącznik:

```cpp
#ifndef PRESSURE_SENSOR_DIAGNOSTIC
#define PRESSURE_SENSOR_DIAGNOSTIC 1
#endif
```

Domyślnie włączony w tej zmianie, bo użytkownik musi ją wgrać i odczytać. Wyłączenie: `-DPRESSURE_SENSOR_DIAGNOSTIC=0` w `build_flags`. `VARIO:`, `MS5611:` i emisja i-Bus pozostają bez zmian.

Alternatywa: osobne środowisko PlatformIO. Odrzucona — dodatkowa konfiguracja bez zysku; flaga w `main.cpp` wystarcza.
Alternatywa: komenda po serialu. Odrzucona — wymaga parsera i nowej specyfikacji; diagnostyka ma być jednorazowa.

### Interpretacja wyników i zakończenie diagnostyki

Na ~300 m n.p.m. z Aqarą ~97 969 Pa:

- `P5607 ≈ 97–98 kPa`, `P5611 ≈ 49 kPa` → czujnik MS5607, firmware używa złego wzoru.
- `P5611 ≈ 97–98 kPa`, `P5607 ≈ 195 kPa` → czujnik MS5611; problem leży gdzie indziej (PROM, odczyt, uszkodzenie).
- oba daleko od referencji → nie zakładać typu; sprawdzić PROM/odczyt/zakres.

Po rozstrzygnięciu należy **usunąć lub wyłączyć** diagnostykę i zaplanować osobną zmianę: wsparcie MS5607 albo wymiana modułu. Diagnostyka nie jest docelową funkcją produktu.

### Testy hostowe

Dodać test pierwszeństwa na istniejącym wektorze datasheetu MS5611:

```text
PROM = {{0x0001,0x9CBF,0x903C,0x5B15,0x5AF2,0x82B8,0x6E98,0x123B}}
D1 = 9085466, D2 = 8569150
compensate()        -> P = 100009 Pa, T = 2007
compensateMs5607()  -> P = 200018 Pa, T = 2007
```

To dowodzi, że różnica pierwszego rzędu wynosi dokładnie 2x i że diagnostyka rozpozna wariant. Dodać też test, że `compensateMs5607()` nie modyfikuje PROM i nie zależy od `inRange()`. Testy drugiego rzędu MS5607 należy oprzeć na wektorze z datasheetu, jeśli będzie dostępny; przy braku wektora ograniczyć się do pierwszego rzędu i odnotować to w zadaniach (diagnostyka i tak działa przy 25 °C).

Alternatywa: testować tylko na sprzęcie. Odrzucona — błąd w shiftach dałby mylące `DIAG:` i zmarnował iterację.

### README

Opisać: co to za diagnostyka, jak włączyć/wyłączyć, jak czytać `DIAG:`, jakie wartości oznaczają MS5607 vs MS5611 i że po diagnozie trzeba usunąć/wyłączyć linię. Nie zmieniać sekcji i-Bus/targetów.

## Risks / Trade-offs

- **Zły drugi rząd MS5607** → przy 25 °C nie ma to znaczenia; dla zimna zweryfikować z datasheetem i ewentualnie dodać wektor. Diagnostyka typu i tak opiera się na pierwszym rzędzie.
- **Wynik MS5607 poza zakresem 10–120 kPa** (np. 200 kPa na wektorze) → nie stosować `inRange()` do diagnostyki; logować surowe Pa.
- **Diagnostyka myli użytkownika** → README z prostą tabelą interpretacji; linia `DIAG:` obok istniejącego `MS5611:`.
- **Dodatkowy ruch po serialu** → tylko 1 Hz, kilka liczb; pomijalny.
- **Diagnostyka zostanie w produkcie** → flaga domyślnie 1 w tej zmianie; po diagnozie obowiązkowe wyłączenie/usunięcie w follow-upie.
- **Brak surowych D1/D2 przy błędzie odczytu** → logować tylko gdy `hasSample()`; ostatnie poprawne wartości są wystarczające.
- **PROM nie identyfikuje typu** → nie próbować automatycznego wykrywania po PROM; porównanie dwóch kompensacji z referencją jest dowodem.

## Migration Plan

1. Dodać `compensateMs5607()` i test 2x w `ms5611_math.h` / `test_ms5611_math`.
2. Dodać surowe D1/D2 do sterownika.
3. Dodać `logDiagnostic()` i flagę w `main.cpp`.
4. Zaktualizować README.
5. Uruchomić `pio test -e native`, `pio run -e esp32-c6-zero`, `pio run`, `openspec validate ... --strict`.
6. Wgrać firmware, otworzyć `pio device monitor -e esp32-c6-zero`, porównać `P5611`/`P5607` z Aqarą ~97 969 Pa i zapisać wynik.
7. Po diagnozie: wyłączyć/usunąć diagnostykę i zaplanować właściwą poprawkę (MS5607 albo wymiana modułu).

Wycofanie: usunąć `logDiagnostic()`/flagę i gettery surowych D1/D2; `compensateMs5607()` można zostawić do przyszłego wsparcia MS5607 lub usunąć razem ze zmianą. Główna ścieżka vario nie jest modyfikowana, więc ryzyko regresji jest małe.

## Open Questions

- Czy uda się zdobyć oficjalny wektor datasheetu MS5607 dla drugiego rzędu? To nie blokuje diagnostyki przy 25 °C; można dodać później.
- Czy po potwierdzeniu MS5607 docelowo wspierać oba typy (flaga kompilacyjna) czy wymienić moduł? Decyzja po wyniku `DIAG:`.
- Czy diagnostykę zostawić za flagą na stałe, czy usunąć po zakończeniu? Domyślnie do usunięcia; decyzja w follow-upie.
