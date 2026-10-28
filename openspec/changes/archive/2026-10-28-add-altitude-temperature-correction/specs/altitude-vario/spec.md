# Spec Delta

## MODIFIED Requirements

### Requirement: Wysokość względna i punkt odniesienia

System SHALL wyznaczyć ciśnienie i temperaturę odniesienia jako średnie z pierwszego okna 20 prawidłowych pomiarów po udanej inicjalizacji MS5611 (około 1 s przy 20 Hz) i raportować wysokość względem nich w centymetrach ze znakiem. W chwili wyznaczenia referencji wysokość MUST wynosić 0, a obie referencje MUST pozostawać stałe do restartu urządzenia. Do średniej temperatury wchodzą wyłącznie prawidłowe temperatury; przy braku prawidłowej temperatury system MUST użyć 288,15 K.

#### Scenario: Zerowanie po oknie referencyjnym

- **WHEN** po udanej inicjalizacji MS5611 upłynie okno 20 prawidłowych pomiarów (około 1 s)
- **THEN** średnia z tych pomiarów staje się referencją ciśnienia i temperatury, a raportowana wysokość i prędkość wznoszenia wynoszą 0

#### Scenario: Wysokość powyżej i poniżej referencji

- **WHEN** ciśnienie jest niższe niż referencyjne
- **THEN** raportowana wysokość jest dodatnia, a gdy ciśnienie jest wyższe niż referencyjne — ujemna

#### Scenario: Brak referencji przed pierwszym pomiarem

- **WHEN** inicjalizacja MS5611 nie powiodła się albo okno referencyjne nie zostało jeszcze wypełnione
- **THEN** system nie raportuje wysokości, a pomiary nieudane lub poza zakresem nie wchodzą do średniej referencyjnej

#### Scenario: Brak prawidłowej temperatury w oknie

- **WHEN** w oknie referencyjnym żaden pomiar nie ma prawidłowej temperatury, ale ciśnienia są prawidłowe
- **THEN** referencja ciśnienia jest wyznaczana z prawidłowych ciśnień, a do przeliczania wysokości używana jest temperatura standardowa 288,15 K

### Requirement: Przeliczanie ciśnienia na wysokość

System SHALL przeliczać ciśnienie na wysokość względem referencji według wzoru `h = 44330 * (T_ref_K / 288.15) * (1 - (P/P_ref)^(1/5.255))` metrów, gdzie `P_ref` jest ciśnieniem odniesienia, a `T_ref_K` temperaturą odniesienia w kelwinach. Gdy temperatura odniesienia jest niedostępna lub poza zakresem `-40..85 °C`, system MUST użyć `T_ref_K = 288.15 K`. Wynik MUST być zgodny z tym wzorem w tolerancji 0,1 m dla ciśnień w zakresie pomiarowym MS5611.

#### Scenario: Ciśnienie równe referencji

- **WHEN** zmierzone ciśnienie jest równe ciśnieniu odniesienia
- **THEN** wysokość względna wynosi 0 m

#### Scenario: Znana różnica ciśnień

- **WHEN** `P_ref = 101325 Pa`, `T_ref = 15 °C`, a zmierzone ciśnienie wynosi `89876 Pa` albo `113927 Pa`
- **THEN** wysokość wynosi odpowiednio około `+1000 m` albo `-1000 m` z tolerancją 0,1 m

#### Scenario: Ciepła referencja startowa

- **WHEN** `P_ref = 101325 Pa`, `T_ref = 25 °C`, a zmierzone ciśnienie wynosi `89876 Pa`
- **THEN** wysokość wynosi około `+1034,7 m` z tolerancją 0,1 m

#### Scenario: Zimna referencja startowa

- **WHEN** `P_ref = 101325 Pa`, `T_ref = 5 °C`, a zmierzone ciśnienie wynosi `89876 Pa`
- **THEN** wysokość wynosi około `+965,3 m` z tolerancją 0,1 m

#### Scenario: Brak prawidłowej temperatury odniesienia

- **WHEN** temperatura odniesienia jest niedostępna albo poza zakresem `-40..85 °C`
- **THEN** system używa `T_ref = 15 °C` i zwraca wysokość zgodną z dotychczasowym wzorem standardowym

## ADDED Requirements

### Requirement: Stabilność wysokości przy zmianie bieżącej temperatury

System MUST wyliczać wysokość względną wyłącznie z temperatury odniesienia. Zmiany bieżącej temperatury MS5611 po wypełnieniu okna referencyjnego MUST NOT zmieniać raportowanej wysokości ani prędkości wznoszenia, jeśli ciśnienie pozostaje stałe.

#### Scenario: Nagła zmiana bieżącej temperatury

- **WHEN** po wypełnieniu okna referencyjnego bieżąca temperatura MS5611 zmieni się o 10 °C, a ciśnienie pozostanie stałe
- **THEN** raportowana wysokość zmienia się o mniej niż 1 cm, a prędkość wznoszenia pozostaje w granicach ±0,05 m/s

#### Scenario: Szum bieżącej temperatury

- **WHEN** bieżąca temperatura MS5611 oscyluje o ±0,5 °C przy stałym ciśnieniu
- **THEN** raportowana prędkość wznoszenia pozostaje w granicach ±0,1 m/s
