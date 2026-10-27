# Proposal

## Why

Projekt ma już targety PlatformIO, ale nie potrafi jeszcze odczytać barometru — kolejny punkt roadmapy. Bez sprawdzonego odczytu ciśnienia i temperatury z MS5611 (GY-63) nie da się policzyć wysokości ani prędkości wznoszenia, a bez walidacji PROM uszkodzony lub źle podłączony czujnik może dawać wiarygodnie wyglądające, ale błędne dane.

## What Changes

- Dodać samodzielny sterownik MS5611 po I²C (bez zewnętrznej biblioteki sensorowej): adres `0x77`, magistrala 400 kHz, reset, odczyt 8 słów PROM, CRC4 i kontrola sensowności współczynników.
- Dodać odczyt surowych wartości D1/D2 z oversamplingiem 4096, kompensację drugiego rzędu i wynik w jednostkach `Pa` oraz `°C`.
- Dodać czystą, testowalną warstwę obliczeń (CRC4, walidacja PROM, kompensacja, zakres pomiaru) oddzieloną od sprzętowego I²C.
- Dodać minimalny `src/main.cpp`: inicjalizacja I²C i czujnika oraz log ciśnienia i temperatury co ~1 s po USB w formacie czytelnym w `pio monitor` (czas w ms, ciśnienie w Pa, temperatura w °C, osobne linie błędów); przy braku czujnika lub błędzie odczytu program raportuje problem i ponawia próbę zamiast się zawieszać.
- Dodać środowisko `native` w `platformio.ini` z Unity do testów hostowych CRC4 i kompensacji na wartościach z datasheetu; nie zmienia ono domyślnego targetu `esp32-c6-zero`.
- Zaktualizować `README.md`: oznaczyć punkt roadmapy jako zrobiony i doprecyzować strukturę plików oraz sposób uruchamiania testów.
- Poza zakresem: wysokość względna, filtracja i prędkość wznoszenia, emulacja i-Bus, obsługa SPI, target nRF52840, sterowanie diodą WS2812, własny board def oraz okablowanie mechaniczne.

## Capabilities

### New Capabilities

- `ms5611-sensor`: inicjalizacja MS5611 po I²C, walidacja PROM (CRC4 i sensowność współczynników), kompensowany odczyt ciśnienia i temperatury oraz raportowanie błędów czujnika.

### Modified Capabilities

- `platformio-configuration`: dodanie środowiska testów hostowych `native` (Unity) obok dwóch środowisk budowania firmware, bez zmiany domyślnego targetu.

## Impact

- Nowe pliki: `src/ms5611.h`, `src/ms5611.cpp`, `src/main.cpp`, `include/ms5611_math.h`, testy w `test/`.
- Zmieniane pliki: `platformio.ini` (nowe środowisko `native`), `README.md` (roadmapa i struktura).
- Brak nowej zależności runtime — sterownik jest własny. Testy hostowe korzystają z Unity pobieranego przez PlatformIO.
- Kolejne zadania (wysokość, vspd, i-Bus) będą korzystać z API sterownika; finalny smoke test wymaga fizycznego GY-63 podłączonego do GPIO0/GPIO1 ESP32-C6-Zero.
