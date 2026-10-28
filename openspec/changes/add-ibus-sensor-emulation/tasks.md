# Tasks

## 1. Czysty protokół i emulator z testami hostowymi

- [x] 1.1 Dodać `include/ibus_protocol.h`: checksuma `0xFFFF - sum`, walidacja ramki, budowa odpowiedzi discover/type/value (4/6/8 B), nasycenie Vspd do `int16_t`, kodowanie little-endian oraz tabela sensorów z adresami 1-based; zweryfikować, że nagłówek kompiluje się bez Arduino w teście z 1.3.
- [x] 1.2 Dodać `include/ibus_transport.h` z interfejsem `IbusTransport` i `IbusCounters` oraz `include/ibus_emulator.h` z `IbusSensorEmulator` rejestrującym Alt (`0x83`, 4 B) i Vspd (`0x09`, 2 B), wysyłającym zera przed `valid` i aktualizującym wartości po każdym prawidłowym `VarioReading`; zweryfikować przez test z 1.3, że emulator działa z transportem-atrapą.
- [x] 1.3 Dodać `test/test_ibus_emulation/test_main.cpp` z Unity: checksumy i długości ramek, wartości graniczne Alt (`INT32_MIN/MAX`), znak i nasycenie Vspd, odrzucanie błędnej checksumy/nieznanego adresu, rejestracja dwóch sensorów, zera przed `valid`, aktualizacja po `valid` i zachowanie ostatniej wartości po błędzie; zweryfikować przez `pio test -e native`, że wszystkie testy przechodzą.

## 2. Transporty sprzętowe

- [x] 2.1 Dodać `src/ibus_transport.cpp` z adapterem `IBusBMTransport` (domyślnym) delegującym do `IBus.addSensor`/`setSensorMeasurement`, uruchamiającym `IBus.begin(Serial1, 1, kIbusRxPin, kIbusTxPin)`, akumulującym 8-bitowe liczniki biblioteki do 32-bitowych oraz prostą fabryką wybierającą transport makrem; zweryfikować przez `pio run -e esp32-c6-zero`, że firmware się buduje.
- [x] 2.2 Dodać w `src/ibus_transport.cpp` adapter `HalfDuplexTransport`: parser ramek z przerwą ≥3 ms, obsługa tylko `len == 1`, timer 1 ms, przełączanie GPIO5 między wyjściem a wejściem, ignorowanie własnego echa i liczniki 32-bitowe; zweryfikować przez `PLATFORMIO_BUILD_FLAGS="-DIBUS_TRANSPORT_HALFDUPLEX=1" pio run -e esp32-c6-zero`, że wariant półdupleksowy się buduje.

## 3. Integracja firmware i dokumentacja

- [x] 3.1 Zintegrować `src/main.cpp`: utworzyć transport i emulator, zarejestrować sensory przed pierwszą próbką, aktualizować wartości przy publikacji vario co ~200 ms, wołać `poll()` i zachować dotychczasowe logi `VARIO:` oraz `MS5611:`; zweryfikować przez `pio run -e esp32-c6-zero` oraz build z 2.2, że oba warianty się kompilują.
- [x] 3.2 Dodać log startowy `IBUS: transport=<...> rx=4 tx=5 alt=0x83 vspd=0x09` oraz log `IBUS: t=<ms> poll=<n> resp=<n>` co ~1 s; zweryfikować przez build obu wariantów i sprawdzenie w kodzie, że liczniki są 32-bitowe i nie zmieniają formatu istniejących linii.
- [x] 3.3 Zaktualizować `README.md`: oznaczyć punkt roadmapy emulacji i-Bus jako zrobiony, opisać piny GPIO4/GPIO5, oba okablowania (A: ~1,2 kΩ lub dioda; B: 100 Ω), wybór transportu makrem, ID `0x83`/`0x09` i procedurę `Delete all sensors` → `Discover new sensors`; zweryfikować, że README zawiera wszystkie elementy i że podane polecenia buildów są zgodne z `platformio.ini`.

## 4. Weryfikacja integracyjna

- [x] 4.1 Uruchomić `pio test -e native`, `pio run -e esp32-c6-zero`, `PLATFORMIO_BUILD_FLAGS="-DIBUS_TRANSPORT_HALFDUPLEX=1" pio run -e esp32-c6-zero` oraz `pio run` bez `-e`; zweryfikować, że testy przechodzą, oba transporty się budują, a domyślny build nadal wybiera wyłącznie `esp32-c6-zero`.
- [x] 4.2 Uruchomić `openspec validate add-ibus-sensor-emulation --type change --strict`; zweryfikować, że walidacja kończy się bez błędów i ostrzeżeń.
- [ ] 4.3 Wykonać smoke test na sprzęcie (ESP32-C6-Zero + GY-63 + FS-iA6B + FS-i6X z OpenI6X 2.3.0): wgrać domyślny wariant, wykonać `Delete all sensors` → `Discover new sensors` i sprawdzić, że pojawiają się Alt i Vspd; potwierdzić rosnące liczniki `IBUS: poll/resp`, zmienić wysokość czujnika i sprawdzić reakcję Alt/Vspd, a następnie powtórzyć dla wariantu półdupleksowego z rezystorem 100 Ω. Jeśli sprzęt jest niedostępny, zapisać wynik jako oczekujący i nie oznaczać zadania jako zweryfikowane.
  Wynik 4.3: oczekujący — brak dostępu do sprzętu (ESP32-C6-Zero + GY-63 + FS-iA6B + FS-i6X); zadanie pozostaje nieoznaczone jako zweryfikowane.

## Workflow follow-up

- Zarchiwizować zmianę po spełnieniu wymagań przeglądu projektu i po zamknięciu zależności od `add-ms5611-i2c-read` oraz `add-relative-altitude-vspd`.
- Po teście na aparaturze zdecydować, czy domyślnym transportem pozostaje `IBusBM`, czy przełączyć na półdupleks, i zaktualizować README bez zmiany specyfikacji sensorów.
