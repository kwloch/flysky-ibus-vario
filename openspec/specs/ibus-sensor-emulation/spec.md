# ibus-sensor-emulation Specification

## Purpose

Umożliwia odbiornikowi FlySky odkrycie i odczyt dwóch sensorów telemetrycznych — wysokości względnej i prędkości wznoszenia — dzięki emulacji protokołu i-Bus na półdupleksowej linii UART.

## Requirements

### Requirement: Zestaw emulowanych sensorów

Urządzenie SHALL emulować dokładnie dwa sensory i-Bus: Alt o typie `0x83` i Vspd o typie `0x09`, niezależnie od wybranego transportu. Inne typy sensorów MUST NOT być zgłaszane odbiornikowi.

#### Scenario: Alt jest zgłaszany jako 4-bajtowy

- **WHEN** odbiornik zapyta o typ pierwszego sensora
- **THEN** odpowiedź zawiera typ `0x83` i rozmiar danych `4`

#### Scenario: Vspd jest zgłaszany jako 2-bajtowy

- **WHEN** odbiornik zapyta o typ drugiego sensora
- **THEN** odpowiedź zawiera typ `0x09` i rozmiar danych `2`

#### Scenario: Brak dodatkowych sensorów

- **WHEN** odbiornik odpyta adresy spoza zarejestrowanych dwóch sensorów
- **THEN** urządzenie nie odpowiada ramką typu ani wartości

### Requirement: Kodowanie wysokości względnej

Odpowiedź wartości Alt SHALL kodować wysokość w centymetrach jako 32-bitową liczbę całkowitą ze znakiem w kolejności bajtów little-endian. Wysokość poniżej referencji MUST być reprezentowana jako wartość ujemna.

#### Scenario: Wysokość dodatnia

- **WHEN** estymator zwraca `altitudeCentiM = 123`
- **THEN** pole wartości Alt zawiera cztery bajty kodujące `123`

#### Scenario: Wysokość ujemna

- **WHEN** estymator zwraca `altitudeCentiM = -45`
- **THEN** pole wartości Alt zawiera cztery bajty kodujące `-45` jako `int32_t`

### Requirement: Kodowanie prędkości wznoszenia

Odpowiedź wartości Vspd SHALL kodować prędkość wznoszenia w centymetrach na sekundę jako 16-bitową liczbę całkowitą ze znakiem w kolejności little-endian. Wartości spoza zakresu `int16_t` MUST zostać nasycone do `-32768` lub `32767`.

#### Scenario: Wznoszenie i opadanie

- **WHEN** estymator zwraca `climbRateCentiMps = 250` lub `-250`
- **THEN** pole wartości Vspd zawiera dwa bajty kodujące odpowiednio `250` lub `-250` jako `int16_t`

#### Scenario: Nasycenie zakresu

- **WHEN** estymator zwraca wartość większą niż `32767` lub mniejszą niż `-32768`
- **THEN** wysyłana wartość Vspd wynosi odpowiednio `32767` lub `-32768`

### Requirement: Odpowiedzi na odpytywanie sensorów

Urządzenie SHALL odpowiadać na każdą prawidłową ramkę odpytywania: `0x8y` odesłaniem tego samego bajtu, `0x9y` ramką typu i rozmiaru, a `0xAy` ramką bieżącej wartości. Każda odpowiedź MUST mieć długość zgodną z protokołem i checksumę, dla której suma wszystkich bajtów ramki wynosi `0xFFFF`.

#### Scenario: Echo discover

- **WHEN** odbiornik wyśle prawidłową ramkę `0x8y` dla zarejestrowanego adresu
- **THEN** urządzenie odsyła 4-bajtową ramkę z tym samym bajtem `0x8y` i poprawną checksumą

#### Scenario: Odpowiedź typu

- **WHEN** odbiornik wyśle prawidłową ramkę `0x9y` dla zarejestrowanego adresu
- **THEN** urządzenie odsyła 6-bajtową ramkę z typem, rozmiarem i poprawną checksumą

#### Scenario: Odpowiedź wartości

- **WHEN** odbiornik wyśle prawidłową ramkę `0xAy` dla zarejestrowanego adresu
- **THEN** urządzenie odsyła ramkę wartości o długości 6 B dla sensora 2-bajtowego albo 8 B dla sensora 4-bajtowego, z poprawną checksumą

### Requirement: Odporność na nieprawidłowe ramki

Urządzenie SHALL ignorować ramki z błędną checksumą, nieoczekiwaną długością lub nieznanym adresem i MUST NOT odpowiadać na nie. Kolejne prawidłowe odpytywanie MUST być obsłużone normalnie.

#### Scenario: Błędna checksuma

- **WHEN** odbiornik wyśle ramkę odpytywania z niepoprawną checksumą
- **THEN** urządzenie nie wysyła odpowiedzi, a następna prawidłowa ramka jest obsłużona

#### Scenario: Nieznany adres

- **WHEN** odbiornik wyśle ramkę `0xAy` dla adresu, który nie został zarejestrowany
- **THEN** urządzenie nie wysyła odpowiedzi

### Requirement: Wartości przed gotowością estymatora

Przed uzyskaniem ważnego odczytu vario urządzenie SHALL odpowiadać wartościami `0` dla obu sensorów, aby pozostały one odkrywalne. Po uzyskaniu ważnego odczytu wartości MUST być aktualizowane nie rzadziej niż co 200 ms. Nieudany lub poza-zakresowy odczyt barometru MUST pozostawić ostatnią prawidłową wartość.

#### Scenario: Start przed referencją

- **WHEN** estymator nie ma jeszcze referencji, a odbiornik odpytuje o wartość
- **THEN** oba sensory zwracają `0`

#### Scenario: Aktualizacja po nowym odczycie

- **WHEN** pojawi się nowy prawidłowy odczyt vario
- **THEN** kolejna odpowiedź wartości odzwierciedla go w ciągu maksymalnie 200 ms

#### Scenario: Błąd odczytu zachowuje wartość

- **WHEN** odczyt MS5611 zakończy się błędem
- **THEN** wysyłane wartości Alt i Vspd pozostają takie jak przy ostatnim prawidłowym odczycie

### Requirement: Wybór transportu i okablowanie

Projekt SHALL udostępniać dwa wymienne transporty o identycznym zachowaniu sensorów: domyślny oparty na `IBusBM` (TX trwale wyjściowy; wymagany rezystor ~1,2 kΩ szeregowo albo dioda 1N4148) oraz alternatywny własny półdupleks (TX w wysokiej impedancji poza transmisją; wystarcza 100 Ω). Transport MUST być wybierany przy kompilacji, a brak jawnego wyboru MUST wybierać wariant `IBusBM`.

#### Scenario: Domyślny transport

- **WHEN** firmware zostanie zbudowany bez flagi wyboru transportu
- **THEN** aktywny jest transport `IBusBM` z wymaganym zabezpieczeniem TX w postaci ~1,2 kΩ lub diody

#### Scenario: Alternatywny półdupleks

- **WHEN** firmware zostanie zbudowany z flagą wyboru transportu półdupleksowego
- **THEN** aktywny jest własny transport przełączający TX w wysoką impedancję poza transmisją, a odpowiedzi sensorów pozostają identyczne

### Requirement: Parametry linii i terminowość odpowiedzi

Transport SHALL komunikować się z odbiornikiem przez UART `115200 8N1` na GPIO4 (RX) i GPIO5 (TX). Odpowiedzi MUST być wysyłane także wtedy, gdy główna pętla wykonuje blokujący odczyt MS5611, tak aby odbiornik nie tracił polli z powodu próbkowania barometru.

#### Scenario: Odczyt barometru nie blokuje odpowiedzi

- **WHEN** trwa odczyt MS5611, a odbiornik wysyła ramkę odpytywania
- **THEN** urządzenie odpowiada na nią, a licznik odebranych polli rośnie

#### Scenario: Parametry UART

- **WHEN** transport zostanie zainicjalizowany
- **THEN** używa 115200 bodów, 8 bitów danych, bez parzystości i 1 bitu stopu na GPIO4/GPIO5

### Requirement: Diagnostyka emulacji

Firmware SHALL logować przez port diagnostyczny wybrany transport i piny przy starcie oraz liczniki odebranych polli i wysłanych odpowiedzi około raz na sekundę. Dotychczasowe linie `VARIO:` i `MS5611:` MUST pozostać bez zmian.

#### Scenario: Log startowy

- **WHEN** urządzenie startuje
- **THEN** log zawiera nazwę wybranego transportu oraz numery pinów RX i TX

#### Scenario: Liczniki pracy

- **WHEN** odbiornik odpytuje sensory
- **THEN** log co około 1 s pokazuje rosnące liczniki polli i odpowiedzi, a linie `VARIO:` i `MS5611:` nadal się pojawiają
