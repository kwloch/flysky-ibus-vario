# Spec Delta

## MODIFIED Requirements

### Requirement: Kompensowany odczyt ciśnienia i temperatury

Po udanej inicjalizacji każdy odczyt SHALL wykonać konwersję D1 (ciśnienie) i D2 (temperatura) z oversamplingiem 4096, odczytać 24-bitowe wyniki i zastosować kompensację pierwszego oraz drugiego rzędu z datasheetu wybranego wariantu MS5611 albo MS5607. Wynik MUST być podany jako ciśnienie w paskalach i temperatura w stopniach Celsjusza.

#### Scenario: Odczyt zgodny z przykładem z datasheetu

- **WHEN** zbudowano wariant MS5611, a PROM i surowe wartości D1/D2 odpowiadają przykładowi z datasheetu MS5611
- **THEN** odczyt zwraca ciśnienie `100009 Pa` i temperaturę `20.07 °C` w tolerancji odpowiednio 1 Pa i 0.01 °C

#### Scenario: Odczyt zgodny z wektorem potwierdzonego MS5607

- **WHEN** zbudowano wariant MS5607, a PROM i D1/D2 odpowiadają potwierdzonemu modułowi GY-63
- **THEN** odczyt zwraca ciśnienie `98 206 Pa` i temperaturę `25,41 °C` w tolerancji odpowiednio 1 Pa i 0.01 °C

#### Scenario: Kompensacja w niskiej temperaturze

- **WHEN** obliczona temperatura jest niższa niż `20.00 °C`
- **THEN** wybrany wariant stosuje swoją poprawkę drugiego rzędu i zwraca temperaturę oraz ciśnienie po korekcie

#### Scenario: Błąd podczas konwersji lub odczytu ADC

- **WHEN** zapis komendy konwersji albo odczyt ADC zakończy się błędem I²C
- **THEN** odczyt kończy się błędem i nie udostępnia nowej wartości pomiaru

## ADDED Requirements

### Requirement: Wybór wariantu czujnika ciśnienia

System SHALL wybierać wariant MS5611 albo MS5607 przy kompilacji. Domyślnie MUST używać MS5607 dla potwierdzonego modułu GY-63; `PRESSURE_SENSOR_MS5607=0` MUST wybierać MS5611. W danym buildzie MUST być używany dokładnie jeden wariant, bez automatycznego wykrywania i przełączania w runtime.

#### Scenario: Domyślny wariant MS5607

- **WHEN** firmware jest budowany bez nadpisania wariantu
- **THEN** kompensacja używa wzorów MS5607, a ciśnienie na ~300 m n.p.m. jest zgodne z referencją ~98 kPa

#### Scenario: Wariant MS5611

- **WHEN** firmware jest budowany z `PRESSURE_SENSOR_MS5607=0`
- **THEN** kompensacja używa wzorów MS5611, a wektor datasheetu MS5611 zwraca `100009 Pa`

#### Scenario: Brak automatycznego wykrywania

- **WHEN** firmware startuje z dowolnym PROM
- **THEN** używa wariantu wybranego przy kompilacji i nie przełącza się na drugi wariant w runtime

## REMOVED Requirements

### Requirement: Diagnostyka typu czujnika ciśnienia

**Reason**: Typ czujnika został potwierdzony jako MS5607; diagnostyka `DIAG:` była tymczasowa i nie jest potrzebna w produkcie.

**Migration**: Usunąć linię `DIAG:`, flagę `PRESSURE_SENSOR_DIAGNOSTIC` i surowe D1/D2 z głównej ścieżki. Wybór wariantu odbywa się przez `PRESSURE_SENSOR_MS5607` (domyślnie MS5607).
