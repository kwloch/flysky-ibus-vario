# Design

## Context

Istniejący sterownik MS5611 (`src/ms5611.*`, `include/ms5611_math.h`) udostępnia skompensowane `Sample{pressurePa, temperatureCentiC}` i kody błędów. `src/main.cpp` odczytuje czujnik co ~1 s i loguje ciśnienie oraz temperaturę. Zmiana `add-ms5611-i2c-read` jest jeszcze w toku — brakuje jej sprzętowego smoke testu — ale kod sterownika i testy hostowe są już w repozytorium. Wymagania behawioralne opisuje `specs/altitude-vario/spec.md`; motywację — `proposal.md`.

Ograniczenia: ESP32-C6-Zero z Arduino, magistrala I²C 400 kHz, pojedynczy odczyt D1/D2 przy OSR 4096 trwa około 20 ms, testy hostowe działają w środowisku `native` z Unity, a w środowisku deweloperskim nie ma podłączonego czujnika.

Test na nieruchomym GY-63 z pierwszymi gainami (`alpha=0.4`, `beta=0.2`) dał wysokość średnio około -0,55 m i vspd oscylujące w granicach ±2 m/s. To ujawniło dwa problemy: referencję z pojedynczego, odstającego pomiaru oraz zbyt szybki filtr dla realnego szumu MS5611. Ten design opisuje wersję po korekcie: uśrednioną referencję i filtr nastawiony na stabilny vspd kosztem opóźnienia.

## Goals / Non-Goals

**Goals:**

- Czysta, deterministyczna warstwa obliczeń niezależna od Arduino, testowalna na hoście.
- Uśredniona referencja startowa i stabilna wysokość względna w centymetrach.
- Filtrowana prędkość wznoszenia o czasie odpowiedzi ~0,5–1 s, stabilna przy realnym szumie MS5611.
- Zachowanie dotychczasowego logu MS5611 i dodanie logu vario bez zmiany targetów PlatformIO.

**Non-Goals:**

- Emisja ramek i-Bus, ponowne zerowanie w locie, adaptacyjne strojenie filtra.
- Zmiana sterownika MS5611, oversamplingu lub konfiguracji I²C.
- Kompensacja dryfu ciśnienia atmosferycznego i temperatury otoczenia.
- Weryfikacja sprzętowa w tym środowisku.

## Decisions

### Osobny, nagłówkowy moduł `include/vario_math.h`

Dodać czysty nagłówek z funkcją przeliczania ciśnienia na wysokość, stałymi czasowymi i klasą `VarioEstimator`. Nagłówek nie zależy od Arduino, więc testy `native` mogą go użyć bez stubowania `Wire`, tak jak `ms5611_math.h`.

Alternatywa: dopisać wszystko do `ms5611_math.h`. Odrzucona, bo miesza sterownik czujnika z pochodną telemetrią i utrudnia późniejszą wymianę źródła ciśnienia (np. inny barometr). Alternatywa: plik `.cpp` — odrzucona, bo wymagałby włączenia `src/` do testów hostowych albo duplikacji kompilacji; nagłówek jest wystarczająco mały.

Szkic API:

```cpp
constexpr uint32_t kVarioSampleIntervalMs = 50;
constexpr uint32_t kVarioPublishIntervalMs = 200;
constexpr uint32_t kVarioMaxGapMs = 250;
constexpr uint32_t kVarioReferenceSamples = 20;
constexpr float kVarioAlpha = 0.1f;
constexpr float kVarioBeta = 0.02f;

float pressureToAltitudeMeters(float pressurePa, float referencePa);

struct VarioReading {
  bool valid;
  int32_t altitudeCentiM;
  int32_t climbRateCentiMps;
};

class VarioEstimator {
 public:
  void reset();
  bool hasReference() const;
  float referencePressurePa() const;
  void update(int32_t pressurePa, uint32_t dtMs);
  VarioReading reading() const;
};
```

### Wzór barometryczny w `float`, wyjście w centymetrach

Użyć standardowego wzoru `h = 44330 * (1 - (P/P0)^(1/5.255))`. Wewnętrznie liczyć w metrach `float`, a na wyjściu zaokrąglać do centymetrów. Dokładność `float` jest znacznie lepsza niż wymagana tolerancja 0,1 m, a obliczenia raz na 50 ms są tanie nawet bez FPU.

Alternatywa: linearyzacja `h ≈ (P0 - P) / (rho * g)`. Odrzucona, bo przy 1000 m błąd względny jest rzędu kilku procent, a wyświetlana wysokość ma być wiarygodna w całym zakresie lotu. Alternatywa: arytmetyka całkowita — odrzucona, bo wymagałaby aproksymacji `pow` bez zysku przy 20 Hz; testy tolerancyjne wystarczą, by pokryć różnice `powf` między hostem a ESP32.

### Uśredniona referencja startowa

Po inicjalizacji zbierać pierwsze 20 prawidłowych pomiarów ciśnienia (około 1 s przy 20 Hz) i ustawić `P0` na ich średnią. Dopiero wtedy oznaczyć estymator jako gotowy i wyzerować `alt = 0`, `vspd = 0`. Przed wypełnieniem okna nie publikować wysokości. Referencja nie zmienia się przy błędach odczytu ani po przerwie; brak ponownego zerowania w locie.

Test sprzętowy pokazał, że pojedynczy pierwszy pomiar może być odstający o równowartość ~0,5 m. Średnia z 20 próbek zmniejsza ten błąd około czterokrotnie i centruje wysokość w okolicach zera.

Alternatywa: pojedynczy pierwszy pomiar (wersja pierwotna). Odrzucona po teście sprzętowym. Alternatywa: zerowanie komendą po serialu. Odrzucona w tej zmianie — wymaga parsera komend i obsługi stanu; można dodać później bez zmiany matematyki. Alternatywa: stałe ciśnienie nad poziomem morza. Odrzucona, bo „wysokość względna” ma być liczona od miejsca startu.

### Filtr alpha-beta na wysokości

Utrzymywać stan `alt` i `vspd` i aktualizować go predykcją stałej prędkości:

```text
alt_pred = alt + vspd * dt
innov    = alt_meas - alt_pred
alt      = alt_pred + alpha * innov
vspd     = vspd + beta * innov / dt
```

Przy `dt = 50 ms`, `alpha = 0.1`, `beta = 0.02`: symulacja skoku prędkości z 0 do 2 m/s osiąga 90% wartości po około 650 ms, a po 1 s błąd wysokości wynosi około 0,1 m. Przy szumie wysokości o odchyleniu 0,15 m daje to szum vspd rzędu 0,15 m/s RMS — znacznie spokojniejszy niż poprzednie 0,8 m/s. Filtr jest równoważny ustalonemu filtrowi Kalmana dla modelu stałej prędkości, ale prostszy i w pełni deterministyczny w testach.

Alternatywa: kaskadowe EMA wysokości i jej pochodnej. Odrzucona, bo przy tym samym opóźnieniu daje większy szum vspd (różniczkowanie wzmacnia szum, a EMA tylko go częściowo tłumi). Alternatywa: regresja liniowa po oknie ~0,7 s. Odrzucona, bo ma większe opóźnienie grupowe i gorszą odpowiedź na zmianę prędkości. Alternatywa: pełny filtr Kalmana z macierzami — odrzucona jako nadmiarowa; alpha-beta daje ten sam efekt przy dwóch stałych.

Świadomy kompromis: stabilny vspd wymaga dłuższego czasu odpowiedzi. Wersja `0.4/0.2` dawała około 0,8 m/s RMS szumu i nie nadawała się na stole; `0.1/0.02` daje około 0,15 m/s RMS kosztem ~650 ms do 90% odpowiedzi. Dalsze zwiększanie okna poprawiałoby szum, ale jeszcze bardziej opóźniało wariometr.

### Rzeczywisty `dt` i reset po przerwie

`update()` przyjmuje zmierzony czas od poprzedniej prawidłowej próbki. `dt` jest ograniczany do rozsądnego przedziału (np. 25–250 ms), a gdy przekracza 250 ms, filtr jest resetowany do bieżącej wysokości z `vspd = 0` przy zachowaniu referencji. Dzięki temu po chwilowym zaniku czujnika nie powstaje sztuczny skok prędkości.

Alternatywa: stałe `dt = 50 ms`. Odrzucona, bo po przerwie predykcja rozjeżdża się i generuje fałszywe wznoszenie. Alternatywa: reset całkowity z nową referencją. Odrzucona, bo wysokość względna startu musiałaby się zmienić w locie.

### Wyjście całkowitoliczbowe gotowe pod i-Bus

`VarioReading` zwraca wysokość w cm i vspd w cm/s (`int32_t`), zaokrąglone przez `lroundf`. To dokładnie jednostki przyszłych ramek i-Bus Alt (`0x83`, m × 100) i Vspd (`0x09`, m/s × 100), więc kolejna zmiana nie będzie musiała zmieniać warstwy obliczeń ani formatu.

Alternatywa: `float` na wyjściu. Odrzucona, bo i-Bus wymaga liczb całkowitych, a formatowanie w main i tak wymaga zaokrąglenia.

### Harmonogram w `main.cpp` bez blokowania

Użyć `millis()` i trzech terminów: próbka ciśnienia co 50 ms, log vario co 200 ms, log MS5611 co 1000 ms. Terminy przesuwać o interwał od chwili rozpoczęcia próbki lub logu (`next = start + interval`), aby zachować 20 Hz i 5 Hz; jeśli odczyt przekroczy termin, pominąć zaległe sloty i zaplanować kolejną próbkę od bieżącego czasu, aby uniknąć serii zaległych próbek po dłuższej operacji. Odczyt MS5611 trwa ~20 ms, więc mieści się w okresie 50 ms z zapasem ~30 ms.

Alternatywa: timer sprzętowy lub przerwanie. Odrzucona — I²C i `Serial` nie są bezpieczne w przerwaniu, a 20 Hz w pętli głównej wystarcza. Alternatywa: zachować odczyt co 1 s i interpolować. Odrzucona, bo nie spełnia założonego czasu odpowiedzi ~0,7 s.

### Logowanie

Dodać linię:

```text
VARIO: t=1200 alt=1.23 m vspd=0.45 m/s
```

z dwoma miejscami po przecinku i jawnym znakiem dla wartości ujemnych, formatowaną całkowitoliczbowo (bez `%f`). Zachować dotychczasową linię `MS5611: t=<ms> P=<Pa> Pa T=<°C> C` co ~1 s, aby nie zmieniać wymagania „Minimalny program demonstracyjny” z `ms5611-sensor`.

Alternatywa: pojedyncza linia CSV. Odrzucona, bo mniej czytelna w `pio monitor` i wymagałaby zmiany istniejącego formatu. Alternatywa: log vario co 50 ms. Odrzucona — 5 Hz spełnia budżet i nie zaśmieca portu.

### Testy hostowe na syntetycznych sekwencjach

W `test/test_vario_math/test_main.cpp` generować ciśnienie z zadanej wysokości wzorem odwrotnym `P = P0 * (1 - h/44330)^5.255`, aby testować odpowiedź filtra na znane wymuszenia. Pokryć: uśrednioną referencję z 20 próbek i znak wysokości, wzór barometryczny, stan ustalony, rampę 2 m/s (weryfikacja 1,8 m/s po 700 ms i błędu 0,2 m po 1 s), pojedyncze zakłócenie 0,1 m, naprzemienny szum 0,1 m (vspd w ±0,3 m/s), przerwę >250 ms oraz zaokrąglanie cm/cm/s.

Alternatywa: testy sprzętowe z prawdziwym GY-63. Odrzucona jako jedyna forma — wymaga sprzętu, którego tu nie ma; pozostaje smoke test po implementacji.

### Aktualizacja README

Oznaczyć punkt roadmapy „Wysokość względna + prędkość wznoszenia...” jako zrobiony, opisać nowe pliki, format `VARIO:`, jednostki cm/cm/s, parametry filtra (20 Hz, 5 Hz, alpha-beta) i polecenia testów. Nie zmieniać sekcji o i-Bus ani targetach.

## Risks / Trade-offs

- **Szum vspd a opóźnienie** → stabilny vspd wymaga około 0,5–1 s odpowiedzi; kosztem jest wolniejsze pokazywanie zmian wznoszenia. Mitygacja: dobrane gains 0.1/0.02, ewentualne dalsze uśrednianie ciśnienia lub zwiększenie OSR, bez zmiany API.
- **Różnice `powf` host/ESP32** → tolerancja 0,1 m w testach i zaokrąglanie do cm; mitygacja: brak porównań bitowych.
- **Dryf referencji w czasie lotu** (zmiana ciśnienia atmosferycznego, temperatura) → mitygacja: akceptowalny dla krótkich lotów; ponowne zerowanie to przyszła funkcja.
- **Opoźnienie fizyczne barometru i self-heating** → mitygacja: dystans od źródeł ciepła, smoke test na sprzęcie, ewentualne obniżenie OSR.
- **Blokujący odczyt ~20 ms** → mitygacja: 50 ms okresu daje zapas; po przekroczeniu terminu kolejna próbka jest planowana od bieżącego czasu, a przerwa >250 ms resetuje filtr.
- **Realny szum na stole** → pierwszy test sprzętowy pokazał offset referencji i zbyt szumny vspd; mitygacja: uśredniona referencja i wolniejsze gains, a po wgraniu nowej wersji ponowny test na nieruchomym czujniku.
- **Zależność od niezarchiwizowanej zmiany MS5611** → sterownik istnieje, ale jego smoke test wisi; mitygacja: nowa zmiana korzysta tylko z istniejącego API i nie modyfikuje sterownika.

## Migration Plan

Brak migracji danych. Wdrożenie: dodać nagłówek i testy, zintegrować `main.cpp`, zaktualizować README, uruchomić `pio test -e native` i `pio run -e esp32-c6-zero`, następnie smoke test na sprzęcie. Wycofanie: usunąć nowe pliki i przywrócić poprzedni `src/main.cpp`; sterownik MS5611 pozostaje bez zmian.

## Open Questions

- Czy ~700 ms czasu odpowiedzi vspd będzie akceptowalne w locie, czy trzeba jeszcze wolniej i stabilniej?
- Czy po retune potrzebne będzie dodatkowe uśrednianie ciśnienia, czy pojedynczy odczyt OSR 4096 wystarczy?
- Czy w przyszłości dodać ponowne zerowanie wysokości (komenda/GPIO), czy wystarczy zerowanie przy starcie?
