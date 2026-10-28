# platformio-configuration Specification

## Purpose

Definiuje dwa środowiska budowania PlatformIO dla projektu: pierwszy target ESP32-C6-Zero oraz docelowy moduł nRF52840, wraz z ustawieniami wymaganymi przez ich sprzęt.

## Requirements

### Requirement: Dwa niezależne środowiska budowania

Projekt SHALL udostępniać dwa niezależne środowiska budowania firmware, `esp32-c6-zero` i `nrf52840`, z których każde wybiera właściwą platformę oraz board i może być wybrane bez wpływu na drugie. Środowisko `native` MUST pozostać oddzielone od tych dwóch i służyć wyłącznie testom hostowym.

#### Scenario: Oba środowiska są widoczne

- **WHEN** użytkownik odczyta konfigurację projektu PlatformIO
- **THEN** lista środowisk zawiera `esp32-c6-zero` oraz `nrf52840`

#### Scenario: Wybór konfiguracji targetu

- **WHEN** użytkownik odczyta efektywną konfigurację z parametrem `-e esp32-c6-zero` lub `-e nrf52840`
- **THEN** PlatformIO zastosuje platformę i board przypisane wyłącznie do wybranego środowiska

### Requirement: Ustawienia sprzętowe ESP32-C6-Zero

Środowisko `esp32-c6-zero` MUST być skonfigurowane dla płytki Waveshare ESP32-C6-Zero z 8 MB flash i monitorem szeregowym 115200, aby build i upload nie korzystały z domyślnych 4 MB pamięci.

#### Scenario: Nadpisanie rozmiaru flash

- **WHEN** PlatformIO przygotowuje build lub upload dla `esp32-c6-zero`
- **THEN** efektywny rozmiar flash wynosi 8 MB, a nie domyślne 4 MB z board def

#### Scenario: Prędkość monitora szeregowego

- **WHEN** użytkownik uruchomi monitor dla `esp32-c6-zero`
- **THEN** połączenie zostanie otwarte z prędkością 115200 bodów

### Requirement: Środowisko nRF52840 z boardem prowizorycznym

Środowisko `nrf52840` SHALL korzystać z platformy Nordic nRF52 i frameworka Arduino oraz SHALL deklarować board `adafruit_feather_nrf52840` jako prowizoryczny, dopóki moduł i jego bootloader nie zostaną zidentyfikowane.

#### Scenario: Wybór platformy Nordic

- **WHEN** użytkownik buduje środowisko `nrf52840`
- **THEN** PlatformIO używa platformy `nordicnrf52` i frameworka Arduino

#### Scenario: Prowizoryczny board jest jawnie zadeklarowany

- **WHEN** użytkownik sprawdzi konfigurację środowiska `nrf52840`
- **THEN** board to `adafruit_feather_nrf52840`, a konfiguracja wskazuje, że wymaga on zmiany po identyfikacji modułu

### Requirement: Domyślny target budowania

Domyślny build bez wskazania środowiska MUST budować wyłącznie `esp32-c6-zero`, aby prowizoryczny target nRF52840 nie blokował codziennej pracy.

#### Scenario: Build bez parametru -e

- **WHEN** użytkownik uruchomi `pio run` bez wskazania środowiska
- **THEN** PlatformIO zbuduje wyłącznie `esp32-c6-zero`

### Requirement: Poprawność konfiguracji PlatformIO

Plik konfiguracyjny MUST być poprawnie parsowany przez PlatformIO i MUST definiować oba środowiska bez błędów składni lub nieznanych sekcji.

#### Scenario: Walidacja konfiguracji

- **WHEN** użytkownik uruchomi odczyt konfiguracji projektu PlatformIO
- **THEN** polecenie kończy się sukcesem i zwraca konfigurację obu środowisk

### Requirement: Środowisko testów hostowych

Projekt SHALL udostępniać środowisko PlatformIO `native` przeznaczone wyłącznie do testów hostowych z frameworkiem Unity. Uruchomienie `pio test -e native` MUST wykonać testy bez sprzętu i bez frameworka Arduino, a obecność tego środowiska MUST NOT zmieniać domyślnego targetu ani konfiguracji środowisk firmware.

#### Scenario: Testy hostowe bez sprzętu

- **WHEN** użytkownik uruchomi `pio test -e native`
- **THEN** testy kompilują się i wykonują na komputerze gospodarza bez podłączonego ESP32-C6 ani GY-63

#### Scenario: Domyślny target pozostaje bez zmian

- **WHEN** użytkownik uruchomi `pio run` bez wskazania środowiska
- **THEN** PlatformIO nadal buduje wyłącznie `esp32-c6-zero`

#### Scenario: Testy korzystają z Unity

- **WHEN** PlatformIO buduje testy w środowisku `native`
- **THEN** testy używają frameworka Unity i nie wymagają zainstalowanego frameworka Arduino
