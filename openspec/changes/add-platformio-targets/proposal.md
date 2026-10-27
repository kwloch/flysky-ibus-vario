# Proposal

## Why

Repozytorium nie ma jeszcze konfiguracji budowania, więc nie da się zbudować firmware ani dla pierwszego targetu ESP32-C6-Zero, ani dla docelowego modułu nRF52840. Research wybrał platformy i boardy, ale te ustalenia nie są zapisane w formie wykonywalnej konfiguracji — bez `platformio.ini` każde przyszłe zadanie zaczynałoby się od ręcznego odtwarzania tych samych ustawień.

## What Changes

- Dodać `platformio.ini` w katalogu głównym repozytorium z dwoma środowiskami: `esp32-c6-zero` i `nrf52840`.
- **ESP32-C6-Zero**: `platform = espressif32@8.0.0`, `board = waveshare_esp32_c6_zero`, `framework = arduino`, nadpisanie flasha `board_upload.flash_size = 8MB`, `monitor_speed = 115200`.
- **nRF52840**: `platform = nordicnrf52@11.0.0`, `framework = arduino`, `monitor_speed = 115200`; board pozostaje placeholderm `adafruit_feather_nrf52840` do czasu identyfikacji modułu i bootloadera.
- Ustawić `default_envs = esp32-c6-zero`, aby domyślny build nie próbował targetu nRF52840.
- Dodać zależność `bmellink/IBusBM` tylko do środowiska ESP32-C6 (wsparcie nRF52840 w tej bibliotece nie jest potwierdzone).
- Poza zakresem: kod źródłowy, własne board defs, biblioteka MS5611, logika telemetrii, okablowanie oraz pełny build firmware (brakuje `src/`; weryfikacja ogranicza się do poprawnego parsowania konfiguracji).

## Capabilities

### New Capabilities

- `platformio-configuration`: wymagania dotyczące środowisk PlatformIO, przypisania platform/boardów, nadpisań specyficznych dla płytki i wspólnych ustawień budowania dla obu targetów.

### Modified Capabilities

Brak.

## Impact

- Nowy plik: `platformio.ini` (katalog główny).
- Brak zmian w istniejącym kodzie — repozytorium nie ma jeszcze źródeł.
- Zależność narzędziowa: PlatformIO Core oraz pakiety platform `espressif32@8.0.0` i `nordicnrf52@11.0.0`.
- Przyszłe zadania (źródła, biblioteki, testy na sprzęcie) będą korzystać z tych dwóch środowisk; zmiana boardu nRF52840 wymaga późniejszej aktualizacji konfiguracji.
