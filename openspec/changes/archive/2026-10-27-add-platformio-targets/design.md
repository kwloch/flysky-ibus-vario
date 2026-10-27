# Design

## Context

Repozytorium zawiera na razie tylko `README.md` i katalog `openspec/` — nie ma katalogu `src/` ani żadnych źródeł, więc konfiguracja nie może opierać się na istniejącym kodzie. README (sekcje 3 i 4) ustala targety, platformy, boardy i planowany szkic `platformio.ini`. PlatformIO Core 6.1.19 jest dostępne lokalnie. Moduł nRF52840 nie został jeszcze zidentyfikowany, więc jego board i sposób wgrywania pozostają niepewne. Wymagania opisuje `specs/platformio-configuration/spec.md`.

## Goals / Non-Goals

**Goals:**

- Jeden plik `platformio.ini` w katalogu głównym z dwoma środowiskami: `esp32-c6-zero` i `nrf52840`.
- Powtarzalne wersje platform i wspólne ustawienia bez duplikacji między środowiskami.
- Konfiguracja poprawnie parsowana przez PlatformIO już teraz, bez zależności od kodu źródłowego.

**Non-Goals:**

- Dodawanie `src/`, biblioteki MS5611, logiki i-Bus/MS5611 ani własnego board JSON.
- Wybór partycji flash, trybu uploadu nRF52840 i finalnego boardu nRF52840.
- Pełny compile firmware — wymaga źródeł i należy do kolejnego kroku roadmapy.

## Decisions

### Jeden plik z sekcją wspólną i dwoma środowiskami

Użyć `[platformio]` z `default_envs = esp32-c6-zero`, sekcji `[env]` na wspólne `framework = arduino` i `monitor_speed = 115200`, oraz dwóch sekcji `[env:...]` tylko na różnice (`platform`, `board`, `board_upload.flash_size`, `lib_deps`).

Alternatywa: powielić wszystkie ustawienia w obu środowiskach, jak w szkicu README. Odrzucona, bo wspólne wartości łatwiej rozjechać; `[env]` jest standardowym mechanizmem PlatformIO i został potwierdzony lokalnym `pio project config`.

### Dokładne wersje platform

Przypiąć `espressif32@8.0.0` i `nordicnrf52@11.0.0` zgodnie z researchem w README. Zapewnia to powtarzalny build i zapobiega niespodziankom przy zmianie domyślnej wersji platformy.

Alternatywa: wersje pływające (`espressif32`, `nordicnrf52`). Odrzucona na tym etapie — trudniej odtworzyć działający build.

### ESP32-C6-Zero: board i nadpisanie flash

Użyć `board = waveshare_esp32_c6_zero` i `board_upload.flash_size = 8MB`, ponieważ board def PlatformIO deklaruje 4 MB, a płytka ma 8 MB. `monitor_speed = 115200` wspólne dla obu środowisk.

Nie wybierać teraz `board_build.partitions`. Samo `board_upload.flash_size` rozwiązuje rozjazd uploadu opisany w README; dobór partycji zależy od przyszłego rozmiaru firmware i strategii OTA. Jeśli pierwszy build pokaże zbyt małą partycję aplikacji, dodać `board_build.partitions = default_8MB.csv`.

### nRF52840: board prowizoryczny

Użyć `board = adafruit_feather_nrf52840` jako placeholdera z komentarzem w pliku, że board wymaga zmiany po identyfikacji modułu. Nie ustawiać `upload_protocol` — domyślny dla tego boardu może wymagać bootloadera Adafruit/UF2/DFU; bez bootloadera pozostaje SWD. Decyzję o uploadzie podejmie się po odczytaniu oznaczeń modułu.

Alternatywa: od razu stworzyć custom `boards/*.json`. Odrzucona, bo brakuje danych o pinach, flashu i bootloaderze.

### IBusBM tylko w środowisku ESP32-C6

Dodać `bmellink/IBusBM` wyłącznie do `[env:esp32-c6-zero]`. README potwierdza wsparcie ESP32, ale nie nRF52840; dodanie biblioteki do obu środowisk mogłoby zepsuć build nRF bez potrzeby.

Alternatywa: wspólne `lib_deps` w `[env]`. Odrzucona do czasu potwierdzenia kompatybilności z nRF52.

### Walidacja bez źródeł

Ponieważ nie ma `src/`, weryfikacja w tym change ogranicza się do `pio project config --json-output`: musi zakończyć się sukcesem i pokazać oba środowiska z odziedziczonymi `framework` oraz `monitor_speed`. Faktyczny `pio run` dla obu targetów będzie możliwy po dodaniu minimalnych źródeł w kolejnym zadaniu.

Alternatywa: dodać teraz `src/main.cpp` tylko po to, by smoke test przeszedł. Odrzucona, bo rozszerza zakres poza `platformio.ini` i miesza się z kolejnym punktem roadmapy (obsługa MS5611).

## Risks / Trade-offs

- **Board nRF52840 nie odpowiada fizycznemu modułowi** → build może się udać, ale upload nie. Mitygacja: jawny komentarz-placeholder w `platformio.ini` i aktualizacja po identyfikacji modułu.
- **8 MB flash bez zmiany partycji** → upload zaakceptuje większy obraz, ale tablica partycji nadal może ograniczać aplikację. Mitygacja: sprawdzić przy pierwszym realnym buildzie i w razie potrzeby ustawić `board_build.partitions`.
- **Brak `src/`** → `pio run` nie jest teraz miarodajny; ryzyko, że błąd konfiguracji wyjdzie dopiero później. Mitygacja: statyczna walidacja `pio project config` i wczesny smoke test po dodaniu źródeł.
- **Pobieranie platform przy pierwszym buildzie** → build offline może się nie udać. Mitygacja: jednorazowe pobranie pakietów przed testem na sprzęcie.
- **Kompatybilność IBusBM z ESP32-C6** → biblioteka może wymagać konkretnej wersji lub poprawek. Mitygacja: sprawdzić przy pierwszym buildzie i w razie potrzeby przypiąć wersję.

## Open Questions

- Który dokładnie moduł nRF52840 i jaki bootloader (Adafruit, OpenBootloader, brak)?
- Czy docelowy firmware zmieści się w domyślnej partycji 4 MB, czy od razu potrzebna jest partycja 8 MB?
