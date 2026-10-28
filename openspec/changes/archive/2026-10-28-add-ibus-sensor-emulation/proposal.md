# Proposal

## Why

Firmware liczy już wysokość względną i filtrowaną prędkość wznoszenia, ale nie wysyła ich do odbiornika FlySky — w aparaturze nie pojawia się żaden sensor, więc wariometr pozostaje bezużyteczny w locie. Potrzebna jest emulacja sensorów i-Bus `0x83` (Alt) i `0x09` (Vspd) na ESP32-C6, wraz z jasno wybranym sposobem podłączenia półdupleksowej linii.

## What Changes

- Dodać czystą, testowalną warstwę protokołu i-Bus: budowanie i parsowanie ramek `0x8y`/`0x9y`/`0xAy`, checksumy `0xFFFF - sum(bytes)`, rejestr sensorów oraz konwersję wartości na bajty.
- Zarejestrować dwa sensory: Alt `0x83` jako 4-bajtową liczbę ze znakiem w cm oraz Vspd `0x09` jako 2-bajtową liczbę ze znakiem w cm/s. Wartości przed uzyskaniem referencji wysyłać jako zero.
- Ograniczyć Vspd do zakresu `int16_t` (cm/s) przed wysłaniem; Alt pozostawić jako `int32_t` (cm).
- Dodać abstrakcję transportu z dwiema implementacjami wybieranymi przy kompilacji:
  - domyślnie `IBusBM` (opcja A: TX trwale wyjściowy, ~1,2 kΩ szeregowo lub dioda 1N4148, obsługa pollingu w przerwaniu timera),
  - alternatywnie własny półdupleks (opcja B: 100 Ω, TX przełączany w stan wysokiej impedancji poza transmisją).
- Użyć UART `115200 8N1` na GPIO4 (RX) i GPIO5 (TX); w wariancie półdupleksowym oba piny podłączyć do tej samej linii i wyłączać odbiór własnych bajtów.
- Zintegrować emulację z `src/main.cpp`: rejestracja sensorów przy starcie, aktualizacja wartości z `VarioReading` co ~200 ms, zachowanie dotychczasowych logów `VARIO:` i `MS5611:` oraz dodanie logu startowego i liczników polli/odpowiedzi.
- Dodać testy hostowe (Unity, `native`) dla ramek, checksum, rejestru, znaku i nasycenia wartości.
- Zaktualizować `README.md`: oznaczyć punkt roadmapy, opisać oba warianty okablowania, wybór transportu, piny UART, ID sensorów i sposób weryfikacji w aparaturze.
- Poza zakresem: inne sensory i-Bus (np. Pressure `0x41`, GPS Alt `0x82`, Alt Max `0x84`, `0xF9`), target nRF52840, konfiguracja sensorów po stronie aparatury, automatyczne przełączanie transportu w runtime oraz zmiany w estymatorze vario.

## Capabilities

### New Capabilities

- `ibus-sensor-emulation`: emulacja sensorów i-Bus Alt (`0x83`) i Vspd (`0x09`) — odpowiedzi na discover/type/value, formaty i jednostki, rejestracja sensorów, aktualizacja wartości, wybór transportu i okablowanie półdupleksowej linii.

### Modified Capabilities

Brak — zmiana korzysta z istniejącego wyjścia `altitude-vario` (cm i cm/s) i nie zmienia jego wymagań ani wymagań `platformio-configuration`. Dodanie opcjonalnego flagu kompilacji do `platformio.ini` jest środkiem implementacji, nie zmianą zachowania środowisk budowania.

## Impact

- Nowe pliki: `include/ibus_protocol.h` (czysty kodek i rejestr), `src/ibus_transport.h` / `src/ibus_transport.cpp` (interfejs i dwa adaptery), `test/test_ibus_protocol/` (testy hostowe).
- Zmieniane pliki: `src/main.cpp` (integracja, piny, logi), `platformio.ini` (flag wyboru transportu, domyślnie IBusBM), `README.md` (roadmapa, okablowanie, piny, sensory).
- Brak nowych zależności runtime — domyślny transport korzysta z już przypiętego `IBusBM`; wariant półdupleksowy nie dodaje bibliotek.
- Wymaga fizycznego FS-iA6B + FS-i6X (OpenI6X 2.3.0) do finalnego smoke testu; bez odbiornika weryfikacja ogranicza się do testów hostowych i buildów obu wariantów.
- Zmiana zakłada działający sterownik MS5611 i estymator vario z wcześniejszych, jeszcze niezarchiwizowanych zmian.
