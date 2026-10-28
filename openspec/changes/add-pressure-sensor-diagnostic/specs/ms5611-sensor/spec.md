# Spec Delta

## ADDED Requirements

### Requirement: Diagnostyka typu czujnika ciśnienia

System SHALL w trybie diagnostycznym logować co około 1 s surowe D1/D2 oraz ciśnienie i temperaturę policzone niezależnie wzorami MS5611 i MS5607. Diagnostyka MUST NOT zmieniać wartości `VARIO:`, `MS5611:` ani Alt/Vspd wysyłanych po i-Bus, a po jej wyłączeniu MUST NOT pojawiać się linia `DIAG:`.

#### Scenario: Linia diagnostyczna

- **WHEN** diagnostyka jest włączona, a odczyt czujnika jest prawidłowy
- **THEN** log zawiera co około 1 s linię `DIAG: t=<ms> D1=<raw> D2=<raw> P5611=<Pa> P5607=<Pa> T5611=<°C> T5607=<°C>` z obiema kompensacjami

#### Scenario: Różnica kompensacji MS5611 i MS5607

- **WHEN** ten sam prawidłowy PROM oraz D1/D2 zostaną policzone wzorami MS5611 i MS5607 przy temperaturze powyżej 20 °C
- **THEN** ciśnienie z kompensacji MS5607 jest około dwukrotnie większe od ciśnienia MS5611, co pozwala rozpoznać niezgodny wariant

#### Scenario: Brak wpływu na vario

- **WHEN** diagnostyka jest włączona i trwa prawidłowy odczyt
- **THEN** `VARIO:`, `MS5611:` oraz Alt/Vspd po i-Bus pozostają liczone istniejącą kompensacją MS5611

#### Scenario: Wyłączenie diagnostyki

- **WHEN** przełącznik diagnostyki jest wyłączony
- **THEN** linia `DIAG:` nie pojawia się, a dotychczasowe logi i emisja i-Bus działają bez zmian

#### Scenario: Rozstrzygnięcie typu czujnika

- **WHEN** użytkownik porówna `P5611` i `P5607` z zewnętrzną referencją (np. Aqara ~97 969 Pa na ~300 m n.p.m.)
- **THEN** kolumna zgodna z referencją wskazuje typ czujnika, a druga kolumna jest około dwukrotnie mniejsza przy niezgodnym wariancie
