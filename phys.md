Bezprzewodowa Apokalipsa 2014
=============================

Szczegóły warstwy fizycznej
---------------------------

Na transmisję składa się nadawanie oraz odbiór. Nadawanie jest procesem
zdecydowanie prostszym, ponieważ w pełni kontrolujemy to, co się dzieje;
zwyczajnie krzyczymy w eter i nie obchodzą nas żadne problemy. Jednocześnie
nadajemy po to, aby być w stanie coś odebrać, zatem staramy się krzyczeć
w taki sposób, aby odbiorca miał możliwie duże szanse poprawnie usłyszeć
naszą transmisję.

Zadaniem karty sieciowej oraz api (zwanych dalej warstwą fizyczną), jest
wyłącznie to, aby ktoś inny ich usłyszał. Nie interesuje ich, kto to będzie.
Nie zapewniają oni żadnej kontroli poprawności, potwierdzeń, niczego.
W szczególności, nie dodają oni żadnych informacji do wysyłanych danych.
Starają się jedynie wykrzyczeć w eter to, co zostanie przekazane do api.

Jednostką nadawania jest ramka. Z punktu widzenia warstwy fizycznej, struktura
ramki nie jest w żaden sposób zdefiniowana; przez ramkę rozumiem tutaj jedynie
arbitralny ciąg bajtów przekazany do funkcji `tx()`. Każde wywołanie metody
`tx()` wysyła nową ramkę. Może to być ramka jednobajtowa (choć nie wydaje się
to bardzo użyteczne). Górnym limitem na długość ramki jest 1535 bajtów
(rozmiar bufora nadawania na karcie sieciowej minus 1).

W celu ułatwienia (właściwie: umożliwienia) odbioru, na początek każdej ramki
karta dodaje dwubajtową preambuę oraz dwa bajty kodu synchronizującego. Fakt
ten jest szczegółem implementacyjnym warstwy fizycznej i wyższe warstwy
zasadniczo nie powinny się nim martwić. Przyczyna takiego działania stanie
się zrozumiała po prześledzeniu procedury odbioru. Należy zauważyć, że te
cztery bajty nie stanowią żadnej informacji i wyższe warstwo nie mają nad nimi
żadnej kontroli. Preambuła ma postać 0xAA, 0xAA, 0x2D, 0xD4. Żaden z tych
czterech bajtów nie przychodzi jako bajt odebrany w metodzie `rx()`.
Bezpośrednio po wysłaniu preambuły, karta wysyła kolejne bajty waszej ramki.
Nie ma żadnej przerwy między bajtami, nie ma żadnych znaczników początku ani
końca.

Procedura nadawania ramki rozpoczyna się od wysłania do modułu radiowego
(który jest częścią karty sieciowej) polecenia wyłączenia odbiornika i
włączenia nadajnika. Wykonanie tego polecenia nie jest natychmiastowe;
w szczególności różnorakie generatory, wzmacniacze i tym podobne elementy radia
muszą się uruchomić i ustabilizować. Czas trwania tych zabiegów nie jest
ani dokładnie znany ani nawet stały (wysłanie każdej ramki może być opóźnione
o inny czas). Dokumentacja modułu radiowego podaje, że uruchomienie nadajnika
trwa około 150 us. Radio samo wykrywa, kiedy wszystko to się dokona,
i automatycznie rozpoczyna transmisję bitów. Jak wiemy, radio ma zaprogramowaną
prędkość wysyłania danych. Przyjmijmy, że jest to 9600 bitów na sekundę.
W takim wypadku jeden bit trwa tbit = 0,0001041(6) sekundy, czyli około jednej
dziesiątej milisekundy. Jeżeli zaprogramowaliśmy radio do pracy z
częstotliwością f0 oraz dewiacją df, zaś kolejne wysyłane bity mają wartości
b0, b1, b2, ..., to radio będzie wysyłało sygnał sinusoidalny o częstotliwości
f0 + df lub f0 - df (zależnie od wartości b0) przez czas tbit; następnie
przejdzie do nadawania kolejnego bitu i znów przez czas tbit będzie wysyłało
sygnał o częstotliwości f0 + df lub f0 - df, tym razem zależnie od wartości b1.
Powyższy proces nazywa się modulacją FSK. Po nadaniu w ten sposób wszystkich
bitów danych kart wysyła jeszcze cztery bity postambuły; są to 1010 lub 0101
zależnie od tego, jaki był ostatni bit wysłanych danych (tak, aby
zmaksymalizować liczbę przejść 0->1 / 1->0). Następnie do radia wysyłane jest
polecenie wyłączenia nadajnika; wykonanie tego polecenia nie jest
natychmiastowe, więc radio nadaje jeszcze przez chwilęe szum.

Wartość f0 jest programowalna w zakresie od 860.48 MHz do 879.515 MHz
z krokiem 5 kHz, co daje około 3800 możliwych ustawień. Wartość df jest
programowalna w zakresie od 15 kHz do 240 kHz z krokiem 5 kHz, co daje 16
możliwych ustawień. Należy przy tym pamiętać, że wszystkie podane tutaj
wartości są przybliżone; rzeczywistość to nie matematyka, zaprogramowana
częstotliwość nie będzie idealnie zgadzała się z realną, radio nie potrafi
również wykonać dodawania częstotliwości z matematyczną precyzją, nie potrafi
odmierzyć czasu z nieskończoną dokładnością i tak dalej.

Zadanie odbiornika jest zdecydowanie trudniejsze niż nadajnika. W pierwszym
kroku sygnał odbierany z anteny jest wzmacniany i filtrowany w celu wyłonienia
z niego tego fragmentu pasma (czyli kanału), na którym spodziewamy się usłyszeć
wiadomość. Wartość wzmocnienia jest programowalna; możemy użyć wzmocnienia
maksymalnego (oznaczanego jako 0 dB) lub wzmocnień mniejszych: -6 dB, -14 dB
lub -20 dB. Decybele (dB) są logarytmiczną jednostką bezwymiarową;
x [dB] = log10 (x/10); na przykład -6 dB ~= 0.25, zatem wzmocnienie -6 dB
oznacza wzmocnienie cztery razy mniejsze niż maksymalne. Szerokość kanału
również jest programowalna i może wynosić 67 kHz, 134 kHz, 200 kHz, 270 kHz,
340 kHz lub 400 kHz.

Po wstępnym przefiltrowaniu nasze radio widzi zatem pewien wycinek spektrum
częstotliwości, w którym mamy nadzieję znaleźć transmisję. Oczywiście w owym
wycinku spektum mogą pojawić się dowolne transmisje, dowolnie kodowane,
dowolnie modulowane i tak dalej. Odbiornik staje zatem przed zadaniem
określenia, czy to, co słyszy, przypomina w ogóle transmisję FSK (czyli taką,
jaką generuje nadajnik). Moduł radiowy posiada wbudowany układ wykrywający
transmisje wyglądające jak FSK. Jeżeli wskaźnik ten wykaże prawdopodobieństwo
odbierania transmisji, dane są kierowane do dekodowania. Należy tutaj
podkreślić, że nie ma tu mowy o żadnej pewności, a jedynie o podejrzeniu,
że odbieramy prawdziwą transmisję.

Ostatnim krokiem odbioru danych jest zamienienie zdemodulowanego sygnału na
ciąg bajtów. Zadanie to niestety również nie jest trywialne, gdyż nie wiadomo,
gdzie zaczyna się, a gdzie kończy się bajt. Nawet, jeżeli jednostką transmisji
byłby pojedyńczy bit, wciąż nie wiadomo, gdzie kończy się jeden a zaczyna się
drugi. Dobrze widać to na przykładzie transmisji bajtu 0xFF, który składa się
z samych jedynek: nie istnieje żadna pewna metoda stwierdzenia, gdzie
przebiegają granice między kolejnymi bitami (czyli jedynkami) w tym bajcie.
Mówiąc nieco innym językiem, odbierany jest strumień danych, lecz brak do niego
zegara. Uwaga: pojęcie to nie ma nic wspólnego z zegarem karty sieciowej ani
żadnymi innymi zegarami wyższego poziomu, turami, rundami etc. Odnosi się ono
jedynie do wiedzy o tym, gdzie znajdują się kolejne bity w odbieranym sygnale.

Moduł radiowy posiada wbudowaną funkcję odzyskiwania zegara do odbieranych
danych. Opiera się ona na założeniu, że początek transmisji będzie zawierał
ciąg naprzemiennych zer i jedynek. Z tego powodu dwubajtowa preambuła, która
rozpoczyna każdą transmisję nadajnika, to nic innego jak na zmianę jedynki
i zera. Ponieważ radio wie (zakłada), co będzie wysyłane na początku, może ono
odtworzyć rytm (zegar), w którym wysyłane są bity; następnie radio
"zapamiętuje" ten rytm (czyli synchronizuje się z nadajnikiem) i używa go do
odzyskania pozostałych bitów sygnału.

Pomimo, że radio może już odzyskać kolejne bity, wciąż nie wiadomo, gdzie
przebiegają granice między bajtami. Co więcej, nie wiadomo nawet, ile bitów
preambuły zostało "zużytych" na zsynchronizowanie się z nadajnikiem, a il
pozostało do odebrania zanim zacznie się transmisja właściwych danych. Aby
rozwiązać ten problem, radio po ustaleniu zegara odbiorczego się oczekuje na
moment, w którym szesnaście kolejnych odebranych bitów będzie równych
ustalonemu kodowi synchronizującemu. Kod ten to dwa bajty nadawane po
preambule (0x2D, 0xD4). W momencie spostrzeżenia tego kodu radio zakłada, że
właśnie te szesnaście kolejnych bitów stanowi dwa bajty i że w związku z tym
następny bit będzie pierwszym bitem pierwszego bajtu danych.

Po złapaniu synchronizacji radio będzie dekodowało kolejne odbierane bity
wedle ustalonego wcześniej zegara transmisji oraz kroiło je na ośmiobitowe
paczki i przesyłało do procesora na karcie, a ten dalej do komputera. Należy
tutaj zauważyć, że rytm odbierania danych nie jest zależny od rytmu nadawania;
radio samo "wymyśla sobie" jak interpretować dane. W szczególności możliwe
jest, że bitrate nadawcy był inny, używał on innego protokołu, bądź wszystkie
mechanizmy zostały oszukane przez zwykły radiowy szum (w którym można dopatrzyć
się wielu rzeczy, jeżeli patrzymy na niego odpowiednio długo). Proces
odbierania danych kończy się w momencie, gdy wskaźnik jakości sygnału w radiu
wskaże, że prawdopodobnie przestaliśmy odbierać transmisję FSK. Może zdarzyć
się, że wskaźnik ten "zgaśnie" chwilę po prawdziwym zakończeniu transmisji;
wtedy do końca pakietu zostanie doklejonych trochę bajtów szumu. Może też
zdarzyć się, że wskaźnik ten "zgaśnie" w trakcie trwania pakietu; wtedy pakiet
zostanie urwany w środku. Po przerwaniu odbierania karta wraca do trybu
odzyskiwania zegaru odbiorczego oraz poszukiwania kodu synchronizującego;
elementy te prawdopodobnie nie wystąpią w dalszej części pakietu, zatem
zostanie ona stracona nawet, jeżeli będzie ona odbierana z bardzo dobrymi
parametrami. Jeżeli jednak żaden ciekawy efekt radiowy nie wystąpi, pakiet
nadany przez metodę `tx()` pojawi się jako pakiet odebrany w metodzie `rx()`.
