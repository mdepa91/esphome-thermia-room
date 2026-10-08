# thermia_room – kontekst do kontynuacji pracy

Stan na **2026-10-07** (wieczór: **emulacja działa na pompie** – patrz §0). Ten plik zbiera wszystko, czego potrzeba, żeby wrócić do tematu bez czytania całej
historii. Szczegółowe dowody i historia: `room_sensor_context.md` (w tym katalogu) (zwłaszcza §11–§13) i
`../../room_sensor/README.md` (sekcja „Stan na 2026-10-07”).

## 0. Najnowsze (2026-10-07 wieczór) – ma pierwszeństwo przed resztą pliku

- Pompa przyjęła emulację (alarm zniknął). Działający YAML: `/home/marcin/projects/thermia/ostrow-budowa-room-sensor.yaml`.
- Ramka ma nowy układ: `start(2)=11 | nastawa(7) | int(8) | dec(4) | chk(5) | koniec(3)=011`,
  `chk = int ^ dec ^ nastawa ^ 0x1F`. „Preambuła” niosła nastawę 21 (stąd stała 21 °C na pompie), „znacznik 111”
  to górne bity int.
- **Temperatura zadana**: przy zaakceptowanym czujniku pompa używa r03 = 21 z czujnika i ignoruje r32; przy alarmie
  czujnika wraca do r32. Pole „nastawy” w ramce musi być 21 (inne wartości = alarm). Opcje `setpoint` i
  `setpoint_in_checksum` zakomentowane. Jak prawdziwy sterownik zmienia nastawę – nieznane (§14b).
- `frame_gap: 2s` działa; `0ms` (ciągły strumień) psuje synchronizację (r01 = 242). Pompa synchronizuje się na
  ciszy i nie sprawdza sumy kontrolnej. Okazjonalne 92 °C (ramka przesunięta o 2 bity) – do obserwacji.
- `~/repo/private/esphome-thermia`: `temp_indoor` (r01+r02) i `temp_indoor_target` (r03+r04) z dziesiątymi – działa na urządzeniu.
- Szczegóły: `room_sensor_context.md` §14. Sekcje 3–6 niżej opisują stary układ pól (bity te same dla nastawy 21).

## 1. Cel

Pompa Thermia/Danfoss nie ma fizycznego czujnika pokojowego i przyjmuje stałe ~20°C jako `temp_indoor` (r01).
Zapis r01 przez port EXT (główny projekt `../thermia`) pompa ignoruje. Dlatego ESP32 ma **emulować czujnik
pokojowy** na zaciskach **303/304** i podawać pompie realną temperaturę (np. z czujnika Zigbee w HA).

## 2. Sprzęt

- Linia 303/304: ~27 V DC w spoczynku, pod obciążeniem ~1 kΩ spada do ~10 V (wewnętrzny rezystor szeregowy pompy
  ~1.7 kΩ). Multimetr: brak odczytu częstotliwości, na AC napięcie skacze (raczej szum/impulsy, nie nośna).
- Zwykły PT1000/rezystor NIE działa – to protokół cyfrowy.
- Stopień nadawczy: ESP32 GPIO25 → R1 220 Ω → **TLP241A** (photoMOS) → R2 1.1 kΩ/1 W obciąża linię 303/304.
  Opcjonalny odczyt: PC817 + R3 4.7 kΩ → GPIO26. TVS P6KE33CA na zaciskach. Schemat i BOM: `room_sensor/README.md`.
- Użytkownik ma ESP podłączone do pompy na wyjściu room sensor. Pierwsze testy (stara 17-bitowa ramka) – **pompa
  nie reagowała**. Pompa pokazuje alarm czujnika pokojowego.

## 3. Protokół (rozszyfrowany, potwierdzony na 3 zrzutach linii)

### Warstwa bitowa – kodowanie odległością impulsów
Każdy bit = **przerwa, potem impuls** (60 ms). Długość przerwy PRZED impulsem koduje bit:
- `I`  = krótka przerwa (80 ms) + impuls → **surowy bit 1** (okres 140 ms)
- `_I` = długa przerwa (~380 ms) + impuls → **surowy bit 0** (okres ~440 ms)

Legenda autora pod tabelą: „MSB oikealla, 1=0 ja 01=1” (symbol `1` = bit logiczny 0, `01` = bit logiczny 1).
Surowy bit = odwrócony bit logiczny.

### Ramka – 29 bitów
```
preambuła(9) | int(5, LSB first) | znacznik(3) | dec(4, LSB first) | chk(5, LSB first) | koniec(3)
110101011    | ...               | 111         | ...               | ...               | 011
```
- **chk = int XOR dec XOR 0b01010** (zgodne z 11 wierszami tabeli autora i zrzutami). Stary wzór `(52-i-d) mod 32`
  był błędny.
- Zakres temperatury 0.0–31.9 °C (5 bitów części całkowitej).
- Kolumna „Start 1 1 1” w tabeli autora = ostatnie 3 bity preambuły (pierwsza długa przerwa nie jest tam
  zapisywana); pierwsze 6 bitów preambuły autor pominął w tabeli, bo się nie zmieniały.

Przykłady (enkoder odtwarza je znak w znak, test hosta to sprawdza):
```
20.1  110101011 | 11010 | 111 | 0111 | 00000 | 011   ____II_II_II_IIIII_II_IIII_IIII_I_I_I_I_I_III
20.2  110101011 | 11010 | 111 | 1011 | 11000 | 011   ____II_II_II_IIIII_II_IIIII_IIIII_I_I_I_III
20.5  110101011 | 11010 | 111 | 0101 | 00100 | 011   ____II_II_II_IIIII_II_IIII_II_II_I_II_I_I_III
```

### Źródła potwierdzenia
- `20.1.png`, `20.2.png` – zrzuty prawdziwego czujnika z bloga (zmierzone pikselowo: 29 impulsów).
- `esp32_implementation.png` – debug string autora `room_temp: 20.5 ____II_II_II_...` (grupy impulsów
  2,2,2,5,2,4,2,2,1,2,1,1,3 = kanał D0).
- `thermiq_table.jpg` – tabela autora (20.1–20.8, 21.9, 22.0–22.2).
- Kursory analizatora: impuls 59.99 ms, okres 140.01 ms, przerwa 80.02 ms. Długi okres wyliczony z proporcji 3.15×.
- Obrazy w pełnej rozdzielczości: podmiana segmentu rozmiaru w URL Bloggera na `/s0/`.

## 4. Kod (ten katalog)

| Plik | Rola |
|---|---|
| `thermia_room_frame.h/.cpp` | czysty enkoder ramki: `FRAME_PREAMBLE`, `FRAME_MARKER`, `FRAME_TAIL`, `CHECKSUM_XOR`, `FRAME_BITS = 29` |
| `thermia_room.h/.cpp` | sterownik RMT (ESP32): przerwa → impuls dla każdego bitu; log `timeline` w notacji `I`/`_I`; stała `SHORT_BIT_VALUE = true` |
| `thermia_room_number.h/.cpp` | encja HA `number` (0.0–31.9, krok 0.1) |
| `__init__.py` | schemat YAML: `pin`, `mark_us`, `space_short_us`, `space_long_us`, `polarity_invert`, `dry_run`, `room_temperature` |

- `short_bit_value` – **wyłączone** (zakomentowane w `__init__.py`, `thermia_room.h`, przykładowym YAML) z
  adnotacją, że może się kiedyś przydać. Jeśli YAML użytkownika ma tę opcję, walidacja ją odrzuci – usunąć.
- `dry_run: true` (domyślne) tylko loguje, nie rusza pinu.
- Ramka trwa 7–8 s → `update_interval: 10s`; gdy poprzednia ramka jeszcze leci, cykl jest pomijany (`busy_`).

Przykładowy YAML: `../../room_sensor/esp32-room-sensor-thermia.example.yaml`:
```yaml
thermia_room:
  pin: GPIO25
  mark_us: 60000
  space_short_us: 80000
  space_long_us: 380000
  polarity_invert: false
  dry_run: true          # false do testu na pompie
  update_interval: 10s
  room_temperature:
    name: "Temperatura pokojowa (emulowana)"
```

### Test hosta
```
cd ~/projects/thermia
g++ -std=c++17 -I custom_components/thermia_room custom_components/thermia_room/thermia_room_frame.cpp \
    test/test_room_frame.cpp -o /tmp/test_room_frame && /tmp/test_room_frame     # 1301/1301
```
Sprawdza: sumę kontrolną na wierszach tabeli, zgodność z debug stringiem 20.5 i zrzutami 20.1/20.2, round-trip
0.0–31.9, clamping.

### Kompilacja (bez lokalnego ESPHome – przez Docker)
Katalog roboczy z `room.yaml` (kopia przykładu z `external_components.path: /proj/custom_components`) i
`secrets.yaml` (`api_key`, `wifi_ssid`, `wifi_password`):
```
docker run --rm -v $PWD:/config -v ~/projects/thermia:/proj:ro -w /config ghcr.io/esphome/esphome:stable compile room.yaml
```
Wynik 2026-10-07 (ESPHome 2026.9.1): sukces, 0 ostrzeżeń w `thermia_room`, RAM 26 %, flash 46.5 %. **Nie
wgrywane.**

## 5. Następny krok – test na pompie

1. W swoim YAML: usuń `short_bit_value:`, ustaw timing z §4, `update_interval: 10s`, `dry_run: false`.
2. Zacznij od **20.5** (bit w bit to, co wysyłała działająca implementacja autora).
3. Log `timeline` dla 20.1 powinien pokazać `II_II_II_IIIII_II_IIII_IIII_I_I_I_I_I_III` (z `____` na początku).
4. **Sukces** = znika alarm czujnika pokojowego / zmienia się r03/r04 (indoor target) przez EXT / na wyświetlaczu
   „HUONE xx (yy)” (aktualna i zadana temperatura).
5. Brak reakcji po ~1 min → `polarity_invert: true`. Dalej: korekta `space_long_us` (wyliczone, nie zmierzone),
   sprawdzenie analizatorem/PC817, czy TLP241A faktycznie zmienia napięcie na linii.

## 6. Otwarte pytania

- **Polaryzacja** – nieznana (u autora impuls = stan wysoki na jego pinie, ale nie wiemy, jak miał transoptor).
- **Preambuła i koniec** – stałe we wszystkich 3 próbkach. Sterownik pokojowy umie też zmieniać temperaturę
  docelową i pokazuje inne sensory. Pierwsze 6 bitów preambuły logicznie = 0,0,1,0,1,0 → LSB-first 20 – może to
  nastawa. Do sprawdzenia dopiero, gdy podstawowa ramka zadziała (zmienić te bity, obserwować r03/r04).
- **Kierunek pompa → sterownik** (wyświetlacz czujnika) – zupełnie nieznany.
- **Odstęp między ramkami** prawdziwego czujnika – nieznany.
- **Menu pompy** – w instrukcji Danfoss DHP: HUOLTO → ASENNUS → JÄRJESTELMÄ → HUONEANTURI (Ø/PÄÄLLÄ);
  HUONEKERROIN (room factor, d60) widoczny tylko z czujnikiem. Użytkownik tej opcji nie widział u siebie; alarm
  czujnika sugeruje, że pompa i tak go oczekuje.
- Nieprzetranskrybowane: 18 wierszy `rn_image_picker.jpg` (21.x/22.x) i kolumny `X`/`.` – dodatkowa weryfikacja.

## 7. Research – co jest w sieci (2026-10-07)

- Blog rainisto (Jonni): https://omakotikotitalomme.blogspot.com/2024/01/danfoss-sisalampotilasensorin.html –
  jedyne źródło protokołu; część 2 z kodem nigdy nie wyszła (komentarz 2025-06: „kod gotowy, brak testów”).
- Komentarz **Kalle Högegård, 2026-03-18** pod tym wpisem: ma działającą emulację na ESP32; po starcie emulacji
  pompa ustawia indoor target r03/r04 = 21.0. Można zapytać o kod/zrzut.
- https://github.com/rainisto/arduino_i2c_orja – tylko EXT/I2C (forki mtornblad, jukkapirinen – bez emulacji).
- https://github.com/MrGlenn-tech/Thermia-ESP32 – ESPHome, `INDR_T` tylko zapisywane („stored only”), brak emulacji.
- ThermIQ-ROOM2 (https://www.thermiq.net) – komercyjny, zamknięty; „2-way protocol”, wykrycie ~30 s, domyślnie 20.5.
- Instrukcja Danfoss DHP (FI): https://www.taloon.com/media/attachments/danfoss/maalampopumppu_danfoss_dhp_asennusohje.pdf
- Bez związku: ryckema/Thermia_itec, thermia-bus-sniffer (RS485/Modbus w iTec), thermiq-bridge (MQTT).

## 8. Historia błędów (dlaczego pierwszy test nie zadziałał)

Stara implementacja różniła się od prawdziwego czujnika w czterech miejscach:
1. 17 bitów zamiast 29 – brak preambuły (9) i końca (3).
2. Znacznik `000` zamiast `111`.
3. Przerwa po impulsie zamiast przed – cała ramka przesunięta o bit.
4. Suma kontrolna `(52-i-d) mod 32` – trafiała tylko część wartości (np. 20.1, 20.5), dla 20.2/20.4/20.6/20.8 źle.

Dodatkowo timing poprawiony z 82/388 ms (piksele) na 80/380 ms (kursory analizatora).
