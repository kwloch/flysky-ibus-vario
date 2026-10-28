# Tasks

## 1. Matematyka wysokości z temperaturą odniesienia

- [x] 1.1 W `include/vario_math.h` dodać stałe `kVarioStandardTemperatureK = 288.15f`, zakres temperatury `-4000..8500` (centygrady) oraz przeciążenie `pressureToAltitudeMeters(pressurePa, referencePa, referenceTemperatureK)` realizujące `44330 * (T_ref_K / 288.15) * (1 - (P/P_ref)^(1/5.255))`; zachować dotychczasową dwuargumentową funkcję standardową i dodać warunkowy przełącznik `VARIO_TEMPERATURE_CORRECTION` (domyślnie 1). Zweryfikować przez `pio test -e native`, że istniejące testy wysokości nadal przechodzą.
- [x] 1.2 Rozszerzyć `test/test_vario_math/test_main.cpp` o testy funkcji z temperaturą: zgodność z dotychczasowymi `+1000 m`/`-1000 m` przy `T_ref = 15 °C`, około `+1034,7 m` przy `25 °C`, około `+965,3 m` przy `5 °C` oraz fallback do 15 °C dla temperatury nieprawidłowej lub poza zakresem; zweryfikować przez `pio test -e native`, że wszystkie przypadki przechodzą z tolerancją 0,1 m.
- [x] 1.3 Dodać test odwrotnej zależności `P = P_ref * (1 - h / (44330 * T_ref_K / 288.15))^5.255` dla 5 °C, 15 °C i 25 °C, aby potwierdzić, że funkcja i generator ciśnienia w testach są spójne; zweryfikować przez `pio test -e native`, że test przechodzi.

## 2. Estymator, integracja firmware i dokumentacja

- [x] 2.1 Rozszerzyć `VarioEstimator` w `include/vario_math.h`: przechowywać `T_ref`, sumę i licznik prawidłowych temperatur w oknie referencyjnym, dodać `update(pressurePa, temperatureCentiC, dtMs)` oraz `referenceTemperatureK()`, a dwuargumentowe `update(pressurePa, dtMs)` pozostawić jako wywołanie z 15 °C. Zweryfikować przez `pio test -e native`, że istniejące testy estymatora nadal przechodzą.
- [x] 2.2 Dodać testy estymatora: uśrednianie `T_ref` z 20 próbek i jego stałość po referencji, brak prawidłowej temperatury → 15 °C, zmiana bieżącej temperatury o 10 °C przy stałym ciśnieniu → wysokość <1 cm i vspd ±0,05 m/s, szum bieżącej temperatury → vspd ±0,1 m/s, rampa 2 m/s przy `T_ref = 25 °C` i `5 °C` → po 700 ms vspd ≥1,8 m/s i po 1 s błąd wysokości ≤0,2 m, bez regresji testów szumu i przerwy >250 ms; zweryfikować przez `pio test -e native`, że wszystkie testy przechodzą.
- [x] 2.3 Zmienić `src/main.cpp`, aby po prawidłowym odczycie wywoływać `vario.update(sensor.sample().pressurePa, sensor.sample().temperatureCentiC, sampleMs - lastSampleMs)`; nie zmieniać logów, harmonogramu ani emisji i-Bus. Zweryfikować przez `pio run -e esp32-c6-zero` oraz `pio run` bez `-e`, że firmware się buduje i nadal wybierany jest target `esp32-c6-zero`.
- [x] 2.4 Zaktualizować `README.md`: opisać korekcję temperaturą odniesienia z okna startowego, wzór, zachowanie przy 15 °C, ograniczenia (temperatura matrycy vs powietrze, self-heating, brak QNH/GPS), polecenie porównania `PLATFORMIO_BUILD_FLAGS="-DVARIO_TEMPERATURE_CORRECTION=0" pio run -e esp32-c6-zero` i oznaczyć punkt roadmapy. Zweryfikować, że README zawiera wszystkie te elementy, a opisy i-Bus i targetów pozostały bez zmian.

## 3. Weryfikacja integracyjna

- [x] 3.1 Uruchomić `pio test -e native`, `pio run -e esp32-c6-zero` i `pio run` bez `-e`; zweryfikować, że testy przechodzą, build się udaje, a domyślnym targetem pozostaje `esp32-c6-zero`.
- [x] 3.2 Uruchomić `PLATFORMIO_BUILD_FLAGS="-DVARIO_TEMPERATURE_CORRECTION=0" pio test -e native` oraz analogiczny build firmware; zweryfikować, że wariant bez korekcji kompiluje się i testy warunkowe przechodzą. Jeśli zmienna nie propaguje się do testów, użyć chwilowej `build_flags` w `[env:native]` i usunąć ją po weryfikacji.
- [x] 3.3 Uruchomić `openspec validate add-altitude-temperature-correction --type change --strict`; zweryfikować, że walidacja kończy się bez błędów i ostrzeżeń.
- [ ] 3.4 Wykonać smoke test na sprzęcie (ESP32-C6-Zero + GY-63): wgrać firmware, otworzyć `pio device monitor -e esp32-c6-zero` i sprawdzić, że przy stałym ciśnieniu zmiana temperatury czujnika (np. ogrzanie palcem) nie zmienia `alt` ani `vspd`; porównać wysokość z włączoną i wyłączoną korekcją przy tej samej temperaturze startu i zapisać wynik. Jeśli sprzęt jest niedostępny, zapisać wynik jako oczekujący i nie oznaczać zadania jako zweryfikowane.
  - Wynik: OCZEKUJE — `pio device list` nie widzi podłączonego ESP32-C6; smoke test do wykonania na sprzęcie.

## Workflow follow-up

- Zarchiwizować zmianę po spełnieniu wymagań przeglądu projektu.
- Po locie długodystansowym zebrać dane `VARIO:` + `MS5611:` i ocenić, czy potrzebny jest offset self-heatingu lub wariant z bieżącą temperaturą; decyzję i ewentualne zmiany zaplanować osobno.
