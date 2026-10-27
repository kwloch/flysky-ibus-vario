# Spec Delta

## MODIFIED Requirements

### Requirement: Dwa niezależne środowiska budowania

Projekt SHALL udostępniać dwa niezależne środowiska budowania firmware, `esp32-c6-zero` i `nrf52840`, z których każde wybiera właściwą platformę oraz board i może być wybrane bez wpływu na drugie. Środowisko `native` MUST pozostać oddzielone od tych dwóch i służyć wyłącznie testom hostowym.

#### Scenario: Oba środowiska są widoczne

- **WHEN** użytkownik odczyta konfigurację projektu PlatformIO
- **THEN** lista środowisk zawiera `esp32-c6-zero` oraz `nrf52840`

#### Scenario: Wybór konfiguracji targetu

- **WHEN** użytkownik odczyta efektywną konfigurację z parametrem `-e esp32-c6-zero` lub `-e nrf52840`
- **THEN** PlatformIO zastosuje platformę i board przypisane wyłącznie do wybranego środowiska

## ADDED Requirements

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
