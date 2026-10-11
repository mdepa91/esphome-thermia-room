# thermia_room – kontekst do kontynuacji pracy

Stan na **2026-10-08**: **emulacja czujnika pokojowego działa na pompie**, łącznie z temperaturą zadaną.
Na ESP wgrana wersja po porządkach (OTA 16:43 UTC, `thermia` z GitHuba) – potwierdzone: 21.5 °C → r01/r02, nastawa 16.0 → r03.
Ten plik opisuje stan bieżący. Pełna historia researchu, z błędnymi hipotezami i dowodami: `room_sensor_context.md`.
Opis komponentu po angielsku (podłączenie, protokół, grafiki): `../README.md`.

**Repozytorium (od 2026-10-11):** `/home/marcin/repo/private/esphome-thermia-room` → `github.com/mdepa91/esphome-thermia-room`,
układ jak esphome-thermia: `components/thermia_room/`, `docs/`, `test/`, przykłady i CI. Stara kopia robocza
`/home/marcin/projects/thermia/custom_components/thermia_room` – już nie rozwijać.

## 1. Cel i efekt

Pompa Thermia/Danfoss bez czujnika pokojowego przyjmuje stałe ~20 °C (r01). Zapisu r01 przez EXT nie przyjmuje.
ESP32 emuluje więc czujnik na zaciskach **303/304** i wysyła:
- **temperaturę pokojową** (0.0–31.9 °C co 0.1, z HA, np. z czujnika Zigbee) → pompa: r01/r02;
- **temperaturę zadaną** (domyślnie 16–31.5 °C co 0.5; ramka niesie 5–35) → pompa używa jej jako r03/r04 zamiast swojej HUONE/r32.

| Stan czujnika | Temperatura zadana pompy |
|---|---|
| ramki przyjmowane (brak alarmu, r14 bit 4 = 0) | z czujnika (`setpoint`) |
| alarm czujnika (brak poprawnych ramek przez dłuższy czas) | z ustawień pompy **r32** (HUONE) – wartość awaryjna |

## 2. Instalacja u użytkownika

- ESP32-S3 (DevKitC-1 N16R8V), IP **192.168.100.95** (stałe), sieć budowy przez bramę 192.168.2.6.
- YAML: `/home/marcin/projects/thermia/ostrow-budowa-room-sensor.yaml`. Encje: „Pok. temp. emulowana”
  (`room_temperature`), „Pok. temp. zadana” (`setpoint`). Na tym samym ESP komponent `thermia` (EXT, I2C na GPIO11/12).
- `thermia_room` na **GPIO10** → TLP241A → R2 1.1 kΩ na 303/304 (schemat: `docs/wiring.svg`).
- Działające parametry: `mark_us 60000`, `space_short_us 82000`, `space_long_us 388000`, `frame_gap 4000ms`,
  `polarity_invert false`. To są też domyślne wartości komponentu.
- Inne urządzenia ESPHome w tej sieci: .97 `esp8266-budowa-thermia` (stary EXT), .99 `budowa-sterowanie-falownikiem`.

## 3. Protokół (potwierdzony)

- Bit = **przerwa, potem impuls 60 ms**. Krótka przerwa (82 ms) = surowy 1, długa (388 ms) = surowy 0.
  Bit logiczny = NOT surowy. Pola LSB-first.
- Ramka 29 bitów:
  ```
  start(1)=1 | nastawa×2 (8) | int (8) | dziesiąte (4) | chk (5) | koniec(3)=011
  chk = (int ^ dziesiąte ^ nastawa×2) & 31
  ```
- **Pompa synchronizuje się na ciszy między ramkami.** 2–4 s działa. 0 s (ciągły strumień) daje śmieci (r01 = 242,
  wcześniej 92).
- **Pompa sprawdza sumę kontrolną** i po cichu odrzuca złe ramki. Pojedyncza zła ramka nie daje alarmu, alarm
  pojawia się dopiero po dłuższym braku dobrych ramek.
- Potwierdzenie nastawy: brute force 32 sum dla 19/20/22/23 °C. Pompa przyjęła dokładnie jedną sumę dla każdej
  wartości i ustawiła r03 na tę wartość. Wzór przewidział 22/19/23 przed testem (`room_sensor_context.md` §14g).
- Niesprawdzone na pompie: nastawy x.5 °C (wzór je obejmuje); kierunek pompa → czujnik (wyświetlacz prawdziwego
  czujnika); odstęp między ramkami u prawdziwego czujnika.

## 4. Kod (ten katalog)

| Plik (w `components/thermia_room/`) | Rola |
|---|---|
| `thermia_room_frame.h/.cpp` | enkoder/dekoder ramki (bez zależności od sprzętu) |
| `thermia_room.h/.cpp` | sterownik RMT: ramka z `loop()` po `frame_gap` ciszy; `update()` tylko odświeża encje |
| `thermia_room_number.h/.cpp` | encje `room_temperature` / `setpoint` |
| `__init__.py` | schemat YAML; deklaruje `esp_driver_rmt` (bez tego ESP32 z ESP-IDF się nie kompiluje) |
| `docs/*.svg` | grafiki do README (generowane skryptem Pythona, nie ręcznie) |
| `test/test_room_frame.cpp` | test hosta (1810 sprawdzeń, w tym zrzuty linii i nastawy potwierdzone na pompie) |
| `example.yaml`, `example-with-thermia.yaml` | przykładowe konfiguracje (kompilowane w CI) |

- Zakres nastawy: `setpoint: min_value/max_value`, domyślnie **16–31.5 °C** (2026-10-10: poniżej 16 °C pompa zgłasza
  alarm czujnika – obserwacja użytkownika). Odtworzona z flash wartość spoza zakresu jest przycinana przy starcie;
  ramka technicznie niesie 5–35 °C.
- Nastawa (`thermia_room_setpoint_v2`) i temperatura pokojowa (`thermia_room_room_temp_v1`, od 2026-10-11) są
  zapisywane w flash tylko przy zmianie (po zaokrągleniu), ESPHome grupuje zapisy (`flash_write_interval`, 1 min).
  NaN (np. niedostępny czujnik HA w lambdzie) jest ignorowany. Ustawianie z lambdy: `number.set` – przykład w README.
- 2026-10-08 kod wyczyszczony z eksperymentów: skanu typów, brute force sum, encji bitów startu/końca,
  `short_bit_value`, `setpoint_in_checksum` i trybu legacy bez `frame_gap`. Ich opis i wyniki są w
  `room_sensor_context.md` §14c–§14g. Usuniętego kodu nie ma też w historii gita (repo założone po porządkach).
- Log: DEBUG = jedna linia na ramkę („sending 20.3 °C, setpoint 21.0 °C”), VERBOSE = suma i przebieg `I`/`_I`.

## 5. Budowanie i wgrywanie (bez lokalnego ESPHome – Docker)

```
S=<katalog z YAML i secrets.yaml>
docker run --rm --network host -v $S:/config -v ~/projects/thermia:/proj:ro \
  -v ~/repo/private/esphome-thermia:/thermia:ro -w /config ghcr.io/esphome/esphome:stable \
  run <plik>.yaml --device 192.168.100.95 --no-logs
```
- W kopii YAML do budowania wystarczy zmienić `path: custom_components` na `/proj/custom_components`; `thermia`
  pobiera się z GitHuba (dziesiętne są tam od commita 23d5a65).
- **Przed OTA zatrzymać kontenery z logami** (`docker ps`, `docker stop`). `timeout` na `docker run` zabija tylko
  klienta, a kontener z połączeniem API działa dalej, raz nawet przez 12 h.
- **OTA czasem zawiesza ESP** (2 z ~7 prób): transfer staje, ESP przestaje odpowiadać nawet na ping, pomaga
  odłączenie zasilania. Działające firmware nie jest naruszane. Przyczyna nieznana; podejrzenie: przerwania RMT /
  programowego I2C (`thermia`) podczas kasowania flash. Pomysł: wstrzymywać obie transmisje w `ota on_begin`.
- Gotowy obraz do USB (wersja ze skanem, już nieaktualna): `../../firmware/`.

## 6. Otwarte sprawy

1. ~~Push dziesiętnych esphome-thermia~~ – zrobione przez użytkownika (commit 23d5a65, 2026-10-08).
2. Sprawdzić na pompie nastawę x.5 (np. 21.5).
3. Przyczyna zawieszeń przy OTA (pkt 5).
4. ~~Zapisywanie temperatury pokojowej w flash~~ – zrobione 2026-10-11.
5. Opcjonalnie: podsłuch kierunku pompa → czujnik (PC817 na wolny GPIO). Do działania nie jest potrzebny.
6. Okazjonalne 92 °C z czasów `frame_gap` 2 s – przy 4 s nie obserwowano. Obserwować dalej.

## 7. Zasady pracy z użytkownikiem

- **Nie zmieniać `name:` encji w YAML.** ESPHome wylicza z niej `object_id`, a HA z niego `unique_id`, więc zmiana
  nazwy oznacza nową encję i utratę historii. Wyświetlane nazwy zmienia się w HA. Sprawdzone w źródłach ESPHome
  2026.9.1 (`core/entity_helpers.py`): nie ma opcji ustawienia ID osobno.
- Komentarze w YAML użytkownika po polsku (bez polskich znaków w starszych miejscach), kod i README po angielsku.
- Commity: format `<JIRA-TASK>: opis`, bez stopek o AI (CLAUDE.md użytkownika).
- Przed zdalnym OTA upewnić się, że łącze jest stabilne. Zawieszenie ESP oznacza dla użytkownika wizytę przy pompie.

## 8. Źródła

- Blog rainisto: https://omakotikotitalomme.blogspot.com/2024/01/danfoss-sisalampotilasensorin.html (kodowanie,
  zrzuty 20.1/20.2, tabela, debug 20.5). Pod wpisem: Kalle Högegård (2026-03), ten sam problem z nastawą 21.
- maalampofoorumi.fi topic 2246/4474: oryginalny czujnik 086U6003 (LCD, przyciski, nastawa co 0.5 °C;
  Thermia = Danfoss; ok. 100–200 €; JS Energi 3241 SEK).
- ThermIQ (thermiq.net, github.com/ThermIQ/thermiq_mqtt-ha): ROOM2 wysyła tylko `INDR_T`, nastawa przez d050/r32.
- Instrukcja Danfoss DHP (FI): HUONEANTURI w HUOLTO → ASENNUS → JÄRJESTELMÄ; HUONEKERROIN 0–4.
