# Room Sensor (ThermIQ-ROOM2 / Danfoss "sisälämpötilasensori") – dziennik researchu

> Przeniesiony 2026-10-07 z `room_sensor/context.md` do `custom_components/thermia_room/`. Ścieżki `README.md`
> w tekście oznaczają `../../room_sensor/README.md`; podsumowanie do kontynuacji: `CONTEXT.md` w tym katalogu.

Osobny od głównego `../../context.md` (port EXT). Zaczęty 2026-09-23 na wyraźną prośbę użytkownika: "zapisz jako
może osobny plik bo teraz zajmujemy się kolejnym problemem - nowy context.md i nowy readme.md".

Skrót decyzji i stanu na dziś jest w [`README.md`](../../room_sensor/README.md). Ten plik to surowy log: co sprawdzono, jak, z
jakim wynikiem, skąd pochodzi każdy materiał.

## 0. Skąd wziął się temat

Podczas pracy nad głównym projektem (EXT) okazało się, że `temp_indoor` (r01) jest zawsze sztywne ~20°C, bo pompa
nie ma podłączonego fizycznego czujnika pokojowego. Użytkownik chciał podłączyć realny odczyt (np. z czujnika
Zigbee w HA). Próba zapisu r01 przez EXT (`custom_components/thermia`, opisane w głównym `context.md`) była
**nieskuteczna** – pompa ignoruje zapis tego rejestru. Wniosek: `temp_indoor` prawdopodobnie nie pochodzi z EXT,
tylko z osobnego portu "Room sensor" (potwierdzone też brakiem symbolu rezystora zmiennego na oficjalnym schemacie
instalacyjnym Thermii dla tego portu, w odróżnieniu od czujnika zewnętrznego).

Stąd pytanie użytkownika: "zanim zajmiemy się próbą odczytania tabeli kalibracyjnej z bloga (...) to jakiego
modułu potrzebuję żeby konwertować sygnał z ThermIQ-ROOM2LP? jest tam prawie 30V i przecież nie podłączę tego do
ESP".

## 1. Pomiary fizyczne wykonane przez użytkownika

- Napięcie spoczynkowe na porcie Room Sensor: **~26-27V DC** (multimetr, 2026-09-22/23).
- Pod obciążeniem ~1kΩ (test z PT1000 podłączonym zamiast czujnika): napięcie spada do **~10V**.
- Podłączenie samego PT1000 (symulacja rezystancyjnego czujnika zewnętrznego) **nie zmienia** `temp_indoor` w
  odczytach pompy – pompa nie traktuje tego jako poprawny sygnał.
- Oficjalny schemat instalacyjny (PDF Thermii, przeglądany wcześniej w sesji) rysuje port Room Sensor bez symbolu
  potencjometru/rezystora zmiennego, w odróżnieniu od portu czujnika zewnętrznego (outdoor), który ten symbol ma.

**Ważna rozbieżność do zapamiętania**: wcześniej w tej sesji (przed pomiarem) automatyczne podsumowanie treści
bloga błędnie sugerowało "5V AC" dla tego portu. To jest **błędne** – ufamy pomiarowi użytkownika (26-27V DC),
nie temu wcześniejszemu streszczeniu. Możliwe źródło pomyłki: pomylenie z innym portem/urządzeniem opisanym w tym
samym wątku, albo błąd OCR/interpretacji przez wcześniejsze narzędzie.

## 2. Materiały źródłowe

Główne źródło: blog rainisto (fiński, tłumaczony w toku researchu), wpis:
https://omakotikotitalomme.blogspot.com/2024/01/danfoss-sisalampotilasensorin.html
("Danfoss sisälämpötilasensorin emulointi" – emulacja czujnika pokojowego Danfoss).

Sprawdzono też:
- Pełne archiwum tagu "Danfoss" na blogu – brak osobnego wpisu "część 2" z kodem.
- Lista repozytoriów GitHub rainisto – brak repo z kodem emulacji Room Sensor (jest tylko ThermIQ dla EXT).

**Wniosek: autor nigdy nie opublikował kodu ani PCB dla tej części.** Wszystko poniżej to rekonstrukcja z jego
zdjęć notatek/oscyloskopu w tym jednym wpisie.

Pobrane obrazy (zapisane w scratchpadzie tej sesji, nie w repo – link do wpisu jest źródłem prawdy):
- `vertailu.png` / `vertailu_full.png` (4000x3000, pobrane trikiem `/s0/` na URL-u Bloggera dla pełnej
  rozdzielczości) – **główna tabela kalibracyjna**, wiersze dla części całkowitej 20 (dziesiętne .1 do .7+).
- `IMG_20221015_234642.jpg` (4000x3000) – **druga, wyraźniejsza tabela**, część całkowita 21 (.1 do .5+).
  Kluczowa do potwierdzenia, że wzór na sumę kontrolną uogólnia się na inną część całkowitą.
- `20.1.png`, `20.2.png` – zrzuty z analizatora logicznego/oscyloskopu, znaczniki czasu ~59.99ms / 140.01ms.
  Nieprzeanalizowane jeszcze pod kątem dokładnego timingu bitów (patrz §7 otwarte pytania).
- `thermiq_table.jpg` – zrzut z Saleae Logic 2, próba dekodowania "Async Serial", etykieta widoczna na zrzucie:
  "xoor_temp: 20.5".
- `esp32_implementation.png` (1316x330) – zrzut Saleae Logic 2 z zdekodowanym bitstreamem dla 20.5°C. Obejrzany
  w tej sesji, patrz §6.
- `rn_image_picker.jpg` (4000x3000) – **trzecia tabela kalibracyjna** (części całkowite 21 i 22, 18 wierszy) z
  czytelnym nagłówkiem kolumn. Mimo mylącej nazwy pliku (sugerującej zrzut z natywnego pickera obrazków) to
  realna treść merytoryczna. Obejrzana w tej sesji, patrz §6.
- `IMG_20230111_175648.jpg` – zdjęcie innej płytki autora ("ESP32 I2C orja"), `IMG_20221015_232209.jpg` – zrzut
  ekranu analizatora z kursorami czasowymi. Obejrzane w tej sesji, patrz §6.
- `IMG_20230113_092858.jpg`, `IMG_20230113_093200.jpg` – **niezwiązane z tematem** (obudowa 3D, touchpad
  laptopa), najpewniej pobrane przez pomyłkę spoza treści wpisu. Patrz §6.

## 3. Metoda transkrypcji tabeli kalibracyjnej

Zdjęcia notatek odręcznych są niskiej jakości/skośne. Metoda, która zadziałała:

1. Pobrać oryginał w pełnej rozdzielczości z Bloggera – URL-e obrazków mają segment rozmiaru (np. `/w1200-h900/`
   albo podobny); podmiana na `/s0/` daje oryginalny, niepomniejszony plik. Różnica: `vertailu.png` (1200x900)
   → `vertailu_full.png` (4000x3000).
2. Przyciąć interesujący fragment (ImageMagick `convert`/`magick -crop`, albo Python `PIL.Image.crop`) możliwie
   ciasno wokół pojedynczego wiersza/kolumny.
3. Powiększyć przycięty fragment przez upscale LANCZOS (PIL `Image.resize(..., Image.LANCZOS)`, zwykle 3-4x) –
   znacznie poprawia czytelność odręcznych cyfr binarnych na zdjęciu telefonem.
4. Gdy zdjęcie jest wyraźnie skośne (obrócona kartka), zwykły prostokątny crop łapie fragmenty dwóch sąsiednich
   wierszy zamiast jednego. **Nie próbować na siłę korygować przez węższe/szersze cięcia** – szybciej znaleźć
   inne, mniej skośne zdjęcie tego samego typu danych (tak jak `IMG_20221015_234642.jpg` zastąpiło problematyczną
   próbę z wierszem "23.1" w `vertailu_full.png`).
5. Czytać cyfry z powiększonego obrazu ręcznie (wzrokowo), wiersz po wierszu, kolumna po kolumnie.

Wszystkie pliki pośrednie (przycięte/powiększone kawałki) zostały w scratchpadzie tej sesji – metodologia jest
odtwarzalna, ale pliki same w sobie nie są artefaktami wartymi trzymania w repo.

## 4. Reguła dekodowania bitów – wyprowadzenie i weryfikacja

Notatki autora to tabela: dla każdej temperatury (np. "20.1", "20.2", ..., "21.1", ...) narysowany ciąg 0/1
reprezentujący zaobserwowane bity na linii, plus osobno wyliczona suma kontrolna.

Hipoteza przetestowana i potwierdzona: surowy narysowany bit trzeba **odwrócić** (0→1, 1→0), a wynikowy ciąg
czytać jako liczbę binarną **LSB first** (najmniej znaczący bit na początku), żeby otrzymać wartość dziesiętną
pola.

Zweryfikowano na:
- Wszystkich czytelnych wierszach `vertailu_full.png` z częścią całkowitą 20: dziesiętne .1, .2, .3, .4, .6, .7 –
  każdorazowo odkodowana wartość dziesiętnej części zgadzała się z etykietą wiersza (100% trafień, 6/6).
- Wierszach `IMG_20221015_234642.jpg` z częścią całkowitą 21 – analogicznie zgodne (dodatkowe potwierdzenie, że
  reguła nie jest przypadkiem specyficznym dla wartości 20).

## 5. Struktura ramki i suma kontrolna

Układ tabeli (kolumny w notatkach autora) wskazuje na kolejność pól:

```
[start: 3-4 bity, stałe] [całość: 5 bitów LSB-first] [znacznik: 3 bity, stałe "1,1,1" po odwróceniu]
[dziesiętne: 4 bity LSB-first] [suma kontrolna: 5 bitów LSB-first]
```

Znalezisko z tej sesji, nieobecne w automatycznym streszczeniu treści bloga wcześniej w rozmowie: między polem
całości a polem dziesiętnych jest **stały 3-bitowy znacznik**, we wszystkich sprawdzonych wierszach odczytany
(po odwróceniu) jako `1,1,1`. Rola nieznana – prawdopodobnie separator/sync, ale brak próbki, w której by się
różnił, więc nie można wykluczyć, że w innych warunkach niesie informację.

### Wzór sumy kontrolnej

Autor bloga podał tylko, że suma kontrolna istnieje i ma 5 bitów – nie podał wzoru. Wyprowadzony i zweryfikowany
w tej sesji:

```
checksum = (52 - część_całkowita - część_dziesiętna) mod 32
```

Historia wyprowadzenia:
1. Pierwsza hipoteza z samych wierszy części całkowitej=20: `checksum = 32 - dziesiętna`. Pasowała do
   wszystkich wierszy 20.x, ale to był przypadek szczególny (bo `52 - 20 - d = 32 - d` dla części całkowitej
   akurat równej 20).
2. Test na `IMG_20221015_234642.jpg` (część całkowita 21) obalił wersję uproszczoną i doprowadził do ogólnej
   postaci z `52` i odjęciem obu części, potwierdzonej modulo 32 (pole ma 5 bitów → zakres 0-31).

Zweryfikowane punkty danych (część całkowita, część dziesiętna → checksum zgodny z odczytem z notatek):
- (20, 1), (20, 2), (20, 3), (20, 4), (20, 6), (20, 7) – z `vertailu_full.png`
- (21, x) co najmniej jeden punkt z `IMG_20221015_234642.jpg`, potwierdzający ogólność wzoru dla innej części
  całkowitej.

Razem: **5+ niezależnych punktów, 2 różne wartości części całkowitej, 100% zgodności** ze wzorem `(52 - i - d)
mod 32`.

## 6. Przegląd pozostałych zdjęć z wpisu (ta sesja, 2026-09-23) – FAKTYCZNIE OBEJRZANE

Poniżej realna zawartość każdego obrazu (nie domysł z nazwy pliku):

- **`rn_image_picker.jpg`** (4000x3000) – **NAJWAŻNIEJSZE ZNALEZISKO tej rundy**. To NIE zdjęcie sprzętu, tylko
  **trzecia strona notatek z tabelą kalibracyjną**, wiersze części całkowitej **21** (od .1/.2 do .9) i **22**
  (.0 do .8) – czyli 18 kolejnych wierszy, znacznie więcej niż w dwóch wcześniej przeanalizowanych zdjęciach.
  Kluczowe: ten obraz ma **czytelny nagłówek kolumn**, którego nie było widać w poprzednich zdjęciach:
  ```
  [1 2 4 8 16] [- - -] [1 2 4 8] [X] [.] [16 ...]
  ```
  Czyli wprost: 5 kolumn dla części całkowitej z wagami bitowymi 1,2,4,8,16 (LSB first – **niezależne
  potwierdzenie** reguły z §4, tym razem wprost z podpisu autora, nie tylko z dopasowania wyniku), potem 3 kolumny
  znacznika `- - -` (zgodne z §5), potem 4 kolumny części dziesiętnej z wagami 1,2,4,8, potem kolumna oznaczona
  `X`, kolumna `.`, i zaczynająca się kolumna sumy kontrolnej opisana `16` (obrysowana w prostokąt) – zdjęcie
  jest ucięte z prawej strony, więc dalsze kolumny sumy kontrolnej (1,2,4,8) są poza kadrem.
  **Nowe pytanie otwarte**: kolumny `X` i `.` między polem dziesiętnych a sumą kontrolną nie były wcześniej
  zidentyfikowane – ich rola nieznana (możliwe: bit kierunku/typu ramki, separator wizualny autora, albo coś
  jeszcze). Ze względu na skos i jakość zdjęcia **nie transkrybowano jeszcze samych wartości bitów** z tych 18
  wierszy – to osobne zadanie do zrobienia (duży, wartościowy zbiór danych do dalszej weryfikacji wzoru sumy
  kontrolnej, gdy będzie na to czas).
- **`esp32_implementation.png`** (1316x330) – to NIE fragment kodu, tylko **zrzut ekranu Saleae Logic 2**
  (analizator logiczny), 3 kanały z analizatorem "Async Serial", z panelem danych pokazującym zdekodowany string:
  ```
  room_temp: 20.5 ____II_II_II_IIIII_II_IIII_II_II_I_II_I_III'
  ```
  (dokładne rozmieszczenie `_`/`I` niepewne przy tej rozdzielczości – wymaga zoomu w oryginalnym narzędziu, nie
  ze zrzutu ekranu). To potwierdza, że **20.5°C był rzeczywistym testowym odczytem** użytym przez autora (ta sama
  wartość widoczna też w `thermiq_table.jpg`, gdzie etykieta "xoor_temp: 20.5" to najpewniej błąd
  OCR/przycięcia słowa "room_temp"). Warta dalszej analizy jako gotowy, zweryfikowany fizycznie przykład bitstreamu
  dla znanej temperatury – ale nie rozkodowano jej jeszcze bit-po-bicie (wymaga dostępu do oryginalnego pliku
  Logic 2, nie tylko zrzutu ekranu).
- **`IMG_20230111_175648.jpg`** (4000x3000) – zdjęcie sprzętu: niestandardowa płytka PCB autora, podpisana
  **"ESP32 I2C orja v1.0 C-2023 rainisto@iki.fi"**, z modułem ESP32 i wyświetlaczem OLED pokazującym na żywo:
  `### I2C Orja v2.0 ###`, `DATA: 192.168.1.18`, `RSSI: -32`, `T: 20.5 C`, `MAC: EC62609BCC60`. To najpewniej
  **inny projekt autora** (nazwa "I2C Orja" sugeruje most/gateway I2C, prawdopodobnie do odczytu EXT pompy przez
  WiFi/MQTT), nie bezpośrednio płytka do emulacji Room Sensor – ale pokazuje tę samą testową wartość 20.5°C, więc
  może to być ten sam etap testów/tego samego dnia. Nie zawiera nowych informacji o protokole Room Sensor.
- **`IMG_20221015_232209.jpg`** (938x561, zrzut ekranu analizatora, nie zdjęcie) – 4 kanały przebiegów cyfrowych
  (krótkie serie 2-5 impulsów powtarzające się z grubsza co ~0.7-1s w oknie 8-sekundowym), z dwoma kursorami
  czasowymi autora pokazującymi różnice **80.021167 ms** i **140.001167 ms** między wybranymi punktami. To
  potencjalnie dane o timingu (pytanie otwarte §7 pkt 1), ale przy tej rozdzielczości zrzutu nie da się wiarygodnie
  policzyć dokładnej długości pojedynczego bitu – widać tylko, że pojedyncza "ramka" (klaster impulsów) to rząd
  wielkości dziesiątek-set milisekund, nie mikrosekund. Wymaga własnego przechwycenia z dostępem do oryginalnych
  danych analizatora, nie da się tego wiarygodnie wycisnąć z samego zrzutu ekranu.
- **`IMG_20230113_092858.jpg`**, **`IMG_20230113_093200.jpg`** – **niezwiązane z tematem**: zdjęcie
  wydrukowanej w 3D obudowy/wspornika oraz zdjęcie rozbieranego touchpada laptopa. Najpewniej obrazki
  pobrane przez pomyłkę z innego miejsca strony (np. sekcja "powiązane posty"/reklamy), nie z treści wpisu o
  Danfoss. Brak jakiejkolwiek wartości dla tego projektu.

**Zaktualizowany wniosek z przeglądu**: w przeciwieństwie do wcześniejszego (przedwczesnego) wniosku – autor
jednak zostawił więcej materiału niż tylko dwie tabele: `rn_image_picker.jpg` to trzecia, duża tabela (18 wierszy,
21.x-22.x) z czytelnym nagłówkiem kolumn potwierdzającym strukturę ramki wprost z jego podpisów, a
`esp32_implementation.png`/`IMG_20221015_232209.jpg` dają namiary na realny bitstream i orientacyjny rząd
wielkości timingu. Żadne z nich nie zawiera jednak gotowego kodu źródłowego. Dwa obrazy (drukarka 3D, touchpad)
są szumem niezwiązanym z tematem.

## 7. Otwarte pytania / brakujące dane

1. **Dokładny timing bitów** (długość impulsu, baud rate Manchester). Zrzuty `20.1.png`/`20.2.png` mają widoczne
   znaczniki czasu (~59.99ms, ~140.01ms), ale nie zostały jeszcze przeanalizowane pod kątem wyliczenia
   rzeczywistej długości bitu – do zrobienia jako osobny krok, prawdopodobnie wymaga własnego przechwycenia
   przebiegu (nie da się tego wycisnąć dokładniej z samych zrzutów ekranu autora).

   **Pomiar użytkownika, 2026-09-23, pierwsze podejście**: z przebiegu w `esp32_implementation.png` (kanał D0 –
   to prawdziwy fizyczny sygnał linii, w odróżnieniu od kanału D7, który to osobny log debugowy autora przez
   UART, patrz §6), odległość między dwoma kolejnymi zboczami narastającymi "na oko" to ~143ms.

   **Pomiar pikselowy, 2026-09-23, druga runda – ZASTĘPUJE hipotezę Manchester powyżej**: użytkownik dostarczył
   ten sam fragment z naniesionymi czerwonymi kreskami co 1s (9 kresek). Zmierzono programowo (Python/PIL,
   detekcja krawędzi impulsów per-piksel) zamiast "na oko":
   - Skala: 9 kresek, odstępy [111,111,114,111,113,113,112,111] px, średnio **dokładnie 112.0 px/s**.
   - 29 zboczy narastających na kanale D0, pogrupowanych w **13 serii** (rozmiary serii: 2,2,2,5,2,4,2,2,1,2,1,1,3
     impulsów).
   - Odstęp zbocze-narastające→zbocze-narastające **wewnątrz serii**: średnio **142.3ms** (odch. std 5.9ms,
     n=16) – to dokładnie to, co użytkownik zmierzył "na oko" jako 143ms.
   - Odstęp **między seriami**: średnio **448.3ms** (odch. std 4.3ms, n=12) – **stosunek do odstępu wewnątrz
     serii ≈ 3.15×**, wyraźnie nie 2× (co sugerowałoby Manchester pół/pełny-okres) ani nie czysto całkowita
     wielokrotność.
   - Szerokość samego impulsu (mark, stan wysoki): średnio **60.0ms** (odch. std 5.4ms, n=29) – **spójna dla
     wszystkich 29 impulsów**, niezależnie od tego, czy poprzedza krótki czy długi odstęp.

   **Zrewidowany model (zastępuje wcześniejszą hipotezę Manchester)**: dane świetnie pasują do **kodowania
   odległością impulsów (pulse-distance, jak w IR NEC/RC5)**, NIE do Manchester. Każdy bit to: stały impuls
   (mark) ~60ms + przerwa (space), której DŁUGOŚĆ koduje wartość bitu:
   - bit "krótki": mark (~60ms) + krótka przerwa (~142.3-60.0=**82.3ms**) = razem ~142.3ms
   - bit "długi": mark (~60ms) + długa przerwa (~448.3-60.0=**388.3ms**) = razem ~448.3ms
   Serie z 2-5 impulsów to biegi kolejnych bitów o tej samej ("krótkiej") wartości; pojedynczy odstęp między
   seriami to jeden bit o wartości "długiej" (potwierdzone samospójnie: tam gdzie widać dwie sąsiadujące serie
   o rozmiarze 1 – pozycje 10-11 w liście powyżej – odpowiada to dwóm kolejnym bitom "długim" z rzędu, bez
   żadnego impulsu między nimi poza pojedynczymi marker-mark).

   Zastrzeżenia, wciąż otwarte: (a) źródło to jeden zrzut ekranu nieznanej proweniencji (fragment przechwycenia
   autora bloga, nie nasz własny pomiar na żywej linii) – model dobrze pasuje statystycznie, ale nie
   potwierdzony na naszym sprzęcie; (b) **nie wiadomo, który surowy bit (0 czy 1, w konwencji z §4) odpowiada
   "krótkiej" a który "długiej" przerwie** – czysta zgadywanka (`short_bit_value` w kodzie, domyślnie zgadnięte
   jako 1); (c) stosunek długiej do krótkiej przerwy (~3.15×) nie jest czystą małą liczbą całkowitą, więc nie
   wiadomo czy to "3 sloty zerowe" czy coś strukturalnie innego (np. stały czas ochronny dodany do 2-3 slotów) –
   nie ma to znaczenia dla samego sterownika (i tak używamy zmierzonych `space_long_us`/`space_short_us`
   bezpośrednio, nie jako wielokrotności), ale utrudnia niezależną weryfikację modelu bez własnego przechwycenia.
   Użyte w kodzie: `mark_us: 60000`, `space_short_us: 82000`, `space_long_us: 388000` w
   `esp32-room-sensor-thermia.example.yaml` – wciąż wymaga potwierdzenia na własnym sprzęcie przed
   `dry_run: false`.
2. **Kierunek pompa → czujnik**: rozszyfrowany protokół dotyczy tylko kierunku "czujnik zgłasza temperaturę do
   pompy". Prawdziwy fizyczny czujnik ThermIQ-ROOM2 ma wyświetlacz i przyciski korekty nastawy – więc musi też
   odbierać coś z pompy. ThermIQ opisuje to jako protokół dwukierunkowy ("2-way"). Ta strona jest całkowicie
   nieznana – brak jakichkolwiek danych na ten temat w przejrzanych materiałach.
3. **Rola 3-bitowego znacznika** `1,1,1` (§5) – nie mamy próbki, w której miałby inną wartość.
4. **Dokładne poziomy modulacji obciążenia** (jaki prąd/rezystancja odpowiada bitowi 0 vs 1) – potrzebne do
   doboru konkretnych wartości rezystorów w stopniu nadawczym interfejsu. Wiemy tylko orientacyjnie: obciążenie
   ~1kΩ ściąga napięcie z ~27V do ~10V (pomiar użytkownika, ale to był test z PT1000 w roli "zwykłego czujnika",
   nie próba emulacji właściwego protokołu – więc te liczby są tylko przybliżoną wskazówką co do rzędu wielkości
   impedancji linii, nie potwierdzonym poziomem bitu).
5. **Identyfikacja przewodów** – które z dwóch/więcej żył złącza Room Sensor (zaciski 303/304 wg wcześniej
   przeglądanego schematu Thermii) odpowiadają za co (zasilanie/sygnał/masa) – do zweryfikowania fizycznie przy
   pompie.
6. **Rola kolumn `X` i `.`** widocznych w nagłówku tabeli na `rn_image_picker.jpg`, między polem dziesiętnych a
   sumą kontrolną (§6) – nowo odkryte w tej sesji, nieobecne w opisie struktury wyprowadzonym z wcześniejszych
   dwóch zdjęć. Wymaga transkrypcji wierszy 21.x/22.x, żeby sprawdzić czy te kolumny są stałe czy zmienne.
7. **18 wierszy z `rn_image_picker.jpg` (21.x, 22.x) nie zostało jeszcze przetranskrybowanych** – duży,
   niewykorzystany jeszcze zbiór danych do dalszej weryfikacji wzoru sumy kontrolnej i struktury ramki.

## 8. Architektura interfejsu – koncepcja robocza

Dwa niezależne stopnie opto-izolowane (bo linia ~27V, ESP działa na 3.3V logic – **nie wolno** łączyć bezpośrednio):

1. **Stopień odczytu** (linia → ESP): transoptor (np. PC817/EL817/4N35) z rezystorem szeregowym dobranym pod
   27V po stronie diody LED transoptora, wyjście tranzystorowe podciągnięte do 3.3V, do GPIO.
2. **Stopień nadawczy/modulujący** (ESP → linia, do emulacji obciążenia): albo transoptor sterujący małym
   MOSFET-em przełączającym obciążenie linii, albo gotowy PhotoMOS/przekaźnik półprzewodnikowy z izolacją optyczną
   (np. z serii Panasonic AQY) – prostsze, bo nie trzeba dobierać osobnego tranzystora mocy.

**Nie dobierać ostatecznych wartości podzespołów, dopóki nie znamy poziomów modulacji z §7 pkt 4** – inaczej
ryzyko złego doboru rezystora szeregowego (za duży prąd przez transoptor / zbyt słaby sygnał).

## 9. Powiązanie z migracją na ESP32

Użytkownik i tak planuje migrację głównego projektu (EXT) z ESP8266 na ESP32 (temat odłożony w rozmowie: "mam w
planach migrację do ESP32"). Ten podprojekt jest dodatkowym argumentem za tą migracją, bo:
- autor referencyjnego rozwiązania jawnie pisał, że potrzebował **dwóch rdzeni** ESP32, żeby jednocześnie
  obsłużyć komunikację EXT (I2C-slave, bit-banged, czasowo krytyczne, patrz doświadczenia z głównego projektu –
  konkurencja o CPU z WiFi już tam sprawiała problemy na ESP8266) i emulację Room Sensor (też czasowo krytyczna,
  Manchester-like).
- ESP8266 (jeden rdzeń) prawdopodobnie nie udźwignie obu protokołów jednocześnie bez poważnych kompromisów
  czasowych.

Decyzja: **ten podprojekt czeka na migrację sprzętową na ESP32**, nie jest projektowany pod ESP8266.

## 9a. Dobór komponentów sprzętowych (2026-09-23) – weryfikacja wobec datasheetów

Przy doborze modułu na 303/304 (pełny BOM i schemat: `README.md` sekcja "Moduł sprzętowy") pobrano i przeczytano
w tej sesji rzeczywiste datasheety, zamiast polegać na pamięci:
- Omron G3VM-61G1: https://omronfs.omron.com/en_US/ecb/products/pdf/en-g3vm_61g1.pdf – **okazał się obudową SMD
  (SOP4)**, niewygodną do ręcznego montażu/płytki stykowej. Odrzucony.
- Omron G3VM-61A1/D1: https://omronfs.omron.com/en_US/ecb/products/pdf/en-g3vm_61a1_d1.pdf – G3VM-61A1 to ten
  sam układ w obudowie DIP-4 (PCB terminals, THT), pierwotnie wybrany do BOM. **Poprawka 2026-09-23 (zgłoszona
  przez użytkownika): G3VM-61A1 jest wycofany z produkcji (Omron EOL)** – potwierdzone wyszukiwaniem, m.in.
  [zgłoszenie na GitHub projektu pikvm](https://github.com/pikvm/pikvm/issues/1517) opisujące dokładnie ten sam
  problem z tą samą częścią. Odrzucony.
- **Toshiba TLP241A (wybrany, zastępuje G3VM-61A1)**:
  https://toshiba.semicon-storage.com/info/TLP241A_datasheet_en_20230525.pdf – DIP-4 THT (raster 7.62mm),
  datasheet Rev.9.0.A z 2026-04-07 (aktualny, nie EOL). Pin1=Anoda, Pin2=Katoda, Pin3/Pin4=Drain (symetryczne,
  wyjście bez polaryzacji). Vf typ 1.27V/max 1.4V@10mA, zalecany If 5-25mA (typ 7.5mA), Ron≤150mΩ (ciągłe)/100mΩ
  (impuls <1s), Voff max 40V, VDD zalecane do 32V, Ion do 2.0A (zalecane), izolacja I/O 5000Vrms. Potwierdzona
  dostępność na stanie: DigiKey (odmiana DIP-4, 316+ szt.), Mouser (16 000+ szt.).
- PC817: specyfikacja ogólnie znana/potwierdzona wyszukiwaniem (Vf~1.2-1.4V@20mA, CTR 50-600%@5mA, izolacja
  5kVrms) – standardowy, wielu producentów, nie wymaga jednego konkretnego źródła.

Rezystory (R1-R4) i TVS (D1) dobrane rachunkiem w `README.md` na podstawie powyższych parametrów + wcześniej
zmierzonej ~1.7kΩ rezystancji szeregowej pompy (§7 pkt 4). R1 (220Ω) przeliczony pod nowy Vf TLP241A – wynik
praktycznie taki sam jak dla G3VM (If≈9.2mA, wciąż w zalecanym zakresie). **Nadal niepotwierdzone na żywym
sprzęcie** – to dobór
"na papierze", pierwszy krok przed montażem na płytce stykowej.

## 10. Status i następne kroki

Patrz [`README.md`](../../room_sensor/README.md) sekcja "Następne kroki" – skrót:
1. Wyliczyć timing bitów z dostępnych zrzutów oscyloskopu (albo zebrać własne dane pomiarowe).
2. Napisać host-testowalny enkoder ramki (wzorem silnika EXT z głównego projektu – `THERMIA_HOST_TEST`) z
   testami na znanych parach temperatura→bity z notatek autora.
3. Dopiero potem: dobór konkretnych podzespołów interfejsu i fizyczna budowa na ESP32.

Ten plik będzie aktualizowany w miarę postępu (nowe pomiary, dekodowanie timingu, testy enkodera).

## 11. Pełna ramka z debug stringu autora (2026-10-07) – ZASTĘPUJE §5 w części preambuła/znacznik

Użytkownik odczytał dokładnie string z `esp32_implementation.png`: `room_temp: 20.5 ____II_II_II_IIIII_II_IIII_II_II_I_II_I_I_III`
(wersja w §6 gubiła jedno `_I` pod koniec). Grupy impulsów 2,2,2,5,2,4,2,2,1,2,1,1,3 zgadzają się 1:1 z pikselowym
pomiarem kanału D0 (§7 pkt 1), więc string = fizyczny przebieg. `I` = krótka przerwa + impuls = surowy bit 1,
`_I` = długa przerwa + impuls = surowy bit 0 (zgodne z „1=0 ja 01=1” z bloga po inwersji). **Przerwa jest PRZED
impulsem**, nie po nim.

```
110101011 | 11010  | 111      | 0101  | 00100  | 011
preambuła | int=20 | znacznik | dec=5 | chk=27 | ogon     = 29 bitów
```

- int/dec/chk i wzór `(52-i-d) mod 32` potwierdzone na niezależnej próbce.
- Znacznik na linii to surowe **1,1,1** (§5 błędnie wnioskował 0,0,0).
- Preambuła 9 bitów i ogon 3 bity – stałe tylko przy założeniu (jedna próbka). Odwrócona preambuła czytana LSB-first
  daje 20 na pierwszych 5 bitach – możliwe, że niesie np. nastawę; niezweryfikowane.
- Kod poprawiony: `thermia_room_frame.*` (29 bitów), `thermia_room.cpp` (przerwa→impuls), test sprawdza zgodność
  enkodera z tym stringiem znak w znak.

Inne ustalenia z researchu tej sesji:
- Komentarz pod blogiem (Kalle Högegård, 2026-03-18): emulacja działa u niego; po jej starcie pompa od razu ustawia
  indoor target r03/r04 = 21.0 → dobry wskaźnik wykrycia czujnika przez EXT.
- Instrukcja Danfoss DHP (FI): HUOLTO → ASENNUS → JÄRJESTELMÄ → HUONEANTURI (Ø/PÄÄLLÄ); HUONEKERROIN widoczny tylko
  z czujnikiem.
- Publicznie brak gotowego kodu emulacji (rainisto nie opublikował części 2; MrGlenn-tech/Thermia-ESP32 tylko
  przechowuje INDR_T).

## 12. Korekta sumy kontrolnej i potwierdzenie preambuły (2026-10-07)

- Legenda na dole `thermiq_table.jpg`: **„MSB oikealla, 1=0 ja 01=1”** – tabela jest zapisana w notacji symboli
  linii (`1` = `I`, zakreślone `01` = `_I`), bit logiczny = 0 dla `1` i 1 dla `01`.
- **Suma kontrolna = int XOR dec XOR 0b01010** – pasuje do 11 wierszy tabeli (20.1–20.8, 21.9, 22.0–22.2) i do
  zrzutów linii. Wzór `(52-i-d) mod 32` z §5 był błędny (np. 20.2: 30 zamiast 28, 20.4/20.6/20.8 też źle) –
  wcześniejsze „potwierdzenia” w §5 to były błędne odczyty tabeli.
- Zrzuty prawdziwego czujnika `20.1.png`/`20.2.png` (zmierzone pikselowo): 29 impulsów każdy, ten sam 9-bitowy
  początek `110101011` i 3-bitowy koniec `011` co string 20.5 autora. Kolumna „Start 1 1 1” w tabeli to ostatnie
  3 bity tej preambuły (`_I I I` – pierwsza długa przerwa po prostu nie jest zapisana w tabeli); pierwszych 6 bitów
  (`I I _I I _I I`, logicznie 0,0,1,0,1,0) autor w tabeli pominął, bo się nie zmieniały.
- Enkoder po poprawce odtwarza oba zrzuty znak w znak (test `test_line_captures`).

## 13. Timing z kursorów, short_bit_value, kompilacja (2026-10-07)

- Kursory analizatora autora: `20.2.png` – impuls **59.988625 ms**, okres krótkiego bitu **140.009833 ms**;
  `IMG_20221015_232209.jpg` – **80.021167 ms** (przerwa krótka) i 140.001167 ms. Stosunek okresów długi/krótki na
  `20.1.png` (16 vs 50–51 px) i `20.2.png` (17 vs 53–54 px) ≈ 3.15 → długi okres ≈ 440 ms. Nowe wartości w
  przykładowym YAML: `mark_us 60000`, `space_short_us 80000`, `space_long_us 380000` (wcześniej 82000/388000 z
  pikseli `esp32_implementation.png`, §7 pkt 1 – niedokładne przez rozdzielczość ~9 ms/px).
- `short_bit_value` wyłączone na prośbę użytkownika (zakomentowane w `__init__.py`, `thermia_room.h`, przykładowym
  YAML, z adnotacją „może się przydać”); w kodzie stała `SHORT_BIT_VALUE = true`.
- Pomiar użytkownika: multimetr na 303/304 nie pokazuje częstotliwości, na AC napięcie skacze – brak dowodu na
  nośną AC (rainisto pisał „puskee AC:ta”), raczej szum/impulsy.
- Kompilacja: Docker `ghcr.io/esphome/esphome:stable` (ESPHome 2026.9.1), przykładowy YAML z tymczasowymi
  sekretami w scratchpadzie, projekt zamontowany read-only – sukces, 0 ostrzeżeń w `thermia_room`. Nie wgrywane.
  Polecenie do powtórzenia (z katalogu z YAML-em i secrets.yaml):
  `docker run --rm -v $PWD:/config -v <repo>:/proj:ro -w /config ghcr.io/esphome/esphome:stable compile room.yaml`
  (w YAML `external_components.path: /proj/custom_components`).
- Następny krok: test na pompie wg checklisty w `README.md` „Stan na 2026-10-07”.

## 14. Pierwszy sukces na pompie i nowy układ ramki (2026-10-07)

**Sukces**: pompa przyjęła emulację (alarm czujnika zniknął, wyświetlacz pokazuje temperaturę). Działało z YAML
użytkownika: `mark_us 60000`, `space_short_us 82000`, `space_long_us 388000`, `polarity_invert: false`,
`update_interval: 5s` (ramka co 10 s, ~2.2 s ciszy między ramkami), ESP32-S3, pin GPIO10.

Dwa problemy i ich wyjaśnienie (na bitach z logu dla 23.0: `11010101100010111111101000011`):

1. **Temperatura zadana stoi na 21 °C** mimo zmiany HUONE/r32 (r32 = 15 zgodnie z pompą). Bity logiczne 2–8
   „preambuły” LSB-first = **21**, a stała `0b01010` w sumie kontrolnej = `21 XOR 31`. Wniosek: ramka niesie
   nastawę, pompa przy podłączonym czujniku bierze ją z czujnika. Nowy układ:
   `start(2)=11 | nastawa(7) | int(8) | dec(4) | chk(5) | koniec(3)=011`,
   `chk = (int ^ dec ^ nastawa ^ 0x1F) & 0x1F`. Dla nastawy 21 bity identyczne jak wcześniej.
   Niepotwierdzone: inne wartości nastawy niż 21 (szerokość pola i udział w sumie wywnioskowane).
2. **Pompa czasem pokazuje 92 °C** (wyświetlacz pompy, nie błąd EXT; r01=005C w logu). „Znacznik 111” to górne
   3 bity 8-bitowego pola int; przeczytanie ramki przesuniętej o 2 bity wcześniej daje z tego bajtu dokładnie 92.
   Pompa czasem źle synchronizuje się na początku ramki. Hipoteza: przerwa ~2.2 s między ramkami jest czytana jako
   bit lub pompa łapie ostatnie „11” poprzedniej ramki jako start. Do eksperymentu: `frame_gap` (0 ms = ciągły
   strumień).

Zmiany w kodzie: `thermia_room_frame.*` (nowy układ pól, `encode_room_temperature(temp, setpoint)`),
`thermia_room.*` (encja `setpoint` zapisywana w flash, opcja `frame_gap` – wysyłanie z `loop()` po zakończeniu
poprzedniej ramki + przerwa), test hosta 1716/1716. Osobno w `~/repo/private/esphome-thermia`: `temp_indoor` łączy
r01 + r02 (dziesiąte), `temp_indoor_target` r03 + r04. Obie kompilacje (z GitHub i z lokalnym esphome-thermia) OK.

### 14a. Test nastawy ≠ 21 (2026-10-07, 23:26)

- `frame_gap: 0ms` (ciągły strumień): pompa traci synchronizację – r01 = 0xF2 (242), brak reakcji. Pompa
  synchronizuje się na ciszy między ramkami i **nie sprawdza sumy kontrolnej** (przyjęła 92 i 242).
- `frame_gap: 2s`, nastawa **19**, `chk = int^dec^sp^31` (20.2 → chk 26, bity `11001101111010111101110100011`):
  **alarm czujnika wraca** (r14=0x10), r01=0x14 (20 = wartość domyślna). Ramka odrzucona.
  Skoro pompa nie sprawdza sumy, odrzucenie może wynikać z samego pola „nastawy” (np. stała sygnatura, a 21 to
  przypadek albo wartość domyślna urządzenia). Następny test: `setpoint_in_checksum: false` z nastawą 19.
- Dziesiętne `temp_indoor` (r01+r02) działają po wypchnięciu esphome-thermia.

### 14b. Wniosek o temperaturze zadanej (2026-10-08)

Potwierdzone przez użytkownika na pompie:

| Stan czujnika | Temperatura zadana używana przez pompę (r03/r04) |
|---|---|
| ramka zaakceptowana (brak alarmu) | z czujnika – zawsze **21** (r32/HUONE ignorowane) |
| alarm czujnika (ramka odrzucona / brak czujnika) | z ustawień pompy **r32** (HUONE) |

- Pole „nastawy” (7 bitów po starcie) musi mieć wartość 21 – każda inna (sprawdzone różne wartości, LSB- i
  MSB-first, z nastawą w sumie kontrolnej i bez) powoduje alarm. Zachowuje się jak stała sygnatura czujnika.
- Zgodne z ThermIQ: ThermIQ-ROOM wysyła tylko temperaturę rzeczywistą (`INDR_T`), temperaturę zadaną ustawia się
  przez EXT d050/r32 – w ich ekosystemie nie ma ścieżki zmiany nastawy przez czujnik. Kalle Högegård (komentarz pod
  blogiem, 2026-03) ma ten sam problem (21.0, brak możliwości zmiany).
- Prawdziwy sterownik Thermia z wyświetlaczem/przyciskami musi zmieniać nastawę jakoś inaczej (inny typ ramki,
  kierunek pompa→czujnik – protokół „2-way”). Nieznane; potrzebny zrzut prawdziwego sterownika przy zmianie nastawy.
- Użytkownik odrzuca obejście „przesuwania” wysyłanej temperatury (chce, żeby pompa pracowała na rzeczywistych
  wartościach).
- Kod: opcje `setpoint` i `setpoint_in_checksum` zakomentowane w `__init__.py` z adnotacją; ramka zawsze niesie 21;
  zapisana w flash nastawa z eksperymentów nie jest już wczytywana. `frame_gap: 2s` zostaje (działa).

### 14c. Eksperyment 1: bity startu i końca (2026-10-08, w toku)

Hipoteza: prawdziwy sterownik z przyciskami zmienia nastawę flagami w bitach dotąd uznawanych za stałe (start `11`,
koniec `011`) – ThermIQ-ROOM, z którego pochodzą wszystkie próbki, nastawy nie zmienia, więc zawsze ma je stałe.
Encje HA `experiment_start_bits` (0–3, domyślnie 3) i `experiment_tail_bits` (0–7, domyślnie 3); wartość = bity w
kolejności nadawania (pierwszy bit = najstarszy). Nie są zapamiętywane – restart wraca do 3/3.
Uwaga: czy pompa sprawdza sumę kontrolną – nierozstrzygnięte (odrzuciła nastawę 19 przy sumie identycznej jak w
przyjmowanych ramkach; wyświetliła 92/242 przy niezgodnej wg naszego wzoru). Alarm przy zmianie końca/startu może
więc oznaczać „suma obejmuje te bity”, a nie „te bity nic nie znaczą”.

Wyniki: (do uzupełnienia)

### 14d. Eksperyment 2: skan typów ramek (2026-10-08, start 22:34:32 czasu urządzenia)

Na prośbę użytkownika wgrane przez Claude (OTA, `ostrow-budowa-room-sensor.yaml` + `experiment_scan:`; kopia
YAML sprzed zmiany: scratchpad sesji). Po 60 s normalnych ramek: na przemian ramka poprawna (21) i eksperymentalna
z 7-bitowym polem = 0..127 (bez 21), temperatura 23.0, każdy typ dwa razy (suma z polem / stała 0b01010).
`frame_gap: 4000ms` → ~11 s na ramkę, ~100 min całość. Obserwowane r01 (czy pompa przyjęła 23.0), r03 (temperatura
zadana), r14 (alarm czujnika = 0x10). Stan wyjściowy: r01=20, r03=21 (0x15), r14=0, r32=14.

**Wynik (koniec 00:11:36): zero reakcji.** 254 ramki eksperymentalne (typy 0–127 bez 21, oba warianty sumy). Przez
cały skan r01 = 0x14 (20), r03 = 0x15 (21), r14 = 0 – pompa po cichu ignoruje ramki z polem ≠ 21 (nie przyjmuje
ich temperatury, nie zmienia nastawy, nie zgłasza alarmu, dopóki co drugą ramkę dostaje poprawną).
Wnioski:
- Pole = 21 jest warunkiem przyjęcia ramki (sygnatura/typ „pomiar temperatury”); innych typów z temperaturą w polu
  int pompa nie rozpoznaje – przynajmniej w tym układzie bitów startu/końca i z suma kontrolną liczoną jednym z
  dwóch naszych wzorów.
- Odrzucenie jest „ciche” – alarm pojawia się dopiero, gdy przez dłuższy czas nie ma żadnej poprawnej ramki.
- Nie wykluczone: nastawa w innym miejscu ramki typu ≠ 21 (nie w polu int), inna długość ramki, albo zmiana nastawy
  tylko w odpowiedzi na sygnał od pompy (kierunek pompa→czujnik, pomysł 3 – wymaga PC817).
- Po skanie YAML przywrócony (identyczny z kopią sprzed eksperymentu), firmware wgrane ponownie, stan normalny
  (r14=0, r03=21). Uwaga: po każdym restarcie ESP temperatura emulowana startuje od 20.0, dopóki HA jej nie ustawi.

### 14e. Eksperyment 3: brute force sumy kontrolnej (2026-10-08, przygotowany, NIE uruchomiony)

Wniosek ze skanu §14d: pompa odrzuca całe ramki z polem ≠ 21 (nawet temperaturę), więc najpewniej sprawdza sumę
kontrolną, a nasz wzór działa tylko dla pola 21. Forum (maalampofoorumi): prawdziwy czujnik 086U6003 ma LCD i
przyciski, nastawa w krokach 0,5 °C, pompa reguluje wg nastawy z czujnika → nastawa jest w ramce, pewnie w
połówkach stopnia. Suma ma 5 bitów = 32 wartości, więc dla danego pola wystarczy wypróbować wszystkie.
Tryb: `experiment_scan: {payload: 23.0, fields: [20, 22, 19, 23]}` – na każde pole 32 sumy, każda ramka testowa 2×
z rzędu + 1 poprawna; ~70 min. Sukces = r01 → 23 (ramka przyjęta), r03/r04 ≠ 21.0 (nowa nastawa).
Firmware skompilowane, ale upload nieudany – urządzenie nieosiągalne (No route to host). YAML przywrócony.
