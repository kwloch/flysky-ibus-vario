# Spec Delta

## Purpose

Definiuje wysokość względną i filtrowaną prędkość wznoszenia wyliczane z ciśnienia MS5611: referencję startową, wzór barometryczny, filtrację i opóźnienie, częstotliwość publikacji, jednostki oraz odporność na błędy odczytu.

## ADDED Requirements

### Requirement: Wysokość względna i punkt odniesienia

System SHALL wyznaczyć ciśnienie odniesienia z pierwszego prawidłowego pomiaru po udanej inicjalizacji MS5611 i raportować wysokość względem niego w centymetrach ze znakiem. W chwili wyznaczenia referencji wysokość MUST wynosić 0, a referencja MUST pozostawać stała do restartu urządzenia.

#### Scenario: Zerowanie przy starcie

- **WHEN** po udanej inicjalizacji MS5611 pojawi się pierwszy prawidłowy pomiar ciśnienia
- **THEN** ten pomiar staje się referencją, a raportowana wysokość i prędkość wznoszenia wynoszą 0

#### Scenario: Wysokość powyżej i poniżej referencji

- **WHEN** ciśnienie jest niższe niż referencyjne
- **THEN** raportowana wysokość jest dodatnia, a gdy ciśnienie jest wyższe niż referencyjne — ujemna

#### Scenario: Brak referencji przed pierwszym pomiarem

- **WHEN** inicjalizacja MS5611 nie powiodła się albo nie było jeszcze prawidłowego pomiaru
- **THEN** system nie raportuje wysokości, a pomiary nieudane lub poza zakresem nie ustanawiają referencji

### Requirement: Przeliczanie ciśnienia na wysokość

System SHALL przeliczać ciśnienie na wysokość według wzoru `h = 44330 * (1 - (P/P0)^(1/5.255))` metrów, gdzie `P0` jest ciśnieniem odniesienia. Wynik MUST być zgodny z tym wzorem w tolerancji 0,1 m dla ciśnień w zakresie pomiarowym MS5611.

#### Scenario: Ciśnienie równe referencji

- **WHEN** zmierzone ciśnienie jest równe ciśnieniu odniesienia
- **THEN** wysokość względna wynosi 0 m

#### Scenario: Znana różnica ciśnień

- **WHEN** `P0 = 101325 Pa`, a zmierzone ciśnienie wynosi `89876 Pa` albo `113927 Pa`
- **THEN** wysokość wynosi odpowiednio około `+1000 m` albo `-1000 m` z tolerancją 0,1 m

### Requirement: Filtrowana prędkość wznoszenia i opóźnienie

System SHALL raportować prędkość wznoszenia jako przefiltrowaną prędkość zmiany wysokości względnej w centymetrach na sekundę ze znakiem, dodatnią przy wznoszeniu. Dla skoku prędkości z 0 do 2 m/s raportowana wartość MUST osiągnąć co najmniej 1,8 m/s w ciągu 200 ms, a błąd wysokości MUST wynosić nie więcej niż 0,2 m.

#### Scenario: Odpowiedź na rozpoczęcie wznoszenia

- **WHEN** wysokość zaczyna rosnąć ze stałą prędkością 2 m/s od stanu zerowego
- **THEN** po 200 ms raportowana prędkość wznoszenia wynosi co najmniej 1,8 m/s, a raportowana wysokość różni się od rzeczywistej o nie więcej niż 0,2 m

#### Scenario: Tłumienie pojedynczego zakłócenia

- **WHEN** pojedynczy prawidłowy pomiar odpowiada skokowi wysokości o 0,1 m, a kolejne wracają do poprzedniej wartości
- **THEN** raportowana wysokość zmienia się o mniej niż 0,05 m, a raportowana prędkość wznoszenia pozostaje w granicach ±0,5 m/s

#### Scenario: Stała wysokość

- **WHEN** ciśnienie pozostaje stałe po ustanowieniu referencji
- **THEN** raportowana prędkość wznoszenia dąży do 0, a wysokość pozostaje na poziomie referencji

### Requirement: Częstotliwość próbkowania i publikacji

System SHALL próbkować ciśnienie co 50 ms (20 Hz) i publikować wysokość oraz prędkość wznoszenia co 200 ms (5 Hz). Publikacja MUST opierać się na najnowszym prawidłowym pomiarze i MUST spełniać budżet opóźnienia 200 ms.

#### Scenario: Próbkowanie 20 Hz

- **WHEN** MS5611 jest zainicjalizowany
- **THEN** firmware podejmuje próbę odczytu ciśnienia co około 50 ms

#### Scenario: Publikacja 5 Hz

- **WHEN** firmware pracuje z prawidłowymi pomiarami
- **THEN** nowa wartość wysokości i prędkości wznoszenia jest publikowana co około 200 ms

### Requirement: Format logu vario

Firmware SHALL logować co 200 ms jedną linię ASCII `VARIO: t=<ms> alt=<m> m vspd=<m/s> m/s` z dwoma miejscami po przecinku i znakiem, bez danych binarnych. Dotychczasowa linia `MS5611: t=<ms> P=<Pa> Pa T=<°C> C` MUST pozostać logowana co około 1 s.

#### Scenario: Linia vario w monitorze

- **WHEN** użytkownik otworzy `pio device monitor -e esp32-c6-zero`
- **THEN** co około 200 ms pojawia się kompletna linia `VARIO:` z czasem, wysokością w metrach i prędkością wznoszenia w m/s

#### Scenario: Zachowany log ciśnienia i temperatury

- **WHEN** firmware pracuje z prawidłowymi pomiarami
- **THEN** co około 1 s nadal pojawia się linia `MS5611:` z ciśnieniem w Pa i temperaturą w °C

### Requirement: Odporność na błędy odczytu i przerwy

System MUST ignorować nieudane lub poza-zakresowe pomiary i nie aktualizować nimi estymatora. Po przerwie dłuższej niż 250 ms między prawidłowymi próbkami system SHALL wznowić filtrację bez fałszywego skoku prędkości, zachowując dotychczasową referencję.

#### Scenario: Pojedynczy nieudany odczyt

- **WHEN** pojedynczy odczyt MS5611 zakończy się błędem
- **THEN** estymator nie jest aktualizowany, a ostatnia opublikowana wysokość i prędkość wznoszenia pozostają bez zmian

#### Scenario: Przerwa w próbkach

- **WHEN** między dwoma prawidłowymi pomiarami minie więcej niż 250 ms
- **THEN** filtracja wznawia się od nowego stanu bez skoku prędkości większego niż 0,2 m/s i bez zmiany referencji
