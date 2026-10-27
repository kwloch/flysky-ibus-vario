# flysky-ibus-vario

Telemetria **i-Bus (AFHDS 2A)** dla aparatury FlySky z firmware **OpenI6X** — wariometr na **MS5611 (GY-63)**.

Cel projektu: mały, lekki moduł, który wysyła do odbiornika FlySky (np. FS-iA6B) wysokość względną
i prędkość wznoszenia, bez kupowania fabrycznych sensorów.

Status: **przygotowanie** — repo zawiera research (standard, źródła, wybór targetów). Pierwszy na warsztat
idzie **ESP32-C6-Zero**, potem mały moduł **nRF52840**.

---

## 1. Architektura

```
GY-63 (MS5611, I²C 0x77)  →  MCU (ESP32-C6 / nRF52840)  →  UART half-duplex 115200 8N1
                                                          →  odbiornik FlySky (pin "sensor"/i-Bus)
                                                          →  aparatura FS-i6X (OpenI6X) → Discover sensors
```

Moduł **emuluje sensory i-Bus**: odbiornik odpytuje kolejne adresy, a my odpowiadamy typem i wartością.

## 2. Standard i-Bus (to, co trzeba wiedzieć)

**Warstwa linku**

* `115200 8N1`, logika **3,3 V**, wspólna masa z odbiornikiem.
* Linia jest **półdupleksowa** — jedna linia danych obsługuje ramki w obie strony.
* Ramka zawsze: `[len][cmd|addr][...payload...][chk][chk]`, checksumy: `0xFFFF - sum(bytes)`.

**Sekwencja odpytywania sensora** (opis z implementacji oXs, patrz źródła):

1. `0x8y` — odbiornik pyta „czy sensor o adresie `y` istnieje?” → sensor odsyła ten sam bajt (echo).
2. `0x9y` — pytanie o typ → odpowiedź 6 B: `06 9y <type> <size> chk chk`.
3. `0xAy` — żądanie wartości → odpowiedź 6 B (2-bajtowa wartość) lub 8 B (4-bajtowa).

**Identyfikatory sensorów, które faktycznie mapuje OpenI6X**

| ID | Sensor | Format |
|----|--------|--------|
| `0x83` | Alt (wysokość względna) | 4 B, **signed**, metry × 100 |
| `0x09` | Vspd (prędkość wznoszenia) | 2 B, m/s × 100 |
| `0x41` | Pressure | 2 B, Pa |
| `0x82` | GPS Alt | 4 B, signed, m × 100 |
| `0x84` | Alt Max | 4 B, signed, m × 100 |

⚠️ `0xF9` (FlySky „Altitude”, 2 B w metrach) **nie jest mapowany** przez OpenI6X — nie warto go wysyłać.
Pełna lista typów (0x00–0xF9) jest w `oXs_on_RP2040/src/ibus.h` — najlepsza dostępna enumeracja standardu.

**Sprzęt — najczęstszy błąd**

* Pin TX mikrokontrolera **nie może** być podłączony na sztywno do linii i-Bus. Wymagane jest
  **rezystor ~1,2 kΩ** albo **dioda 1N4148** (katoda przy TX MCU) — inaczej odbiornik nie odpyta sensora.
* Zasilanie: 3,3 V dla MS5611 (GY-63 ma własny LDO, ale linie I²C są 3,3 V).
* Po podłączeniu: w aparaturze **Delete all sensors → Discover new sensors** (stare wpisy zostają puste).

## 3. Targety

### A. Waveshare ESP32-C6-Zero (pierwszy)

| Parametr | Wartość |
|----------|---------|
| MCU | ESP32-C6 (RISC-V, 160 MHz, Wi-Fi 6 / BLE 5 / 802.15.4) |
| Flash | 8 MB (board def PlatformIO domyślnie mówi 4 MB — **trzeba nadpisać**) |
| PlatformIO | `platform = espressif32` **8.0.0**, board `waveshare_esp32_c6_zero` |
| Framework | `arduino` (biblioteki i-Bus i MS5611 są Arduino) — alternatywnie `espidf` |
| Interfejsy | USB-C natywne (flash + logi po CDC), I²C, UART |

Piny do potwierdzenia w tym projekcie (na płytce C6-Zero):

| Pin | Funkcja |
|-----|---------|
| GPIO0 | SDA (I²C do GY-63) — potwierdzone w poprzednim projekcie na tej płytce |
| GPIO1 | SCL (I²C) |
| GPIO8 | wbudowana dioda WS2812 (status) |
| GPIO9 | BOOT / reset fabryczny |

UART do i-Bus: dowolne wolne GPIO (half-duplex realizujemy softwarowo / przez UART z rezystorem).

### B. nRF52840 (mały moduł — po identyfikacji)

| Parametr | Wartość |
|----------|---------|
| PlatformIO | `platform = nordicnrf52` **11.0.0** |
| Framework | `arduino` (Adafruit nRF52 core) albo `zephyr` |
| Board def | do ustalenia po odczytaniu oznaczeń modułu |

Uwagi:

* W platformie `nordicnrf52` **nie ma** board def dla popularnych „supermini / nice!nano-podobnych” modułów.
  Najbliższe istniejące: `adafruit_feather_nrf52840` (SoftDevice S140 + bootloader Adafruit),
  `nrf52840_mdk` (tylko Zephyr), `nano33ble`. Dla własnego modułu: custom `boards/*.json`.
* Wgrywanie: **UF2/DFU** działa tylko gdy na module jest bootloader (Adafruit/OpenBootloader);
  bez bootloadera zostaje **SWD** (DAPLink / ST-Link / J-Link).
* nRF52840 ma kilka UART-ów, więc półdupleks i-Bus nie jest problemem; zegar 64 MHz, Cortex-M4F.

## 4. PlatformIO — planowana struktura

```
platformio.ini          # [env:esp32-c6-zero], [env:nrf52840]
boards/                 # tylko jeśli trzeba własny board def (np. moduł nRF52840)
src/
  main.cpp              # init I²C, pętla: odczyt baro → wysokość/vspd → ramki i-Bus
  vario.h / vario.cpp   # MS5611: ciśnienie, temperatura, wysokość względna, filtracja vspd
  ibus.h / ibus.cpp     # emulacja sensorów i-Bus: 0x83 (Alt), 0x09 (Vspd)
include/
lib/
docs/                   # notatki, zrzuty ramek, pomiary
```

Szkic `platformio.ini` (do dostrojenia po pierwszym buildzie):

```ini
[env:esp32-c6-zero]
platform = espressif32@8.0.0
board = waveshare_esp32_c6_zero
framework = arduino
board_upload.flash_size = 8MB
monitor_speed = 115200
lib_deps =
    bmellink/IBusBM

[env:nrf52840]
platform = nordicnrf52@11.0.0
board = adafruit_feather_nrf52840   ; do zmiany po identyfikacji modułu
framework = arduino
monitor_speed = 115200
```

## 5. Gdzie szukać informacji (źródła)

**Standard i-Bus / implementacje**

* [`bmellink/IBusBM`](https://github.com/bmellink/IBusBM) — biblioteka Arduino (servo + sensory), wspiera
  ESP32/AVR/STM32; przykład `examples/Ibus_sensor/Ibus_sensor.ino`. Uwaga: definiuje tylko typy
  `0x00–0x03` i `0x41`, więc Alt/Vspd trzeba wysłać surowym bajtem typu.
* [`mstrens/oXs_on_RP2040`](https://github.com/mstrens/oXs_on_RP2040) — `src/ibus.h` (pełna enumeracja typów),
  `src/ibus.cpp` (half-duplex na PIO, opis komend `0x8y/0x9y/0xAy`). Najlepsza dokumentacja stanu faktycznego.
* [`dgatf/msrc`](https://github.com/dgatf/msrc) — RP2040, multi-sensor telemetrii (i-Bus, MS5611);
  wersja AVR: [`dgatf/msrc_arduino`](https://github.com/dgatf/msrc_arduino).
* [`Yenya/ibus-altitude-sensor`](https://github.com/Yenya/ibus-altitude-sensor) — mały sensor wysokości i vario
  na AVR (BMP180/BMP280 — bez MS5611, ale dobry wzorzec ramek).
* [`OpenI6X/opentx`](https://github.com/OpenI6X/opentx) → `radio/src/telemetry/flysky_ibus.cpp` —
  co aparatura realnie czyta z ramek i-Bus (mapowanie ID → sensor).

**Barometr MS5611**

* Datasheet TE Connectivity MS5611-01BA03 (rozdzielczość 24 bit, kompensacja 2. rzędu, PROM z kalibracją).
* [`RobTillaart/MS5611`](https://github.com/RobTillaart/MS5611) — biblioteka Arduino, I²C `0x77` (lub `0x76`).
* GY-63 = MS5611 + LDO + poziomowanie; zasilanie 3,3–5 V, ale linie I²C na 3,3 V.

**Aparatura**

* FS-i6X z OpenI6X 2.3.0 (menu EN). Odbiornik FS-iA6B — pin `sensor`/i-Bus do telemetrii.
* Po zmianach: `Delete all sensors` → `Discover new sensors`.

## 6. Roadmap

- [x] Research: standard i-Bus, ID sensorów, wybór targetów, źródła
- [ ] Repo + `platformio.ini` na dwa targety
- [ ] Odczyt MS5611 po I²C (ciśnienie + temperatura, sanity check PROM)
- [ ] Wysokość względna + prędkość wznoszenia (filtracja, opóźnienie < ~200 ms)
- [ ] Emulacja sensorów i-Bus: `0x83` i `0x09` na ESP32-C6
- [ ] Test na FS-iA6B + FS-i6X (OpenI6X 2.3.0) — pojawienie się Alt i Vspd
- [ ] Target nRF52840 (po identyfikacji modułu i bootloadera)
- [ ] Obudowa 3D + waga < 5 g

## 7. Znane pułapki

* **PlatformIO board def `waveshare_esp32_c6_zero` mówi 4 MB flash**, a płytka ma 8 MB — bez nadpisania
  `board_upload.flash_size = 8MB` (lub partycji) build/upload potrafi się rozjechać.
* **Brak rezystora/diody na TX** → odbiornik nigdy nie odpyta sensora i w aparaturze nie pojawi się żaden wpis.
* **`0xF9` nie jest mapowane** przez OpenI6X — wysyłanie go nie da wysokości.
* Klon RP2040 użyty wcześniej do testów okazał się uszkodzony (zapis flash trafiał wyłącznie do pierwszej
  strony 256 B) — dlatego ten projekt startuje na ESP32-C6, a nie na RP2040.
