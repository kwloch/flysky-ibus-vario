# Tasks

## 1. Konfiguracja PlatformIO

- [x] 1.1 Utworzyć `platformio.ini` w katalogu głównym z sekcją `[platformio]` (`default_envs = esp32-c6-zero`) oraz sekcją `[env]` zawierającą `framework = arduino` i `monitor_speed = 115200`; zweryfikować poleceniem `pio project config --json-output`, że plik jest parsowany i oba środowiska dziedziczą te wartości.
- [x] 1.2 Dodać `[env:esp32-c6-zero]` z `platform = espressif32@8.0.0`, `board = waveshare_esp32_c6_zero`, `board_upload.flash_size = 8MB` i `lib_deps = bmellink/IBusBM`; zweryfikować w `pio project config --json-output`, że platforma, board i nadpisanie flash są widoczne, a `framework` oraz `monitor_speed` są odziedziczone.
- [x] 1.3 Dodać `[env:nrf52840]` z `platform = nordicnrf52@11.0.0`, `board = adafruit_feather_nrf52840` i komentarzem, że board jest prowizoryczny do czasu identyfikacji modułu; zweryfikować w `pio project config --json-output`, że platforma i board są widoczne, a ustawienia wspólne są odziedziczone.
- [x] 1.4 Zaktualizować `README.md`: oznaczyć punkt roadmapy „Repo + platformio.ini na dwa targety” jako zrobiony i wskazać, że źródłem prawdy jest `platformio.ini` w katalogu głównym; zweryfikować, że README nie przedstawia już szkicu jako jedynej konfiguracji.

## 2. Weryfikacja integracyjna

- [x] 2.1 Uruchomić `pio project config --json-output` i sprawdzić, że wynik zawiera oba środowiska, `default_envs` wskazuje tylko `esp32-c6-zero`, ESP32 ma flash 8 MB, a oba środowiska mają `monitor_speed = 115200`; potwierdzić w notatce, że pełny `pio run` jest odroczony do czasu dodania `src/`.
