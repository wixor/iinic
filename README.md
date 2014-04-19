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

Limity karty sieciowej, które symulujemy, są następujące:

* kanał: 0 ... 31,
* bitrate: pomiędzy 300 a 115200 bitów na sekundę,
* długość bufora nadawania: 768 bajtów,
* moc nadawania: bez znaczenia,
* maksymalny czas oczekiwania w metodzie `timing()`: 30 sekund.

Nie oznacza to, że prawdziwe urządzenie będzie miało takie same limity.
W szczególności radzę nie zbliżać się do limitu długości bufora nadawania;
ramki kilkunasto -- kilkudziesięciobajtowe powinny w zupełności wystarczyć.
Należy też zauważyć, że rozmiar bufora nadawania nie jest ograniczeniem na
jedną ramkę, ale na wszystkie dane zakolejkowane do wysłania: sto wywołań
funkcji `tx()` z dziesięciobajtowymi ramkami bez oczekiwania na zakończenie
wysyłania danych powoduje próbę zakolejkowania tysiąca bajtów w buforze!

Domyślne ustawienia symulowanej karty sieciowej:

* kanał = 1,
* bitrate = 300 bitów na sekundę
* moc = 10

Bitrate celowo jest mały, żeby wyraźnie (na oko) było widać efekty związane z
kolejkami, opóźnieniami i kolizjami; oczywiście docelowo chcielibyśmy pracować
z nieco wyższymi prędkościami. Ustawienie mocy nie ma żadnego znaczenia. Karta
sieciowa (ani prawdziwa ani symulowana) nie będzie pamiętała waszych ustawień,
więc jeżeli chcecie je zmienić, musicie wysłać odpowiednie komendy przy każdym
uruchomieniu programu.

Osoby, które znają git-a i chciałyby coś napisać w tym projekcie, zachęcam do
korzystania z niniejszego repo; dajcie znać, a dodam wam dostępy. Jeżeli w
trakcie prac dojdziecie do wniosku, że coś zachowuje się dziwnie, odniesiecie
wrażenie, że serwer umarł, lub coś w tym stylu -- niewykluczone, że macie rację,
więc dawajcie znać. W sytuacjach podbramkowych można postawić serwer u siebie,
ale wtedy jest dużo mniej ciekawie, bo wasze pakiety nie będą miały kolizji z
pakietami innych uczestników zabawy ;)

Czas
----

Zanim omówimy metody obiektu NIC, kilka słów o czasie. W API występują dwa
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
musielibyśmy wykonywać non-stop, gdybyśmy korzystali z timeout-ów).

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

* `ping()`:  
  Ta metoda wysyła "pinga" do karty sieciowej. Karta odpowiada nia niego
  natychmiast (oczywiście kiedy dojdzie do niego w kolejce poleceń). Metoda ta
  zwraca obiekt `PingFuture`. Pole `acked` tego obiektu jest bool-em, który 
  mówi, czy karta odpowiedziała już na wysłanego ping-a. Metoda `await()` tego
  obiektu zawiesza program dopóki nie otrzymamy odpowiedzi na pinga. Metoda ta
  przyjmuje argument `deadline` (opisany wyżej) oraz zwraca parę dwóch wartości:
  pierwsza jest bool-em mówiącym o tym, czy doczekaliśmy się odpowiedzi na
  ping-a (mogliśmy się nie doczekać, jeżeli minął deadline); druga jest listą
  bajtów, które odebrała karta sieciowa, zanim odpowiedziała na ping-a.
* `sync(deadline = None)`:  
  Ta metoda synchronizuje komputer z kartą sieciową przy użyciu ping-a:
  wysyłamy ping-a, czekamy aż karta nam go odeśle i wtedy wiemy, że jesteśmy
  zsynchronizowani. Zwracana wartość ma takie same znaczenie jak w przypadku
  metody `PingFuture.await`.
* `timing(timing)`:  
  Ta metoda wysyła do karty polecenie, aby oczekała do momentu, gdy jej
  wewnętrzny zegar przekroczy wartość `timing`. Ta metoda nie wstrzymuje
  działania programu!
* `set_channel(channel)`:  
  Ta metoda wysya do karty polecenie zmiany kanału na `channel`. Ta metoda nie
  oczekuje na faktyczne dokonanie zmiany!
* `set_bitrate(bitrate)`:  
  Ta metoda wysyła do karty polecenie zmiany szybkości nadawania oraz odbierania
  na `bitrate`. Ta metoda nie oczekuje na faktyczne dokonanie zmiany!
* `set_power(power)`:  
  Ta metoda wysyła do karty polecenie zmiany mocy nadawania na `power`.
  Nie wiadomo jeszcze, jaka jest semantyka tej wartości. Podana tutaj wartość
  zostanie użyta przy następnym nadawaniu.
* `tx(payload)`:  
  Ta metoda wysyła do karty polecenie nadania wiadomości `payload`. Należy
  pamiętać, że proces nadawania trwa nietrywialną ilość czasu. Program na
  komputerze nie jest blokowany na czas nadawania, natomiast dopóki nadawanie
  nie zakończy się, karta nie będzie przetwarzała poleceń.
* `rx(deadline = None)`:  
  Ta metoda odbiera z karty jeden bajt danych. Argument `deadline` jest opisany
  powyżej; jeżeli żaden bajt nie zostanie odebrany przed upływem deadline-u,
  funkcja ta zwraca `None`. Każdy odebrany bajt jest reprezentowany przez
  obiekt `RxByte`, który zawiera pola `byte`, `bitrate`, `channel`, `power`
  oraz `timing`.
