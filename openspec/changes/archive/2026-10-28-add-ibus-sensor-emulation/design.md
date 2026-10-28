# Design

## Context

Patrz `proposal.md` — Why. Stan wyjściowy:

- `include/vario_math.h` udostępnia `VarioReading{valid, altitudeCentiM, climbRateCentiMps}`; wartości są już w jednostkach i-Bus (cm i cm/s).
- `src/main.cpp` próbkuje MS5611 co 50 ms, a `sensor.read()` blokuje pętlę na około 20 ms (dwie konwersje OSR 4096 + odczyty I²C). Loguje `VARIO:` co 200 ms i `MS5611:` co 1 s.
- `platformio.ini` przypina `IBusBM` 1.1.4 z poprawką timera dla Arduino core 3.x. Biblioteka ma własny rejestr sensorów, obsługuje komendy `0x8y/0x9y/0xAy` i domyślnie uruchamia `loop()` w przerwaniu timera co 1 ms.
- I²C zajmuje GPIO0/GPIO1. Dla i-Bus wybrano GPIO4 (RX) i GPIO5 (TX).
- Odbiornik odpytuje sensory w odstępach rzędu kilku milisekund; odpowiedź musi zmieścić się przed kolejnym pollem. Blokujący odczyt barometru uniemożliwia obsługę protokołu wyłącznie z głównej pętli.

Wymagania behawioralne znajdują się w `specs/ibus-sensor-emulation/spec.md`.

## Goals / Non-Goals

**Goals:**

- Warstwa kodowania/dekodowania ramek i logika emulacji niezależna od Arduino, testowalna na hoście.
- Dwa wymienne transporty o tym samym zachowaniu sensorów: domyślny `IBusBM` (opcja A) i własny półdupleks (opcja B).
- Odpowiedzi na polling niezależne od blokującego odczytu MS5611.
- Jasne okablowanie i piny, możliwe do zweryfikowania multimetrem i na aparaturze.

**Non-Goals:**

- Inne sensory i-Bus, w tym Pressure `0x41`, GPS Alt `0x82`, Alt Max `0x84`, `0xF9`.
- Wspólna linia z innymi sensorami lub odbiornikami wymagającymi adresów innych niż 1 i 2.
- Target nRF52840, dynamiczny wybór transportu w runtime, konfiguracja z aparatury.
- Zmiany w estymatorze vario, sterowniku MS5611 lub formacie istniejących logów.

## Decisions

### Podział na czysty kodek, emulator i transport

Trzy warstwy:

1. `include/ibus_protocol.h` — czyste funkcje: checksuma, walidacja ramki, budowa odpowiedzi discover/type/value, nasycenie Vspd, tabela sensorów. Zero zależności od Arduino; testy `native` używają jej bez stubów.
2. `include/ibus_emulator.h` — `IbusSensorEmulator` trzymający adresy Alt/Vspd i wywołujący `IbusTransport`. Bez Arduino; testy mogą podstawić transport atrapę.
3. `include/ibus_transport.h` + `src/ibus_transport.cpp` — interfejs `IbusTransport` i dwa adaptery sprzętowe.

Alternatywa: cała logika w `main.cpp`. Odrzucona, bo protokół i nasycenie wartości są podatne na błędy, a bez rozdzielenia nie da się ich przetestować bez odbiornika. Alternatywa: tylko adapter `IBusBM` bez interfejsu. Odrzucona po decyzji o dwóch wariantach okablowania.

Szkic interfejsu:

```cpp
struct IbusCounters {
  uint32_t polls;
  uint32_t responses;
};

class IbusTransport {
 public:
  virtual ~IbusTransport() = default;
  virtual void begin() = 0;
  virtual uint8_t addSensor(uint8_t type, uint8_t length) = 0;
  virtual void setSensorMeasurement(uint8_t address, int32_t value) = 0;
  virtual void poll() = 0;
  virtual IbusCounters counters() const = 0;
};
```

`addSensor` zwraca adres 1-based, zgodnie z protokołem i `IBusBM`.

### Domyślny transport: `IBusBM`, opcja A

`IBusBMTransport` deleguje `begin()`, `addSensor()` i `setSensorMeasurement()` do biblioteki. `begin()` wywołuje `IBus.begin(Serial1, 1, kIbusRxPin, kIbusTxPin)`, czyli używa wbudowanego przerwania timera 1 ms; dzięki temu odpowiedzi nie zależą od głównej pętli. `poll()` nie robi nic poza akumulacją liczników.

Piny: GPIO4 RX, GPIO5 TX, UART `115200 8N1`. TX jest trwale wyjściowy, więc na linii potrzebny jest **~1,2 kΩ w szereg albo dioda 1N4148** (katoda przy MCU), żeby odbiornik mógł ściągnąć linię do zera. To opcja A z README; staje się domyślna, bo biblioteka jest już zależnością i jej ISR rozwiązuje problem terminowości.

Alternatywa: `IBUSBM_NOTIMER` i wywoływanie `loop()` z `main.cpp`. Odrzucona, bo 20 ms blokady I²C powoduje gubienie polli. Alternatywa: własny półdupleks jako domyślny. Odrzucona jako pierwszy krok — mniej kodu i większe ryzyko błędów timingowych; pozostaje jako wariant B do porównania na sprzęcie.

### Alternatywny transport: własny półdupleks, opcja B

`HalfDuplexTransport` używa tego samego UART-u i pinów, ale:

- w spoczynku GPIO5 jest wejściem (wysoka impedancja), więc odbiornik może swobodnie nadawać;
- przed wysłaniem odpowiedzi pin przechodzi w tryb wyjściowy, po `flush()` wraca w wejście;
- odbiornik na GPIO4 widzi również własne bajty; parser ignoruje bajty odebrane w czasie transmisji i resynchronizuje się po przerwie ≥3 ms;
- przetwarza tylko ramki długości 4 B (`len == 1`), tak jak `IBusBM`, aby uniknąć pętli echa;
- uruchamia własne przerwanie timera 1 ms, które woła `poll()` i wysyła odpowiedź, aby nie zależeć od głównej pętli.

Na linii wystarcza **100 Ω w szereg** od GPIO5; GPIO4 podłączony bezpośrednio. To opcja B z README.

Alternatywa: jeden pin dwukierunkowy zamiast GPIO4/GPIO5. Odrzucona, bo utrudnia diagnostykę i nie pasuje do wybranych pinów; wspólna linia z dwoma pinami działa w obu wariantach. Alternatywa: obsługa półdupleksu w głównej pętli. Odrzucona z tego samego powodu co `IBUSBM_NOTIMER`.

### Wybór transportu przy kompilacji

Domyślnie aktywny jest `IBusBM`. Wariant półdupleksowy włącza makro `IBUS_TRANSPORT_HALFDUPLEX=1` (np. przez `build_flags` lub `PLATFORMIO_BUILD_FLAGS`). `main.cpp` wybiera konkretną klasę przez `#if`, ale pracuje wyłącznie na `IbusTransport&`, więc logika emulatora i rejestracja sensorów są wspólne.

Alternatywa: dwa osobne pliki `main`. Odrzucona, bo dubluje integrację i grozi rozjechaniem zachowania. Alternatywa: wybór w runtime przez GPIO/EEPROM. Odrzucona jako niepotrzebna złożoność; wybór dotyczy okablowania, więc jest stały dla danego builda.

### Adresy i identyfikatory sensorów

Rejestrujemy dwa sensory w kolejności: Alt (`type=0x83`, `length=4`) i Vspd (`type=0x09`, `length=2`). `IBusBM::addSensor()` zwraca 1 i 2, więc odbiornik odpytuje adresy `0x81/0x82`, a w odpowiedzi typu otrzymuje `0x83` i `0x09`. Adres protokołu nie jest tym samym co ID sensora — to normalne w i-Bus i nie wymaga osobnych slotów `0x83`/`0x09`.

Alternatywa: własna mapa adresów 1:1 z typem. Odrzucona, bo `IBusBM` i typowe odbiorniki oczekują kolejnych adresów od 1, a typ jest zgłaszany osobno.

### Wartości, znak i nasycenie

- Alt: `altitudeCentiM` (`int32_t`) → cztery bajty little-endian, bez zmiany znaku.
- Vspd: `climbRateCentiMps` (`int32_t`) → nasycenie do `[-32768, 32767]`, następnie dwa bajty little-endian jako `int16_t`.
- Przed `VarioReading.valid == true` oba sensory mają wartość `0`, ale są zarejestrowane od startu, więc „Discover sensors” w aparaturze zawsze je znajdzie.
- Po nieudanym odczycie MS5611 emulator nie jest aktualizowany, więc transport nadal wysyła ostatnią prawidłową wartość.
- Aktualizacja następuje przy publikacji vario (co ~200 ms), czyli spełnia limit 200 ms; szybsze odświeżanie nie wnosi informacji, bo estymator i tak publikuje z tą częstotliwością.

Alternatywa: wysyłanie `0` przy każdym błędzie odczytu. Odrzucona, bo wariometr „spadałby” do zera przy chwilowym błędzie I²C. Alternatywa: Vspd bez znaku. Odrzucona — OpenI6X traktuje `0x09` jako `int16_t` (`AFHDS2A_ID_CLIMB_RATE`), więc opadanie musi być ujemne.

### Kodek ramek

Czyste funkcje budują całą ramkę razem z checksumą:

```text
discover: 04 (80|addr) chkL chkH
type:     06 (90|addr) type size chkL chkH
value 2B: 06 (A0|addr) b0 b1 chkL chkH
value 4B: 08 (A0|addr) b0 b1 b2 b3 chkL chkH
```

Checksuma to `0xFFFF - suma bajtów poprzedzających checksumę`; suma całej ramki wynosi `0xFFFF`. Walidacja odrzuca ramki krótsze niż 4 B, dłuższe niż 0x20 B oraz o niezgodnej checksumie.

Alternatywa: liczenie checksumy w miejscu wysyłki w każdym adapterze. Odrzucona, bo łatwo o rozjazd między wariantami; wspólny kodek jest testowany na hoście.

### Diagnostyka i liczniki

Log startowy: `IBUS: transport=<IBusBM|halfduplex> rx=4 tx=5 alt=0x83 vspd=0x09`. Co ~1 s: `IBUS: t=<ms> poll=<n> resp=<n>`.

`IBusBM` udostępnia liczniki jako `volatile uint8_t`, które się zawijają. Adapter akumuluje różnice modulo 256 przy każdym `poll()` (wołanym często z głównej pętli), dzięki czemu log pokazuje 32-bitowe sumy. Własny transport prowadzi liczniki 32-bitowe bezpośrednio.

Alternatywa: logować surowe `uint8_t`. Odrzucona, bo po kilku sekundach liczby się zawijają i nie da się ocenić, czy odbiornik w ogóle pyta.

### Testy hostowe i weryfikacja

Testy `native` pokrywają:

- checksumę i długości ramek discover/type/value;
- wartości graniczne Alt (`INT32_MIN/MAX`) i Vspd (nasycenie, znak);
- odrzucanie błędnej checksumy i nieznanego adresu;
- emulator z transportem-atrapą: rejestracja dwóch sensorów, zera przed `valid`, aktualizacja po `valid`, zachowanie wartości po błędzie;
- brak zależności od Arduino w warstwie testowanej.

Weryfikacja sprzętowa: build obu wariantów, a następnie FS-iA6B + FS-i6X (OpenI6X 2.3.0), `Delete all sensors` → `Discover new sensors`, obecność Alt i Vspd oraz reakcja na zmianę wysokości. Bez odbiornika wynik pozostaje oczekujący.

Alternatywa: testy jednostkowe samych adapterów. Odrzucona w `native` — wymagają UART-u i przerwań ESP32; ich logika jest cienka, a realnym testem jest aparatura.

## Risks / Trade-offs

- **Blokujący odczyt MS5611 a polling** → przerwanie 1 ms w obu transportach; główna pętla nie odpowiada za timing. Ryzyko: przerwanie UART koliduje z I²C. Mitygacja: krótkie odpowiedzi (4–8 B), timer 1 ms jak w sprawdzonym `IBusBM`, test na sprzęcie.
- **Echo własnej transmisji** → parser przetwarza tylko ramki `len == 1` i resynchronizuje po 3 ms przerwy, jak biblioteka. Mitygacja: testy ramek i smoke test.
- **Przełączanie TX w wysoką impedancję może nie odłączyć peryferium UART** → na ESP32-C6 `pinMode(INPUT)` powinno wyłączyć driver, ale to wymaga potwierdzenia na płytce. Mitygacja: wariant B jest alternatywą, domyślny A korzysta ze sprawdzonej biblioteki; przy problemach wrócić do A.
- **Kierunek diody w opcji A** → zła orientacja nadal blokuje linię. Mitygacja: README opisuje katodę przy MCU; 1,2 kΩ jest prostszym fallbackiem.
- **Adresy 1 i 2 kolidują z innymi sensorami** → odbiornik może obsługiwać tylko jeden łańcuch. Mitygacja: w tej zmianie tylko dwa sensory; inne ID poza zakresem.
- **Vspd poza zakresem `int16_t`** → nasycenie zamiast zawinięcia. Mitygacja: jawny clamp i testy granic.
- **Liczniki `IBusBM` są 8-bitowe** → akumulacja różnic w `poll()`. Mitygacja: 32-bitowe liczniki w logu; przy dłuższym blokowaniu pętli możliwa utrata pojedynczych zawinięć, ale nie wpływa to na działanie sensorów.
- **Odkrywanie sensorów zależy od kolejności rejestracji** → Alt pierwszy, Vspd drugi, adresy 1/2. Mitygacja: rejestracja w `begin()` przed pierwszym pollsem i log adresów.
- **Zależność od niezarchiwizowanych zmian** → `add-ms5611-i2c-read` i `add-relative-altitude-vspd` nie są zarchiwizowane, ale ich kod istnieje. Mitygacja: emulacja korzysta tylko z gotowego `VarioReading`; brak zmian w tych warstwach.

## Migration Plan

1. Dodać czysty kodek i emulator oraz testy hostowe; uruchomić `pio test -e native`.
2. Dodać interfejs transportu i adapter `IBusBM`; zbudować `esp32-c6-zero` i sprawdzić, że istniejące logi działają.
3. Dodać adapter półdupleksowy i flagę wyboru; zbudować oba warianty.
4. Zintegrować `main.cpp`: rejestracja, aktualizacja co 200 ms, logi startowy i liczniki.
5. Zaktualizować README (roadmapa, piny, okablowanie A/B, wybór transportu, sensory).
6. Smoke test na FS-iA6B + FS-i6X; w razie problemów wrócić do domyślnego A.

Wycofanie: usunąć nowe pliki i przywrócić poprzedni `src/main.cpp` oraz `platformio.ini`; `IBusBM` pozostaje nieużywany jak wcześniej. Brak migracji danych.

## Open Questions

- Czy na docelowym egzemplarzu odbiornika odkrywanie zadziała od razu po starcie, czy wymagane będzie `Delete all sensors` przed pierwszym testem? To pytanie proceduralne, nie zmienia projektu.
