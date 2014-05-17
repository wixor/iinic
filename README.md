Bezprzewodowa Apokalipsa 2014
=============================

Co ja pacze?
------------

Kod API, z którego możecie korzystać, leży w pythonowej paczce `iinic.py`. Kod
serwera, którego nie potrzebujecie, ale pewnie chcielibyście się pośmiać (albo
samemu sobie uruchomić), leży w pliku `server.py`. Do tego mamy kilka programów
przykładowych: `monitor.py` po prostu nasłuchuje i wypisuje na ekran to, co
usłyszy; `console.py` wypisuje na ekran bajty, które usłyszy (więc dobrze jest,
jeżeli są one sensownymi znakami ascii) oraz jednocześnie wciąga od użytkownika
linijkę tekstu z ekranu (ona się nie pokazuje, ale to nie szkodzi) i gdy ów
naciśnie enter -- wysyła ją. `ticker.py` to mały programik, który wysyła 
`aaaaaaaaaa\n` lub `bbbbbbbbbb\n` co 1 sekundę lub 0.85 sekundy; uruchomienie
dwóch kopii tego programu na tym samym kanale ilustruje, że działają kolizje.

**Zanim zaczniecie czytać szczegóły, dobrze jest zobaczyć bardzo prosty i krótki
tutorial, czyli program `ticker.py`**

Ten kod z pewnością nie jest odporny na walenie w niego młotkiem.
Przypuszczalnie nie jest nawet odporny na używanie go zgodnie z dokumentacją.
Obsługa błędów na symulatorze polega mniej-więcej na tym, że ignorujemy
polecenie, które wydaje się błędne. Będzie to naturalnie powodowało dziwne
efekty w działaniu waszych programów, więc najlepiej nie wysyłać błędnych
poleceń ;) O ile zwracanie błędów przez serwer (tudzież prawdziwą kartę
sieciową) nie stanowi problemu, o tyle nie mam dobrego pomysłu na to, jak
zakomunikować owe błędy programowi. Chyba nie tak: "hej, polecenie które
wydałeś karcie czterdzieści dwie instrukcje temu, właśnie próbowało się wykonać
i zgadnij, co się okazało? było błędne! deal with it".

Szczegółowy opis warstwy fizycznej znajdziecie w pliku `phys.md`. Najważniejsze
parametry karty sieciowej są następujące:

* procesor: 8-bitowy AVR mega32 taktowany 14.7456 MHz, 2 kB pamięci RAM,
* interfejs USB: port szeregowy 230400 bps w trybie 8n1 bez kontroli przepływu,
* długość bufora nadawania: 1536 bajtów,
* długość bufora poleceń: 256 bajtów,
* długość bufora dla PC: 128 bajtów,
* rozdzielczość timera: około 0.5 us
* modulacja radiowa: FSK,
* częstotliwość środkowa modulacji: od 860.48 MHz do 879.515 MHz co 5 kHz,
* dewiacja: od 15 kHz do 240 kHz
* szerokość pasma odbieranego: od 67 kHz do 400 kHz
* bitrate: od 600 do 57600 bitów na sekundę,
* moc nadawania: od 0.09 mW do 5 mW (od -10.5 dBm do 7 dBm) z anteną,
* czułość: rzędu 0.01 pikowata (-110 dBm).

Po podłączeniu do prądu karta będzie migała zieloną lampką. Podczas
inicjalizacji obiekt `NIC` nawiązuje połączenie z kartą, co sygnalizowane jest
stałym zapaleniem się zielonej lampki. W obliczu stresująych sytuacji karta
odmówi współpracy i przejdzie w stan paniki. Jes to sygnalizowane ciągłym
zapaleniem się lampki czerwonej. Aby udobruchać kartę należy ją odłączyć oraz
ponownie podłączyć do komputera. Prosimy nie dawać karcie czekoladek; są dla
niej bardzo niezdrowe. Hipotetycznie może też zdarzyć się zawieszenie się
karty sieciowej; taka sytuacja oznacza błąd w oprogramowaniu karty i będzie
sygnalizowana miganiem lampki czerwonej.

Najprostszym sposobem na zdenerwowanie karty jest spowodowanie przepełnienie
bufora. Bufor danych nadawania jest całkiem spory i przepełnienie go nie
spowoduje awarii karty; stare dane nadpiszą nowe i dane zakolejkowane do
wysłania pójdą w świat jako bełkot. Kod API stara się uchronić programistę
przed takimi sytuacjami śledząc ilość danych znajdującą się w tym buforze.
Wszystkie dane, które mają zostać wysłane z karty do komputera, przechodzą
przez bufor dla PC. Przepełnienie tego bufora wydaje się trudne, jednak
z pewnością nie niemożliwe. W szczególności wykonywanie bardzo wielu poleceń,
które powodują odpowiedź karty (np. `ping()`) połączone z odbieraniem szybkiej
transmisji danych (np. z prędnością 57600 bps lub większą) może spowodować
przekręcenie się tego bufora. W takiej sytuacji karta zacznie wysyłać bełkot
do PC, co prawdopodobnie spowoduje jakąś awarię w waszym programie.
Zdecydowanie najbardziej wrażliwym na przepełnienie jest bufor poleceń.
Przepełnienie go spowoduje z wielkim prawdopodobieństwem przejście w stan
paniki. W szczególności poniższy kod psuje kartę sieciową:
```python
while True:
    nic.tx('a')
```
Ta pętla powoduje nieustanne wysyłanie do karty poleceń nadania jednego
bajtu danych. Takie polecenie zajmuje jeden bajt w buforze danych do wysłania
oraz aż pięć bajtów w buforze poleceń. Zatem bufor poleceń przepełni się dużo
szybciej niż bufor danych do wysłania; zabezpieczenie przed przepełnieniem
tego drugiego nie zdąży zadziałać i karta spanikuje. Hardware przewiduje
możliwość wprowadzenia kontroli przepływu na linii komputer-karta; dzięki temu
karta mogłaby wstrzymać strumień danych płynący od komputera do czasu, gdy w
buforach zrobi się miejsce. Na dzień dzisiejszy funkcja ta nie jest jednak
zaimplementowana w oprogramowaniu karty.

Domyślne ustawienia karty sieciowej są następujące:

* kanał: 868.32 MHz, dewiacja 60 kHz, pasmo odbierane 67 kHz
* bitrate: 9600 bitów na sekundę
* moc nadawania: minimalna
* czułość: minimalna

Częstotliwość środkowa kanału znajduje się mniej-więcej w środku pasma
obsługiwanego przez radio i jest domyślną wartością podaną w dokumentacji
radia. Pozostałe parametry zostały dobrane tak, aby wyeksponować możliwie wiele
radiowych efektów transmisji. Pasmo obierane jest zostało ustawione na
najmniejsze z możliwych; dewiacja została ustawiona na największą mieszczącą
się w paśmie. Wybrana prędkość transmisji pozwala na dość efektywną, ale
nie bezbłędną, transmisję. Manipulacja jedynie prędkością transmisji umożliwia
uzyskiwanie kanałów bardzo wiernych (minimalna prędkość transmisji) lub ledwie
działających (maksymalna prędkość transmisji). Karta sieciowa nie pamięta
waszych ustawień, więc jeżeli chcecie je zmienić, musicie wysłać odpowiednie
komendy przy każdym uruchomieniu programu.

Osoby, które znają gita i chciałyby coś napisać w tym projekcie, zachęcam do
korzystania z niniejszego repo; dajcie znać, a dodam wam dostępy. Jeżeli w
trakcie prac dojdziecie do wniosku, że coś zachowuje się dziwnie, odniesiecie
wrażenie, że serwer umarł, lub coś w tym stylu -- niewykluczone, że macie rację,
więc dawajcie znać. W sytuacjach podbramkowych można postawić serwer u siebie,
ale wtedy jest dużo mniej ciekawie, bo wasze pakiety nie będą miały kolizji z
pakietami innych uczestników zabawy ;)

Czas
----

Zanim omówimy metody obiektu `NIC`, kilka słów o czasie. W API występują dwa
rodzaje czasu: jeden odnosi się do czasu komputera, zaś drugi do zegarka na
karcie sieciowej. Oba czasy są bezwzględne. Czas komputera jest doublem i jest
mierzony jako unixowy timestamp, czyli liczbę sekund, które minęły od początku
świata. Jego aktualną wartość można uzyskać z pythonowej funkcji `time.time()`.
Czas karty sieciowej jest int-em i jest mierzony jako liczba mikrosekund, które
minęły od jakiegoś ważnego dla karty wydarzenia (na przykład włączenia prądu).

Czas karty sieciowej pojawia się pod nazwą `timing`. Możemy z niego skorzystać
w metodzie `timing`, która oznacza "poczekaj, aż wybije podana godzina". Jest
też zwracany przez metodę `rx` przy każdym odebranym bajcie.

Czas komputera pojawia się jako argument `deadline` dla metod API, które
potencjalnie oczekują na jakąś komunikację ze strony karty sieciowej. Jeżeli
argument taki zostanie podany (na przykład metodzie `rx`), to metoda podda się
jeżeli nie uda się jej zrealizować swojego zadania przed upływem deadline-u.
W takim wypadku metoda poinformuje o porażce zwracając jakąś ciekawą wartość
(`None`, `False` lub podobną). Zatem jeżeli chcemy poczekać maksymalnie pięć
sekund na bajt z karty sieciowej, możemy napisać:
```python
    b = nic.rx(deadline = time.time() + 5.)
```
Jeżeli podamy deadline, który jest w przeszłości (w szczególności wartość 0),
metoda musi wykonać się od razu (to znaczy bez oczekiwania na komunikację
z kartą). Możemy zatem zapytać się API, czy w tej chwili mamy do odebrania
jakiś bajt z karty, przy użyciu kodu
```python
    b = nic.rx(deadline = 0.)
```
Jeżeli nie podamy żadnej wartości deadline (bądź podamy `None`), metoda czeka
do skutku (więc jeżeli będziemy w ten sposób próbowali odebrać kolejne bajty
ramki, a ramka zostanie ustrzelona w połowie transmisji i końcówka nie dojdzie,
to nasz program się zawiesi).

Fakt, że podajemy bezwzględny czas, w którym ma zmieścić się procedura (a nie
na przykład czas, jaki wolno jej czekać), nie jest przypadkowy. Procedury
wyższego poziomu będzą wymagały wywołania wielu procedur niższego poziomu.
Dzięki deadline-om wystarczy, że procedura wyższego poziomu przyjmie jako
argument swój deadline i będzie przekazywała go do wszystkich wywołań niższego
poziomu. Nie musimy ciągle martwić się o to, ile czasu minęło od ostatniej
operacji i ile w związku z tym czasu nam pozostało (takie obliczenia
musielibyśmy wykonywać non-stop, gdybyśmy korzystali z timeoutów).

Dobrze byłoby, aby protokoły wyższego poziomu eksponowały w pełne możliwości
protokołów niższego poziomu (w szczególności aby procedury protokołów wyższego
poziomu rzeczywiście miały ten argument deadline i go obsługiwały). Dobrze
byłoby również, gdyby protokoły wyższego poziomu zachowywały asynchroniczną
naturę niniejszego API i nie nadużywały możliwości synchronizacji z kartą
sieciową. Prawdziwe protokoły sieciowe, dzięki którym mamy Internet i wszystko,
są obsługiwane w sposób asynchroniczny. Jest to fantastyczna okazja, by
zasmakować w tym sposobie programowania. Byłoby strasznie kiepsko, gdyby
procesor "czekał" aż przyjdzie pakiet z internetu, albo gdyby jądro tworzyło
osobny wątek dla każdego otwartego połączenia TCP.

Metody API
----------

* `get_uniq_id()`:  
  Ta metoda zwraca unikalny identyfikator karty sieciowej. Identyfikator
  ten jest liczbą całkowitą z zakresu 0x0001 do 0xFFFE, czyli dwubajtową
  liczbą bez znaku, która nie jest ciągiem samych zer ani samych jedynek.
  Na symulatorze, z racji braku fizycznego urządzenia, identyfikator ten
  jest losowany przy każdym połączeniu z serwerem; może to prowadzić do
  kolizji.
* `get_approx_timing()`:  
  Ta metoda zwraca przybliżoną aktualną wartość zegarka na karcie sieciowej.
  Nie ma żadnych gwarancji ani oszacowań na jakość tego przybliżenia.
  Błąd może być dowolnie wielki, jednak mamy nadzieję, że będzie mieścił
  się w zakresie kilkudziesięciu milisekund.
* `ping()`:  
  Ta metoda wysyła "pinga" do karty sieciowej. Karta odpowiada nia niego
  natychmiast (oczywiście kiedy dojdzie do niego w kolejce poleceń). Metoda ta
  zwraca obiekt `PingFuture`. Pole `acked` tego obiektu jest bool-em, który 
  mówi, czy karta odpowiedziała już na wysłanego pinga. Metoda `await()` tego
  obiektu zawiesza program dopóki nie otrzymamy odpowiedzi na pinga. Metoda ta
  przyjmuje argument `deadline` (opisany wyżej) oraz zwraca boola mówiącego
  o tym, czy doczekaliśmy się odpowiedzi na pinga (mogliśmy się nie doczekać,
  jeżeli minął deadline). Metoda `add_callback(cb)` tego obiektu pozwala
  zarejestrować funkcję, która zostanie wywołana, gdy karta odpowie na tego
  pinga. Uwaga: funkcja ta zostanie wywołana w nieprzewidywalnym momencie,
  z nieprzewidywalnego miejsca w kodzie. W szczególności może ona zostać
  wywołana z kodu obsługi transmisji lub z innego kodu, który manipuluje
  stanem karty sieciowej. W związku z tym, funkcja ta absolutnie nie może
  wykonywać żadnych operacji na obiekcie `NIC`; należy również zachować
  najwyższą ostrożność używając tej funkcji do manipulacji stanem programu.
* `sync(deadline = None)`:  
  Ta metoda synchronizuje komputer z kartą sieciową przy użyciu pinga:
  wysyłamy pingna, czekamy aż karta nam go odeśle i wtedy wiemy, że jesteśmy
  zsynchronizowani. Zwracana wartość ma takie same znaczenie jak w przypadku
  metody `PingFuture.await`.
* `timing(timing)`:  
  Ta metoda wysyła do karty polecenie, aby oczekała do momentu, gdy jej
  wewnętrzny zegar przekroczy wartość `timing`. Ta metoda nie wstrzymuje
  działania programu!
* `set_channel(channel)`:  
  Ta metoda wysya do karty polecenie zmiany kanału na `channel`. Kanał jest
  opisany przez obiekt `iinic.Channel`. Konstruktor tego obiektu przyjmuje
  argumenty `freq`, `dev` oraz `bw`. Częstotliwość (`freq`) podawana jest jako
  liczba całkowita, która zostanie przeliczona na prawdziwą częstotliwość
  według wzoru 20 * (43 + freq / 4000) [MHz]. Minimalną wartością jest 96,
  natomiast maksymalną 3909. Dewiacja (`dev`) jest podawana jako liczba
  całkowita, która zostanie przeliczona na prawdziwą dewiację według wzoru
  (1 + dev) * 15 kHz. Minimalną wartością jest 0, natomiast maksymalną 15.
  Szerokość pasma odbieranego (`bw`) jest jedną ze stałych
  `iinic.Channel.BW_*`. Domyślny kanał (czyli ten, który jest ustawiany po
  resecie urządzenia) jest dostępny jako stała `iinic.NIC.defaultChannel`.
  Ta metoda nie oczekuje na faktyczne dokonanie zmiany kanału!
* `set_bitrate(bitrate)`:  
  Ta metoda wysyła do karty polecenie zmiany szybkości nadawania oraz
  odbierania na `bitrate`. Wartość `bitrate` powinna być jedną ze stałych
  `iinic.NIC.BITRATE_*`. Można też podawać inne wartości; w takim przypadku
  muszą one być z zakresu od 0 do 255. Jeżeli najstarszy bit podanej wartości
  jest ustawiony, prawdziwy bitrate oblicza się ze wzoru
  10e+6 / 29 / 8 / ((bitrate & 0x7F) +1) [bps]; jeżeli najstarszy bit jest
  wyczyszczony, prawdziwy bitrate oblicza się ze wzoru
  10e+6 / 29 / (bitrate+1) [bps]. Należy pamiętać, że obsługa transmisji
  przychodzących wymaga pewnej mocy obliczeniowej od karty sieciowej.
  Ustawienie zbyt wysokiego bitrate'u może spowodować przeciążenie karty,
  co prawdopodobnie spowoduje przejście w stan paniki. Domyślny bitrate jest
  dostępny jako stała `iinic.NIC.defaultBitrate`. Ta metoda nie oczekuje na
  faktyczne dokonanie zmiany!
* `set_sensitivity(gain, rssi)`:  
  Ta metoda wysyła do karty polecenie zmiany wzmocnienia sygnału odbieranego
  (`gain`) oraz progu czułości (`rssi`). Wartość progu czułości nie jest w tej
  chwili wykorzystywana. Wartość wzmocnienia musi być jedną ze stałych
  `iinic.NIC.GAIN_*`. Domyślne parametry są dostępne jako stałe
  `iinic.NIC.defaultGain` oraz `iinic.NIC.defaultRSSI`. Ta metoda nie oczekuje
  na faktyczne dokonanie zmiany!
* `set_power(power)`:  
  Ta metoda wysyła do karty polecenie zmiany mocy nadawania na `power`.
  Podana wartość musi być jedną ze stałych `iinic.NIC.POWER_*`. Zostanie ona
  użyta przy następnym nadawaniu. Domyślna wartość jest dostępna jako stała
  `iinic.NIC.defaultPower`.
* `tx(payload, overrun_fail=True, deadline=None)`:  
  Ta metoda wysyła do karty polecenie nadania wiadomości `payload`. Należy
  pamiętać, że proces nadawania trwa nietrywialną ilość czasu. Domyślnie
  program na komputerze nie jest blokowany na czas nadawania, natomiast dopóki
  nadawanie nie zakończy się, karta nie będzie przetwarzała poleceń. Bufor
  nadawczy karty ma ograniczoną pojemność; w związku z tym nie możemy
  zakolejkować wielkiej ilości danych do wysłania. Obiekt `NIC` przy użyciu
  pingów śledzi ilość danych, które znajdują się w kolejce karty. Jeżeli okaże
  się, że zakolejkowanie danej wiadomości spowodowałoby przekroczenie rozmiaru
  bufora, metoda `tx()` zgłosi wyjątek. Na potrzeby debuggowania oraz
  implementacji bardzo prostych protokołów udostępniona jest opcja
  `overrun_fail`. Ustawienie jej na wartość `False` spowoduje, że metoda `tx()`
  będzie oczekiwała na to, aż w buforze karty zrobi się wystarczająco dużo
  miejsca na zakolejkowanie danego pakietu. Oczekiwanie to jest ograniczone
  czasem `deadline`. Dokładny czas takiego oczekiwania jest jednak trudny do
  ustalenia. Metoda `tx()` zwraca obiekt `PingFuture`; karta wyśle
  potwierdzenie tego pinga w momencie, gdy zakończy nadawanie wiadomości.
  W szczególności wysłanie danych wraz z zablokowaniem programu do momentu
  zakończenia wysyłania można uzyskać przy użyciu idiomu
  `nic.tx(payload).await()`.
* `rx(deadline = None)`:  
  Ta metoda odbiera z karty jedną paczkę danych. Argument `deadline` jest
  opisany powyżej; jeżeli żadna paczka nie zostanie w całości odebrana przed
  upływem deadline-u, funkcja ta zwraca `None`. Odebrane dane są reprezentowane
  przez obiekt `RxBytes`, który zawiera pola `bytes`, `rssi` oraz `timing`.
  Należy zauważyć, że odebrany bajt może pochodzić z przeszłości: na przykład
  podczas, gdy oczekiwaliśmy na synchronizację z kartą sieciową przy użyciu
  metody `sync()`, karta mogła odbierać dane; metoda `rx()` zwróci je zanim
  przystąpi do pobierania danych aktualnie transmitowanych na łączu. Krótko
  mówiąc, metoda `rx()` zwraca kolejne paczki, które usłyszała karta sieciowa
  nawet, jeżeli nasz program przez jakiś czas "nie uważał".
