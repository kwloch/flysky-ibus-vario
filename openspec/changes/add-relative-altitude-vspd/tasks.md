# Tasks

## 1. Warstwa vario i testy hostowe

- [x] 1.1 Dodać `include/vario_math.h`: stałe interwałów, `pressureToAltitudeMeters()`, `VarioReading` oraz `VarioEstimator` z automatyczną referencją z pierwszego pomiaru, filtrem alpha-beta (`alpha=0.4`, `beta=0.2`), ograniczeniem `dt`, resetem po przerwie >250 ms i wyjściem w cm oraz cm/s; zweryfikować, że nagłówek kompiluje się bez Arduino w testach z 1.2–1.3.
- [x] 1.2 Dodać `test/test_vario_math/test_main.cpp` z Unity dla referencji i przeliczenia ciśnienia: pierwszy pomiar daje wysokość 0, ciśnienie niższe/wyższe daje znak dodatni/ujemny, `P0=101325 Pa` z `P=89876 Pa` i `P=113927 Pa` daje około `+1000 m` i `-1000 m` z tolerancją 0,1 m; zweryfikować przez `pio test -e native`, że testy przechodzą.
- [x] 1.3 Rozszerzyć ten sam plik testów o zachowanie filtra: stan ustalony przy stałym ciśnieniu, rampa 2 m/s (po 200 ms vspd ≥1,8 m/s i błąd wysokości ≤0,2 m), pojedyncze zakłócenie 0,1 m (zmiana wysokości <0,05 m, vspd w ±0,5 m/s), przerwa >250 ms (brak skoku >0,2 m/s, referencja bez zmian) oraz zaokrąglanie do cm i cm/s; zweryfikować przez `pio test -e native`, że wszystkie testy przechodzą.

## 2. Integracja firmware

- [x] 2.1 Zmienić `src/main.cpp` na harmonogram `millis()`: próbka ciśnienia co 50 ms, aktualizacja `VarioEstimator` tylko po prawidłowym odczycie z `dt` liczonym od poprzedniej prawidłowej próbki, zachowanie ponawiania inicjalizacji i logowania błędów; zweryfikować przez `pio run -e esp32-c6-zero`, że firmware się buduje, a `pio run` bez `-e` nadal wybiera `esp32-c6-zero`.
- [x] 2.2 Dodać w `src/main.cpp` log `VARIO: t=<ms> alt=<m> m vspd=<m/s> m/s` co 200 ms z dwoma miejscami po przecinku i znakiem dla wartości ujemnych (formatowanie całkowitoliczbowe) oraz zachować linię `MS5611: t=<ms> P=<Pa> Pa T=<°C> C` co około 1 s; zweryfikować przez `pio run -e esp32-c6-zero`, że build przechodzi i że formaty w kodzie odpowiadają specyfikacji.
- [x] 2.3 Zaktualizować `README.md`: oznaczyć punkt roadmapy „Wysokość względna + prędkość wznoszenia...” jako zrobiony, dopisać `include/vario_math.h` i `test/test_vario_math/` do struktury, opisać format `VARIO:`, jednostki cm/cm/s, parametry 20 Hz / 5 Hz / alpha-beta oraz polecenia `pio test -e native`, `pio run -e esp32-c6-zero -t upload` i `pio device monitor -e esp32-c6-zero`; zweryfikować, że README zawiera wszystkie nowe elementy i nie zmienia opisu i-Bus ani targetów.

## 3. Weryfikacja integracyjna

- [x] 3.1 Uruchomić `pio test -e native` oraz `pio run -e esp32-c6-zero`; zweryfikować, że testy przechodzą, firmware się buduje, a `pio run` bez `-e` nadal buduje wyłącznie `esp32-c6-zero`.
- [x] 3.2 Uruchomić `openspec validate add-relative-altitude-vspd --type change --strict`; zweryfikować, że walidacja kończy się bez błędów i ostrzeżeń.
- [ ] 3.3 Wykonać smoke test na sprzęcie (ESP32-C6-Zero + GY-63 na GPIO0/GPIO1): wgrać firmware, otworzyć `pio device monitor -e esp32-c6-zero` i sprawdzić, że linie `VARIO:` pojawiają się co ~200 ms, linie `MS5611:` co ~1 s, a w spoczynku wysokość i vspd oscylują blisko zera; następnie zmienić wysokość czujnika (np. winda/schody) i sprawdzić, że vspd reaguje w ciągu ~200 ms, a po odłączeniu i ponownym podłączeniu czujnika nie pojawia się fałszywy trwały skok. Jeśli sprzęt jest niedostępny, zapisać wynik jako oczekujący i nie oznaczać zadania jako zweryfikowane.
  - Wynik: OCZEKUJE — w środowisku nie ma podłączonego ESP32-C6-Zero ani GY-63.

## Workflow follow-up

- Wykonać smoke test sprzętowy i ewentualnie dostroić `alpha`/`beta` na podstawie rzeczywistego szumu, bez zmiany specyfikacji.
- Zarchiwizować zmianę po spełnieniu wymagań przeglądu projektu.
