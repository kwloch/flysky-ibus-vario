# Tasks

## 1. Środowisko testów hostowych

- [x] 1.1 Dodać do `platformio.ini` środowisko `[env:native]` z `platform = native`, pustym `framework =`, `test_framework = unity` i `test_build_src = no`; zweryfikować przez `pio project config --json-output`, że `native` nie dziedziczy Arduino, a `default_envs` nadal wskazuje `esp32-c6-zero`.
- [x] 1.2 Dopisać w `README.md` informację o testach hostowych i poleceniu `pio test -e native`; zweryfikować, że opisane polecenie odpowiada konfiguracji i nie sugeruje potrzeby podłączania sprzętu.

## 2. Czysta matematyka i testy hostowe

- [x] 2.1 Dodać `include/ms5611_math.h` z typami PROM/próbki, CRC4, walidacją PROM (`0x0000`/`0xFFFF`), kompensacją całkowitoliczbową pierwszego i drugiego rzędu oraz kontrolą zakresu; zweryfikować, że nagłówek kompiluje się bez Arduino w testach z 2.2.
- [x] 2.2 Dodać `test/test_ms5611_math/test_main.cpp` z Unity: CRC4 dla `C7=0x123B` → `0x0B`, zmiana bitu → niezgodność, odrzucenie `0x0000`/`0xFFFF`, wektory `2007`/`100009`, `-62`/`95989`, `-2130`/`91910` oraz granice zakresu; zweryfikować przez `pio test -e native`, że wszystkie testy przechodzą.

## 3. Sterownik i minimalny firmware

- [x] 3.1 Dodać `src/ms5611.h` i `src/ms5611.cpp`: reset, odczyt PROM z CRC4, konwersje D1/D2 przy OSR 4096, odczyt ADC, kompensacja, zakres, próbka i kody błędów; zweryfikować kompilację i linkowanie w buildzie z 3.2.
- [x] 3.2 Dodać `src/main.cpp`: `Wire.begin(0, 1)` z 400 kHz, inicjalizacja z ponawianiem po błędzie i log ASCII co ~1 s w formacie `MS5611: t=<ms> P=<Pa> Pa T=<°C> C`, z liniami `boot`, `init OK` (CRC + PROM), `init FAILED <błąd>; retry in 1000 ms` i `read FAILED <błąd>; continuing`; zweryfikować przez `pio run -e esp32-c6-zero`, że firmware się buduje.
- [x] 3.3 Zaktualizować `README.md`: dopisać `src/ms5611.*`, `include/ms5611_math.h` i `test/` do struktury, oznaczyć punkt roadmapy „Odczyt MS5611 po I²C...” jako zrobiony oraz opisać `pio run -e esp32-c6-zero -t upload` i `pio device monitor -e esp32-c6-zero`; zweryfikować, że README wymienia nowe pliki, polecenia testów i monitora oraz zaktualizowany status.

## 4. Weryfikacja integracyjna

- [x] 4.1 Uruchomić `pio test -e native` oraz `pio run -e esp32-c6-zero`; zweryfikować, że testy przechodzą, firmware się buduje, a `pio run` bez `-e` nadal wybiera `esp32-c6-zero`.
- [x] 4.2 Uruchomić `openspec validate add-ms5611-i2c-read --type change --strict`; zweryfikować, że walidacja kończy się bez błędów i ostrzeżeń.
- [ ] 4.3 Wykonać smoke test na sprzęcie (ESP32-C6-Zero + GY-63 na GPIO0/GPIO1): wgrać firmware i otworzyć `pio device monitor -e esp32-c6-zero`; sprawdzić, że pojawiają się czytelne linie `MS5611: t=... P=... Pa T=... C` co ~1 s z plausibilnymi wartościami. Następnie odłączyć czujnik i sprawdzić, że pojawia się `read FAILED ...; continuing`, a program ponawia próby; po ponownym podłączeniu pomiary wracają. Jeśli sprzęt jest niedostępny, zapisać wynik jako oczekujący i nie oznaczać zadania jako zweryfikowane.
  - Wynik: OCZEKUJE — w środowisku nie ma podłączonego ESP32-C6-Zero ani GY-63; test sprzętowy należy wykonać przed archiwizacją.

## Workflow follow-up

- Uzupełnić smoke test sprzętowy i ewentualnie dodać rzeczywisty zrzut PROM jako fixture CRC przed archiwizacją.
- Zarchiwizować zmianę po spełnieniu wymagań przeglądu projektu.
