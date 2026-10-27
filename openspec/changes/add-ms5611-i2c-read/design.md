# Design

## Context

Repozytorium ma tylko `README.md`, `platformio.ini` i `openspec/` — nie ma jeszcze `src/`, `include/` ani testów. `platformio.ini` definiuje `esp32-c6-zero` (pierwszy target) i prowizoryczny `nrf52840`, a `framework = arduino` oraz `monitor_speed` są dziedziczone z `[env]`. Lokalnie dostępne są PlatformIO 6.1.19 oraz platformy `native` 1.2.1 i `espressif32` 7.0.1, natomiast plik przypina `espressif32@8.0.0`, więc pierwszy build może wymagać pobrania pakietów. W środowisku nie ma podłączonego ESP32-C6 ani GY-63.

README ustala sprzęt: MS5611/GY-63 pod `0x77`, SDA GPIO0, SCL GPIO1, logika 3,3 V. Datasheet MS5611 opisuje reset, PROM, konwersje D1/D2, kompensację pierwszego i drugiego rzędu oraz CRC4. Biblioteka `RobTillaart/MS5611` jest dobrym źródłem odniesienia, ale sprawdza tylko niezerowość PROM i nie wykonuje CRC4. Wymagania behawioralne są w `specs/ms5611-sensor/spec.md` i `specs/platformio-configuration/spec.md`.

## Goals / Non-Goals

**Goals:**

- Samodzielny, przenośny sterownik MS5611 z twardą walidacją PROM i przewidywalną matematyką.
- Rozdzielenie czystych obliczeń od sprzętowego I²C, aby CRC4 i kompensację testować na hoście.
- Minimalny firmware pozwalający sprawdzić czujnik na ESP32-C6-Zero bez wchodzenia w wysokość, vspd i i-Bus.
- Środowisko testowe `native`, które nie zmienia zachowania dwóch środowisk firmware.

**Non-Goals:**

- Wysokość względna, filtracja, prędkość wznoszenia, emulacja i-Bus.
- Wsparcie SPI, nRF52840 na sprzęcie, własny board def.
- Kalibracja offsetów, kompensacja self-heating, zarządzanie energią, dioda statusowa.
- Asynchroniczny lub nieblokujący odczyt — obecny zakres to prosty odczyt synchroniczny.

## Decisions

### Własny sterownik zamiast biblioteki RobTillaart

Napisać sterownik w `src/ms5611.*` i czystą matematykę w `include/ms5611_math.h`. Biblioteka RobTillaart nie realizuje wymaganego CRC4, a jej kompensacja oparta na `float` utrudnia powtarzalne testy hostowe. Własny kod daje też kontrolę nad jednostkami i obsługą błędów.

Alternatywa: cienka warstwa nad RobTillaart wykorzystująca `getProm()` i własne CRC4. Odrzucona, bo nadal zostawia float, niezerowe sprawdzenie PROM i zewnętrzną zależność runtime bez potrzeby.

### Dwie warstwy: czysta matematyka i sterownik I²C

`include/ms5611_math.h` zawiera tylko funkcje niezależne od Arduino: typ `Prom`, `Sample`, CRC4, walidację PROM, kompensację i kontrolę zakresu. `src/ms5611.h/.cpp` zawiera klasę sterownika przyjmującą `TwoWire&` i adres, wykonującą reset, odczyt PROM, konwersje i odczyt ADC.

Szkic API:

```cpp
enum class Ms5611Error : uint8_t {
  Ok, NotDetected, ResetFailed, PromReadFailed, PromCrcMismatch,
  PromInvalid, NotInitialized, ConversionFailed, AdcReadFailed, OutOfRange
};

struct Ms5611Sample {
  int32_t pressurePa;
  int32_t temperatureCentiC;
};

class MS5611 {
 public:
  explicit MS5611(TwoWire& wire, uint8_t address = 0x77);
  Ms5611Error begin();
  Ms5611Error read();
  bool hasSample() const;
  const Ms5611Sample& sample() const;
  const Prom& prom() const;
  Ms5611Error lastError() const;
};
```

Dzięki temu sterownik nie wie o pinach ESP32, a przyszły target nRF52840 może podać inny `TwoWire` i adres. Minimalny `main.cpp` odpowiada za `Wire.begin(0, 1)`, `Wire.setClock(400000)` i logowanie.

Alternatywa: cała logika w jednej klasie z `Wire` na sztywno. Odrzucona, bo utrudnia testy i przeniesienie na nRF52840.

### Kompensacja w liczbach całkowitych

Użyć formuł z datasheetu na `int32_t`/`int64_t`:

- `dT = D2 - C5 * 256`
- `TEMP = 2000 + ((dT * C6) >> 23)` — setne części °C
- `OFF = (C2 << 16) + ((C4 * dT) >> 7)`
- `SENS = (C1 << 15) + ((C3 * dT) >> 8)`
- dla `TEMP < 2000`: `T2 = (dT*dT) >> 31`, `OFF2 = 5*(TEMP-2000)^2/2`, `SENS2 = 5*(TEMP-2000)^2/4`
- dla `TEMP < -1500`: dodatkowo `OFF2 += 7*(TEMP+1500)^2`, `SENS2 += 11*(TEMP+1500)^2/2`
- `TEMP -= T2`, `OFF -= OFF2`, `SENS -= SENS2`
- `P = (((D1 * SENS) >> 21) - OFF) >> 15` — paskale

Wynik to `int32_t` (centi-°C i Pa), a `float` powstaje dopiero w prezentacji. Daje to identyczne wyniki na ESP32 i hoście oraz pozwala porównywać wartości dokładnie w testach.

Alternatywa: `float` jak w RobTillaart. Odrzucona, bo wyniki zależą od zaokrągleń i utrudniają testy granic. Przesunięcia w prawo ujemnych `int64_t` są w C++ implementation-defined; GCC/Clang na obu targetach robią przesunięcie arytmetyczne, a testy pinują oczekiwane wartości.

### Walidacja PROM: CRC4 i sensowność współczynników

Po resecie odczytać słowa `0..7` komendą `0xA0 + 2*i`. Zapisać `crcStored = prom[7] & 0x0F`, wykonać CRC4 na kopii z `prom[7] &= 0xFF00` i porównać. To wariant zgodny z Betaflight/ArduPilot. Dodatkowo odrzucić PROM, gdy którykolwiek z `C1..C6` jest `0x0000` lub `0xFFFF`, co łapie wymazaną pamięć i brak czujnika.

Alternatywa: zerowanie całego `prom[7]` przed CRC. Odrzucona, bo różni się od powszechnie używanych implementacji i mogłaby odrzucać sprawne czujniki.

Alternatywa: tylko niezerowe współczynniki (jak RobTillaart). Odrzucona, bo nie wykrywa przekłamań w PROM.

### Odczyt synchroniczny przy OSR 4096

Sekwencja odczytu: konwersja D1 (`0x48`), oczekiwanie 10 ms, odczyt ADC (`0x00`), konwersja D2 (`0x58`), oczekiwanie 10 ms, odczyt ADC. Pełny pomiar trwa około 20 ms, co przy logowaniu 1 Hz jest bezpieczne i pozostawia zapas dla przyszłego wariometru z opóźnieniem < 200 ms.

Alternatywa: OSR 1024 lub 2048 dla mniejszego self-heatingu (ArduPilot tak robi). Odrzucona na tym etapie, bo priorytetem jest niski szum ciśnienia; OSR można zmniejszyć po pomiarach na sprzęcie bez zmiany API.

Alternatywa: maszyna stanów nieblokująca. Odrzucona, bo komplikuje minimalny main; można ją dodać później, zachowując czystą matematykę.

### Kontrola zakresu i model błędów

Po kompensacji odrzucić wynik, jeśli ciśnienie jest poza `10000–120000 Pa` albo temperatura poza `-40.00–85.00 °C`. `read()` zwraca `OutOfRange` i nie nadpisuje ostatniej poprawnej próbki. `begin()` zwraca błąd kategorii wykrycia/resetu/PROM, a `read()` błąd konwersji/ADC/zakresu. Dzięki temu main może ponowić inicjalizację przy błędzie startowym i kontynuować przy pojedynczym błędzie odczytu.

Alternatywa: wyjątki lub `abort()`. Odrzucona na mikrokontrolerze; main ma pozostać odporny na odłączenie czujnika.

### Format logu dla pio monitor

Logować po USB przy 115200 jako pełne linie ASCII zakończone `\n`, bez danych binarnych i bez częściowych linii. Minimalny zestaw komunikatów:

```text
MS5611: boot addr=0x77 SDA=0 SCL=1 400kHz
MS5611: init OK CRC=0x0B PROM=0001 9CBF 903C 5B15 5AF2 82B8 6E98 123B
MS5611: t=1000 P=100009 Pa T=20.07 C
MS5611: t=2000 read FAILED AdcReadFailed; continuing
MS5611: t=3000 init FAILED NotDetected; retry in 1000 ms
```

Pomiar to jedna linia z czasem `millis()`, ciśnieniem w Pa i temperaturą w °C z dwoma miejscami po przecinku. Linia `init OK` zawiera CRC i zrzut PROM, aby ułatwić zapisanie rzeczywistego PROM jako fixture po podłączeniu sprzętu. Nazwy błędów pochodzą z `Ms5611Error`. Main nie dodaje własnego znacznika daty — `t=` wystarcza do korelacji, a `pio monitor` nie wymaga zmiany `platformio.ini`.

Alternatywa: CSV. Odrzucona, bo jest mniej czytelny w monitorze, a do analizy można później dodać osobną opcję. Alternatywa: filtr czasu PlatformIO (`monitor_filters = time`). Odrzucona, bo zmienia wspólną konfigurację monitora, a czas z firmware jest wystarczający.

### Środowisko testów hostowych

Dodać do `platformio.ini`:

```ini
[env:native]
platform = native
framework =
test_framework = unity
test_build_src = no
```

Puste `framework =` czyści dziedziczone `arduino` z `[env]`; sprawdzono lokalnie, że `pio project config` i `pio test -e native` to akceptują. `test_build_src = no` powoduje, że testy obejmują tylko `test/` i nagłówek `include/ms5611_math.h`, bez potrzeby stubowania `Arduino.h`/`Wire.h`. Domyślny target pozostaje `esp32-c6-zero`.

Alternatywa: testować pełny sterownik na hoście ze stubem `Wire`. Odrzucona, bo wymaga więcej infrastruktury, a ścieżka I²C i tak wymaga weryfikacji na ESP32.

### Zakres testów

Testy hostowe w `test/test_ms5611_math/` obejmują:

- CRC4 dla PROM `C0=0x0001`, `C1..C6` z datasheetu i `C7=0x123B` → oczekiwane CRC `0x0B`; zmiana dowolnego bitu daje niezgodność.
- Odrzucenie `0x0000` i `0xFFFF` w `C1..C6` nawet przy poprawnym CRC.
- Wektor nominalny z datasheetu (`D1=9085466`, `D2=8569150`) → `2007` centi-°C i `100009 Pa`.
- Wektor poniżej 20 °C (`D2=8000000`) → `-62` centi-°C i `95989 Pa`, co ćwiczy poprawkę drugiego rzędu.
- Wektor poniżej -15 °C (`D2=7500000`) → `-2130` centi-°C i `91910 Pa`, co ćwiczy dodatkową gałąź i pozostaje w dozwolonym zakresie.
- Granice kontroli zakresu ciśnienia i temperatury.

Weryfikacja compile: `pio run -e esp32-c6-zero`. Test sprzętowy: wgrać firmware, uruchomić `pio device monitor -e esp32-c6-zero`, podłączyć GY-63, sprawdzić linie `MS5611: t=... P=... T=...` co ~1 s oraz zachowanie po odłączeniu czujnika.

Alternatywa: dodać testy jednostkowe ścieżki I²C z mockiem. Odrzucona jako nieproporcjonalna do prostego sterownika.

### Konfiguracja I²C w main

Stałe `kSdaPin = 0`, `kSclPin = 1`, `kI2cHz = 400000`, `kAddress = 0x77` w `src/main.cpp`. Dzięki temu zmiana płytki lub adresu nie wymaga ruszania sterownika. Jeśli na sprzęcie pojawią się błędy przy 400 kHz, można obniżyć `kI2cHz` do 100000 bez zmiany logiki.

Alternatywa: automatyczne wykrywanie `0x76`/`0x77`. Odrzucona na tym etapie, bo GY-63 wg README używa `0x77`, a dodatkowa ścieżka komplikuje obsługę błędów.

### Aktualizacja README

Po implementacji oznaczyć roadmapowy punkt „Odczyt MS5611 po I²C...” jako zrobiony, dopisać `src/ms5611.*`, `include/ms5611_math.h`, `test/` do struktury, podać `pio test -e native` oraz przykład `pio run -e esp32-c6-zero -t upload` i `pio device monitor -e esp32-c6-zero`. Nie zmieniać opisu targetów ani standardu i-Bus.

## Risks / Trade-offs

- **Brak sprzętu w środowisku** → nie da się potwierdzić odczytu z prawdziwego GY-63. Mitygacja: testy hostowe, build ESP32 oraz jawna instrukcja smoke testu; wynik sprzętowy oznaczyć jako oczekujący.
- **Różne warianty algorytmu CRC4** → możliwe odrzucenie sprawnego czujnika. Mitygacja: zgodność z Betaflight/ArduPilot (`prom[7] &= 0xFF00`) i test z prawdziwym zrzutem PROM przy pierwszym uruchomieniu.
- **Przesunięcia ujemnych wartości** → zachowanie implementation-defined. Mitygacja: GCC/Clang na ESP32 i hoście, testy pinujące wyniki; w razie przenoszenia na inny kompilator użyć jawnego dzielenia z podłogą.
- **OSR 4096 i self-heating** → temperatura może być zawyżona przez ciepło MCU/LDO. Mitygacja: dystans od źródeł ciepła, pomiar porównawczy i ewentualne przejście na OSR 1024/2048.
- **Blokujący odczyt ~20 ms** → w obecnym main nieszkodliwy, ale blokuje inne zadania. Mitygacja: przyszła maszyna stanów lub odczyt asynchroniczny bez zmiany matematyki.
- **400 kHz na długich przewodach** → błędy I²C. Mitygacja: możliwość obniżenia `kI2cHz` do 100 kHz i krótkie połączenia.
- **Brak lokalnej platformy `espressif32@8.0.0`** → pierwszy build może wymagać sieci. Mitygacja: pobrać pakiet przed testem; nie zmieniać przypiętej wersji bez powodu.
- **Fixture CRC pochodzi z obliczeń, nie z fizycznego czujnika** → test może nie wychwycić różnicy w interpretacji słowa 7. Mitygacja: po podłączeniu GY-63 zapisać rzeczywisty PROM jako dodatkowy fixture.

## Open Questions

- Czy po pomiarach na sprzęcie OSR 4096 okaże się wystarczająco stabilny, czy trzeba przejść na 1024/2048 z powodu self-heatingu?
- Czy docelowy moduł GY-63 będzie zawsze pod `0x77`, czy potrzebne będzie wykrywanie `0x76`?
- Czy przyszły wariometr wymaga odczytu nieblokującego, czy 20 ms co 100 ms jest akceptowalne?
