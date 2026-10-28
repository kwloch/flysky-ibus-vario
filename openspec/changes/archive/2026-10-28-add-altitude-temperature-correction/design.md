# Design

## Context

Motywację i odpowiedź na „czy warto / jak robią inni” zawiera `proposal.md` (sekcja Why). Wymagania behawioralne opisuje delta `specs/altitude-vario/spec.md`.

Stan obecny:

- `include/vario_math.h` ma `pressureToAltitudeMeters(pressurePa, referencePa)` ze wzorem `44330 * (1 - (P/P0)^(1/5.255))` oraz `VarioEstimator::update(int32_t pressurePa, uint32_t dtMs)`, który w oknie 20 próbek uśrednia tylko ciśnienie.
- `src/main.cpp` po każdym prawidłowym odczycie przekazuje do estymatora wyłącznie `sensor.sample().pressurePa`; temperatura jest logowana, ale nieużywana.
- `MS5611::read()` zwraca `Sample{pressurePa, temperatureCentiC}` i odrzuca cały pomiar, jeśli ciśnienie lub temperatura są poza zakresem (`ms5611_math.h`). Dzięki temu każda próbka, która dociera do estymatora, ma prawidłową temperaturę, ale warstwa vario i tak powinna być odporna na brak sensownej temperatury.
- Testy hostowe (`test/test_vario_math/`) używają odwrotnego wzoru standardowego do generowania ciśnienia z zadanej wysokości; po zmianie potrzebny jest wariant odwrotny z temperaturą odniesienia.
- Brak GPS i drugiego czujnika temperatury; dostępna jest tylko temperatura matrycy MS5611.

## Goals / Non-Goals

**Goals:**

- Poprawić dokładność wysokości względnej dla nie-ISA temperatury startu bez zmiany sposobu zerowania, filtracji i emisji i-Bus.
- Użyć temperatury MS5611 w sposób, który nie wprowadza dryfu wysokości od self-heatingu w trakcie lotu.
- Zachować dotychczasowe zachowanie przy 15 °C i przy braku wiarygodnej temperatury.
- Pokryć nową matematykę testami hostowymi, w tym ciepły/zimny start i brak temperatury.

**Non-Goals:**

- Wysokość bezwzględna, QNH, fuzja z GPS, kompensacja wilgotności i temperatury wirtualnej.
- Kalibracja offsetu self-heatingu, obudowa i wentylacja czujnika.
- Zmiana filtra alpha-beta, częstotliwości próbkowania/publikacji, formatu logu lub ramek i-Bus.
- Konfiguracja w locie (przycisk, komenda serial, radio).

## Decisions

### Temperatura odniesienia z okna startowego, nie bieżąca temperatura

Podczas tego samego okna 20 prawidłowych pomiarów, w którym wyznaczane jest `P_ref`, uśredniać także temperaturę i zapisać `T_ref`. Po wypełnieniu okna wysokość liczyć zawsze z `T_ref`; bieżąca temperatura nie wpływa już na wysokość ani vspd.

Uzasadnienie: dla wysokości względnej właściwą temperaturą warstwy jest temperatura na poziomie odniesienia, a nie chwilowa temperatura matrycy na aktualnej wysokości. `T_ref` jest stała, więc ewentualny dryf self-heatingu, nagrzewanie elektroniki i szum termiczny nie tworzą wolnego dryfu wysokości ani fałszywego wznoszenia. To spełnia wymaganie „Stabilność wysokości przy zmianie bieżącej temperatury”.

Alternatywa: bieżąca temperatura w każdym pomiarze, jak w `msrc` (`(T+273.15)*(1-(P/P0)^(1/5.256))/0.0065`). Odrzucona jako domyślna, bo temperatura matrycy w małej obudowie może być o kilka stopni wyższa od powietrza, a jej dryf w locie przekłada się bezpośrednio na dryf wysokości (ok. 3,5 m na 1 °C przy 1000 m) i może maskować rzeczywiste wznoszenie. Pozostaje wariantem do porównań na stanowisku.

Alternatywa: równanie hipsometryczne z uśrednieniem `(T_ref + T_bieżąca)/2`. Odrzucona w tej zmianie, bo nadal wymaga bieżącej temperatury matrycy i komplikuje filtr; może wrócić, jeśli w przyszłości pojawi się dobrze wentylowany czujnik temperatury powietrza.

### Wzór: standardowy, przeskalowany temperaturą odniesienia

Zachować obecny wykładnik i stałą, a przeskalować człon wysokości:

```text
h = 44330 * (T_ref_K / 288.15) * (1 - (P / P_ref)^(1/5.255))
```

Dla `T_ref_K = 288.15 K` (15 °C) wzór jest identyczny z dotychczasowym, więc testy i zachowanie w standardowej atmosferze nie zmieniają się. Wzór jest równoważny wersji z gradientem `h = (T_ref/L)*(1-(P/P_ref)^(R*L/g))` dla `L = 288.15/44330 ≈ 0.0065 K/m`; wybrano formę ze stałą `44330`, bo minimalizuje diff i utrzymuje tolerancję 0,1 m względem istniejącej specyfikacji.

Alternatywa: użyć dokładnych stałych `R = 287.05`, `g = 9.80665`, `L = 0.0065` i wykładnika `R*L/g`. Odrzucona, bo daje inną stałą skali (44330,8 zamiast 44330) i wymagałaby zmiany istniejących oczekiwań testowych bez zauważalnego zysku; różnica na typowych wysokościach jest poniżej 0,1 m, ale nie ma powodu jej wprowadzać.

Alternatywa: linearyzacja `h ≈ (P_ref - P)/(rho*g)`. Odrzucona — błąd rzędu kilku procent na 1000 m.

### Zakres i fallback temperatury

Uznawać temperaturę za prawidłową, gdy mieści się w `-40..85 °C` (ten sam zakres co `ms5611-sensor`). Do średniej `T_ref` wliczać tylko prawidłowe temperatury; jeśli w oknie nie ma żadnej, ustawić `T_ref = 288.15 K`. Po wypełnieniu okna nie zmieniać `T_ref` nawet przy błędach odczytu lub przerwie.

Alternatywa: odrzucać całą próbkę bez prawidłowej temperatury także w oknie referencyjnym. Odrzucona, bo opóźniałaby start vario i w praktyce nic nie wnosi: `MS5611::read()` i tak odrzuca próbkę z temperaturą poza zakresem.

Alternatywa: nie robić fallbacku i wstrzymać wysokość do uzyskania temperatury. Odrzucona — pojedynczy problem z temperaturą nie powinien blokować vario; standardowa atmosfera jest bezpiecznym zachowaniem.

### API: nowe przeciążenie `update()` i funkcja z temperaturą

Zachować dwuargumentowe `update(pressurePa, dtMs)` jako wariant standardowy (15 °C), aby nie psuć prostych wywołań i testów. Dodać trzyargumentowe `update(pressurePa, temperatureCentiC, dtMs)`, którego użyje `main.cpp`. W `include/vario_math.h`:

```cpp
constexpr float kVarioStandardTemperatureK = 288.15f;
constexpr int32_t kVarioMinTemperatureCentiC = -4000;
constexpr int32_t kVarioMaxTemperatureCentiC = 8500;

float pressureToAltitudeMeters(float pressurePa,
                               float referencePa);

float pressureToAltitudeMeters(float pressurePa,
                               float referencePa,
                               float referenceTemperatureK);

class VarioEstimator {
 public:
  void update(int32_t pressurePa, uint32_t dtMs);
  void update(int32_t pressurePa, int32_t temperatureCentiC, uint32_t dtMs);
  float referenceTemperatureK() const;
  // ... istniejące metody bez zmian
};
```

Estymator przechowuje `referenceTemperatureK_` oraz sumę i licznik prawidłowych temperatur w oknie. Po wypełnieniu okna `referenceTemperatureK_` jest stała. Dwuargumentowe `update()` przekazuje 15 °C, więc działa jak dotychczas.

Alternatywa: przekazywać `Sample` z `ms5611_math.h`. Odrzucona, bo `vario_math.h` nie powinien zależeć od nagłówka sterownika czujnika; testy i przyszły inny barometr mają pozostać niezależne.

Alternatywa: osobne `setReferenceTemperature()` wywoływane z `main.cpp`. Odrzucona — łatwo rozjechać referencję z oknem ciśnienia i trudniej to przetestować.

### Przełącznik kompilacyjny do porównań

Dodać w `vario_math.h` możliwość wyłączenia korekcji na czas testów stanowiskowych:

```cpp
#ifndef VARIO_TEMPERATURE_CORRECTION
#define VARIO_TEMPERATURE_CORRECTION 1
#endif
constexpr bool kVarioTemperatureCorrectionEnabled =
    VARIO_TEMPERATURE_CORRECTION != 0;
```

Domyślnie korekcja jest włączona. Przy `-DVARIO_TEMPERATURE_CORRECTION=0` estymator ignoruje temperaturę i używa 288,15 K, co daje dokładnie dotychczasowe zachowanie. Testy ciepłego/zimnego startu są kompilowane warunkowo (`#if kVarioTemperatureCorrectionEnabled`), aby oba warianty buildu przechodziły.

Alternatywa: konfiguracja runtime przez serial/radio. Odrzucona w tej zmianie — wymaga parsera, UI i nowej specyfikacji; przełącznik kompilacyjny wystarcza do porównania A/B na stanowisku.

### Filtr i jednostki bez zmian

Nie zmieniać `alpha=0.1`, `beta=0.02`, ograniczeń `dt`, resetu po przerwie >250 ms ani wyjścia w cm/cm/s. Korekcja zmienia jedynie skalę wysokości o `T_ref/288.15` (dla 5–25 °C od −3,5% do +3,5%), więc czasy odpowiedzi i tłumienie szumu pozostają w istniejących tolerancjach. Testy filtra należy powtórzyć dla `T_ref = 5 °C` i `25 °C`, aby to potwierdzić.

Alternatywa: stroić `alpha`/`beta` zależnie od temperatury. Odrzucona — skala zmienia się o kilka procent, a dodatkowa zależność komplikuje filtr i utrudnia reprodukcję wyników.

### Integracja w `main.cpp`

W gałęzi prawidłowego odczytu wywołać:

```cpp
const Sample& sample = sensor.sample();
vario.update(sample.pressurePa, sample.temperatureCentiC,
             sampleMs - lastSampleMs);
```

Logi `MS5611:`, `VARIO:`, logi i-Bus i harmonogram 20 Hz/5 Hz pozostają bez zmian. Przy błędzie odczytu estymator nie jest aktualizowany, tak jak dotychczas.

### Testy hostowe

W `test/test_vario_math/test_main.cpp` dodać pomocniczą funkcję odwrotną:

```text
P = P_ref * (1 - h / (44330 * T_ref_K / 288.15))^5.255
```

Pokrycie:

- `pressureToAltitudeMeters` w 15 °C: dotychczasowe `+1000 m` / `-1000 m` z tolerancją 0,1 m.
- 25 °C i 5 °C: odpowiednio ok. `+1034,7 m` i `+965,3 m` dla `P_ref=101325 Pa`, `P=89876 Pa`.
- uśrednianie `T_ref` w oknie 20 próbek (np. temperatury 20,0–21,9 °C) i dostęp przez `referenceTemperatureK()`.
- brak prawidłowej temperatury w oknie → standardowe 15 °C.
- zmiana bieżącej temperatury po referencji przy stałym ciśnieniu → wysokość i vspd bez zmian.
- rampa 2 m/s przy `T_ref=25 °C` i `5 °C`: po 700 ms vspd ≥1,8 m/s, po 1 s błąd wysokości ≤0,2 m.
- szum i pojedyncze zakłócenie bez regresji.

Alternatywa: testy tylko na `pressureToAltitudeMeters`, bez estymatora. Odrzucona — nie sprawdziłyby uśredniania ani braku wpływu bieżącej temperatury.

### Aktualizacja README

Opisać korekcję, wzór, że używana jest temperatura odniesienia z okna startowego, ograniczenia (temperatura matrycy ≠ temperatura powietrza, self-heating, brak QNH/GPS) oraz sposób porównania:

```bash
PLATFORMIO_BUILD_FLAGS="-DVARIO_TEMPERATURE_CORRECTION=0" pio run -e esp32-c6-zero
```

Oznaczyć punkt roadmapy jako zrobiony i dopisać, że korekcja dotyczy wysokości względnej, nie bezwzględnej.

## Risks / Trade-offs

- **Temperatura matrycy ≠ temperatura powietrza** → korekcja może poprawić lub pogorszyć wynik zależnie od obudowy i wentylacji. Mitygacja: użycie tylko `T_ref`, dokumentacja ograniczenia, zalecenie przewiewnej obudowy; docelowo możliwy offset kalibracyjny jako osobna zmiana.
- **T_ref z pierwszych ~1 s może obejmować stan nieustalony termicznie** → mitygacja: urządzenie jest zasilane przed startem; jeśli testy pokażą problem, dodać opóźnienie stabilizacji jako osobną zmianę (bez zmiany wzoru).
- **Założenie stałego gradientu temperatury w całej warstwie** → w realnej atmosferze profil może być inny; korekcja jest pierwszym przybliżeniem. Mitygacja: nie deklarować dokładności bezwzględnej; w przyszłości GPS/QNH.
- **Skala vspd zmienia się o ±3,5% przy 5/25 °C** → czasy odpowiedzi mogą się nieznacznie różnić. Mitygacja: testy filtra przy skrajnych temperaturach; tolerancje specyfikacji bez zmian.
- **Przełącznik kompilacyjny tworzy drugi wariant** → ryzyko, że wariant `0` nie jest testowany. Mitygacja: testy warunkowe i uruchomienie obu wariantów w zadaniach weryfikacyjnych.
- **Brak poprawy, jeśli temperatura startu jest bliska ISA** → oczekiwane; korekcja nie szkodzi, bo przy 15 °C wzór jest identyczny.
- **Fallback maskuje awarię temperatury** → wysokość nadal działa, ale bez korekcji; log `MS5611:` nadal pokazuje temperaturę, więc problem jest widoczny.

## Migration Plan

Brak migracji danych i stanu trwałego. Wdrożenie:

1. Dodać nowe stałe, przeciążenia i stan `T_ref` w `include/vario_math.h`.
2. Przekazać temperaturę w `src/main.cpp`.
3. Zaktualizować/dodać testy hostowe i uruchomić `pio test -e native`.
4. Uruchomić `pio run -e esp32-c6-zero` oraz `pio run` bez `-e`.
5. Zaktualizować README i zweryfikować `openspec validate ... --strict`.
6. Smoke test na sprzęcie: porównać `VARIO:` z włączoną i wyłączoną korekcją przy tej samej temperaturze startu; sprawdzić, że przy stałym ciśnieniu zmiana temperatury (np. ogrzanie czujnika) nie rusza wysokości.

Wycofanie: usunąć przeciążenie i stan `T_ref`, przywrócić wywołanie `vario.update(pressurePa, dtMs)` w `main.cpp` oraz poprzednie testy/README. Sterownik MS5611 i i-Bus pozostają nietknięte.

## Open Questions

- Czy po testach na sprzęcie potrzebny będzie stały offset temperatury (self-heating) albo filtr temperatury odniesienia? To można dodać później jako osobną zmianę; nie zmienia wzoru ani kontraktu wysokości.
- Czy w przyszłości warto porównać wariant `T_ref` z wariantem bieżącej temperatury (`msrc`) na rzeczywistych długich lotach? Wymaga danych z lotu, nie blokuje implementacji.
- Czy docelowo dodać QNH/GPS, aby poprawić wysokość bezwzględną? Poza zakresem tej zmiany.
