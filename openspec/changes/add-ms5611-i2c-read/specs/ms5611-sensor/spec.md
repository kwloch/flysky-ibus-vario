# Spec Delta

## Purpose

Definiuje zachowanie odczytu barometru MS5611 po I²C: inicjalizację czujnika, walidację pamięci kalibracyjnej PROM, kompensowany pomiar ciśnienia i temperatury oraz raportowanie błędów.

## ADDED Requirements

### Requirement: Inicjalizacja czujnika po I²C

System SHALL wykryć MS5611 pod adresem `0x77` na magistrali I²C 400 kHz (SDA GPIO0, SCL GPIO1), zresetować czujnik, odczekać co najmniej 3 ms i odczytać wszystkie 8 słów PROM. Inicjalizacja MUST zakończyć się błędem, jeśli czujnik nie potwierdzi swojego adresu.

#### Scenario: Czujnik obecny i odpowiada

- **WHEN** firmware uruchomi inicjalizację, a MS5611 odpowie pod adresem `0x77`
- **THEN** inicjalizacja kończy się sukcesem, a PROM zostaje odczytany przed pierwszym pomiarem

#### Scenario: Brak czujnika na magistrali

- **WHEN** firmware uruchomi inicjalizację, a żaden czujnik nie potwierdzi adresu `0x77`
- **THEN** inicjalizacja kończy się błędem „czujnik nie wykryty”, a firmware nie udostępnia pomiarów

### Requirement: Walidacja pamięci PROM

System MUST zweryfikować PROM za pomocą CRC4 z datasheetu, porównując obliczoną wartość z dolnymi 4 bitami słowa 7. System MUST odrzucić PROM, jeśli CRC się nie zgadza albo którykolwiek współczynnik C1–C6 ma wartość `0x0000` lub `0xFFFF`.

#### Scenario: Poprawny PROM przechodzi walidację

- **WHEN** odczytany PROM ma zgodne CRC4, a współczynniki C1–C6 są różne od `0x0000` i `0xFFFF`
- **THEN** walidacja kończy się sukcesem i czujnik jest gotowy do pomiaru

#### Scenario: Niezgodne CRC4

- **WHEN** obliczone CRC4 różni się od wartości zapisanej w PROM
- **THEN** inicjalizacja kończy się błędem walidacji PROM i nie są zwracane żadne pomiary

#### Scenario: Współczynnik z pamięci wymazanej

- **WHEN** którykolwiek współczynnik C1–C6 wynosi `0x0000` lub `0xFFFF`
- **THEN** inicjalizacja kończy się błędem nieprawidłowego PROM, nawet jeśli CRC4 jest zgodne

### Requirement: Kompensowany odczyt ciśnienia i temperatury

Po udanej inicjalizacji każdy odczyt SHALL wykonać konwersję D1 (ciśnienie) i D2 (temperatura) z oversamplingiem 4096, odczytać 24-bitowe wyniki i zastosować kompensację pierwszego oraz drugiego rzędu z datasheetu. Wynik MUST być podany jako ciśnienie w paskalach i temperatura w stopniach Celsjusza.

#### Scenario: Odczyt zgodny z przykładem z datasheetu

- **WHEN** PROM i surowe wartości D1/D2 odpowiadają przykładowi z datasheetu MS5611
- **THEN** odczyt zwraca ciśnienie `100009 Pa` i temperaturę `20.07 °C` w tolerancji odpowiednio 1 Pa i 0.01 °C

#### Scenario: Kompensacja w niskiej temperaturze

- **WHEN** obliczona temperatura jest niższa niż `20.00 °C`
- **THEN** odczyt stosuje poprawkę drugiego rzędu i zwraca temperaturę oraz ciśnienie po korekcie

#### Scenario: Błąd podczas konwersji lub odczytu ADC

- **WHEN** zapis komendy konwersji albo odczyt ADC zakończy się błędem I²C
- **THEN** odczyt kończy się błędem i nie udostępnia nowej wartości pomiaru

### Requirement: Kontrola zakresu i raportowanie błędów

System SHALL odrzucić pomiar, którego ciśnienie wykracza poza `10000–120000 Pa` albo temperatura poza `-40.00–85.00 °C`, oraz MUST rozróżniać błąd inicjalizacji od błędu pojedynczego odczytu.

#### Scenario: Pomiar poza zakresem

- **WHEN** skompensowany odczyt daje ciśnienie lub temperaturę poza dopuszczalnym zakresem
- **THEN** odczyt kończy się błędem zakresu i nie udostępnia nowej wartości

#### Scenario: Przejściowy błąd odczytu

- **WHEN** pojedynczy odczyt zakończy się błędem I²C, a kolejna transmisja się powiedzie
- **THEN** firmware zgłasza błąd tylko dla nieudanego odczytu i może kontynuować bez ponownej inicjalizacji

### Requirement: Minimalny program demonstracyjny

Firmware SHALL inicjalizować czujnik przy starcie i logować co około sekundę jedną linię ASCII z czasem w ms, ciśnieniem w Pa i temperaturą w °C, czytelną w `pio monitor`. Błędy MUST być raportowane osobnymi liniami, a program MUST kontynuować działanie z ponawianiem prób zamiast się zawieszać.

#### Scenario: Czujnik podłączony po starcie

- **WHEN** ESP32-C6-Zero uruchomi firmware z podłączonym GY-63
- **THEN** log po USB zawiera co około sekundę nową linię w formacie `MS5611: t=<ms> P=<Pa> Pa T=<°C> C`

#### Scenario: Odczyt w pio monitor

- **WHEN** użytkownik otworzy `pio device monitor -e esp32-c6-zero`
- **THEN** linie pomiarowe są zakończone znakiem nowej linii, zawierają czas, ciśnienie i temperaturę oraz nie zawierają danych binarnych

#### Scenario: Brak czujnika przy starcie

- **WHEN** firmware wystartuje bez odpowiedzi z `0x77`
- **THEN** log zawiera linię `init FAILED <błąd>; retry in 1000 ms`, a program okresowo ponawia próbę i nie blokuje się na stałe

#### Scenario: Błąd pojedynczego odczytu

- **WHEN** czujnik przestanie odpowiadać po udanej inicjalizacji
- **THEN** log zawiera linię `read FAILED <błąd>; continuing`, a program kontynuuje kolejne próby
