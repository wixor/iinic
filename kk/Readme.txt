+++ Config.py +++
Tutaj znajdują się wszystkie stałe, jakich będziemy potrzebowali.

+++ OurException.py +++
Podklasa standardowej klasy Exception, opisuje wszystkie wyjątki, które dotyczą naszego kodu.

+++ Frame.py +++
Struktura ramki jest następująca:
LTSSRRDDD...DDDC
gdzie:
- L to długość payloadu, maksymalnie 255
- T to jednobajtowy typ ramki
- SS to dwubajtowy identyfikator nadawcy
- RR to identyfikator odbiorcy
- DDD to payload o długości L
- C to jednobajtowa suma kontrolna (crc-8) wszystkich poprzednich bajtów.
Zakładamy, że każde urządzenie ma identyfikator różny od 0 i różny od 65535.

Ten plik eksportuje dwie klasy:
* Frame
    Klasa reprezentująca pojedynczą ramkę. Przechowuje bajty ramki, czas odebrania i moc odbioru.
    Opis metod:
    - fromReceived(msg: string, firstTiming: double, powers: [double])
        tworzy ramkę na podstawie odebranych bajtów, czasu odebranego i mocy odbioru 
        poszczególnych bajtów. Metoda ta NIE sprawdza poprawności bajtów, 
        ta odpowiedzialność spoczywa na funkcji odbierającej (w klasie FrameLayer) i przez nią
        jedynie powinna być używana;
    - toSend(ftype: char, fromId: unsigned short, toId: unsigned short, payload: string)
        tworzy ramkę gotową do wysłania na podstawie typu, identyfikatorów odbiorcy i nadawcy
        oraz payloadu. Dokleja do niej długość oraz sumę kontrolną.
    - bytes():
        zwraca wszystkie bajty ramki
    - __repr__():
        tworzy czytelną reprezentację ramki
    - type(), fromId(), toId(), content(), power(), timing(), payloadLength()
        metody zwracające odpowiednie wartości - nie wymagają komentarza ;)
* FrameLayer
    Klasa odpowiadająca za pierwszą warstwę, czyli odbieranie i wysyłanie ramek. Docelowo będzie
    też eksportować wszystkie funkcje obiektu NIC.
    Opis metod:
    - __init__(nic: NIC, myId: int = None)
        Jeżeli myId nie został podany lub myId = None, identyfikator pobierany jest z obiektu NIC
        metodą get_uniq_id().
    - getMyId()
        zwraca mój identyfikator
    - receiveFrame(deadline: double = None)
        funkcja odbierająca pojedynczą ramkę. Argument deadline ma taką samą semantykę, 
        jak w przypadku funkcji NIC.rx(). Wartością zwracaną jest obiekt typu Frame, gdzie timing
        tej ramki jest równy czasowi rozpoczęcia odbioru ramki (czyli ma taką samą semantykę, jak
        timing w metodzie sendFrame).
    - sendFrame(ftype: char, fromId: unsigned short, toId: unsigned short, content: string, 
        timing: double = None)
        funkcja wysyłająca ramkę, a konkretnie wysyłająca do urządzenia/symulatora polecenie
        wysłania ramki - czyli ta funkcja NIE synchronizuje się z kartą. Funkcja zwraca
        dokładnie to, co zwraca metoda .tx() obiektu NIC.

+++ Dispatcher.py +++
Klasa odpowiedzialna za odbieranie ramek i przekazywanie ich odpowiednim protokołom. 
Umożliwia rejestrowanie protokołów. 
Przy rejestracji protokół podaje nazwę (po której będzie można go znaleźć z innych protokołów)
oraz z jakimi typami ramek ma być skojarzony. Ramki tego typu zostaną mu przekazane po odebraniu.
Klasa umożliwia też rejestrowanie callbacków z zadanymi terminami wykonania.
Opis funkcji:
-  registerProto(proto: Proto, name: string)
    Funkcja rejestrująca protokół proto o nazwie name. Zostaje on skojarzony z typami występującymi
    w stringu proto.frameTypes. Jeden typ może być skojarzony z wieloma protokołami!

- getProtoByName(name: string)
    Funkcja zwracająca protokół, który podał nazwę name przy rejestracji.

- scheduleCallback(callback, timing)
    Funkcja rejestrująca callback, który ma być wykonany w czasie timing 
    (jest to czas systemowy, nie karty).
        
- scheduleRepeatingCallback(callback, firstCall, interval)
    Tak jak wyżej, ale callback będzie wykonywany w nieskończoność, poczynając od firstCall
    z odstępami interval.

- loop()
    Funkcja odpowiadająca za pętlę główną programu.
    
+++ Proto.py +++
Klasa bazowa każdego protokołu. Posiada pola dispatcher wskazującym na dispatchera 
(lub None, jeżeli protokół jest niezarejestrowany) oraz frameLayer wskazującym na obiekt FrameLayer
(lub None, w analogicznym przypadku).
Klasa posiada pole frameTypes, koniecznie do przeładowania w klasach pochodnych. Jest to string
lub lista bajtów, które oznaczają typy ramek skojarzonych z danym protokołem.
Opis metod:
- handleFrame(frame)
    Funkcja "wirtualna", którą trzeba zaimplementować w klasie pochodnej. Jest ona wywoływana 
    przez dispatchera za każdym razem, gdy przyjdzie ramka skojarzona z danym protokołem.
    
- doRegistration(dispatcher)
    Funkcja wywoływana przez dispatchera przy rejestracji. Nie wywoływać tej funkcji manualnie!

- onStart(dispatcher)
    Funkcja "wirtualna" do zaimplementowania w klasie pochodnej.
    Wywołwana przez dispatchera tuż przed startem głównej pętli.
    
+++ PingPongProto.py +++
Przykładowy protokół dla dwóch urządzeń. Po włączeniu, nasłuchujemy przez chwilę. 
Jeżeli odebraliśmy wiadomość postaci Ping X lub Pong X, gdzie X jest liczbą, to odpowiadamy 
odpowiednio Pong X+1 lub Ping X+1 po sekundzie i tak w kółko. Jeżeli nic nie odebraliśmy lub
komunikacja nam się urwała po pewnym czasie, uznajemy, że jesteśmy stroną inicjującą
i co 2 sekundy wysyłamy "Ping 1" w eter. Po szczegóły polecam wczytać się w kod.


+++ TimeSyncProto.py +++
Prosty (prawdopodobnie zbyt prosty) asynchroniczny protokół do synchronizacji czasu. Celem tego
protokołu jest obliczenie różnicy między "moim" czasem karty, a czasem karty, która wystartowała
jako pierwsza w całej sieci. No, nie do końca, tak naprawdę chodzi o maksimum ze wskazań zegarów
wszystkich kart w sieci. Po zsynchronizowaniu się, protokół ustawia pole self.clockDiff, będący
różnicą między "moim" zegarem a najstarszym zegarem. Wartość ta jest (powinna być!) zawsze
nieujemna.
Typ ramek właściwy dla tego protokołu to 'S', tzw ramki sync. Są to ramki, zawierające jedną
liczbę (zapisaną jako string) będącą zawsze aktualnym wirtualnym wskazaniem zegara. Wirtualnym 
tzn rzeczywistym przesuniętym o clockDiff (który może być zero).
Każde przejście do stanu SYNCED, czyli uznanie, że jesteśmy zsyncrhonizowani, zwieńczone jest
wysłaniem pięciu (MASTER_SYNC_FRAMES_COUNT) ramek sync z kwadratowym backoffem.
Ramki te są wysyłane niezależnie od innych protokołów (TODO), dlatego TimeSyncProto nazwałem
asynchronicznym.
Wszystkie porównania czasów następują z pewną tolerancją odpowiadającą mniej więcej czasowi
wysłania jednego bajtu (TODO).
Jest to protokół stanowy z następującymi stanami:
0. PREPARING
    Stan oczekiwania na zsynchronizowanie się z kartą. Za pomocą pingów obliczamy
    * roundTripTime będący sumę czasów: wysłania polecenia do karty oraz dotarcia danych z karty
    * approxCardTimeDiff będący sumą: opóźnienia pomiędzy get_approx_time_diff obiektu NIC
        a jego realnym czasem oraz wartości roundTripTime. Wartość approxCardTimeDiff należy brać
        pod uwagę, jeżeli chcemy zadać warstwie frameLayer wysłanie ramki o określonym timingu i
        chcemy, żeby karta otrzymała ten komunikat w przyszłości. Do tej wartości można dobrać
        się metodą self.getApproxTiming().
    Uwaga! Wszystkie wartości tutaj są jedynie orientacyjne i nie ma żadnych gwarancji co do ich
    poprawności. Z moich eksperymentów wynika, że dla symulatora warto zwiększyć opóźnienie
    o kolejne 100ms (patrz: funkcja _sendSyncFrame()).
    
    W tym stanie wszystkie ramki są *ignorowane*.
    
    Po upływie 1.5s zakładamy, że synchronizacja z kartą się udała i przechodzimy do następnego
    stanu.
    
1. STARTING
    Stan nasłuchiwania na istniejącą komunikację i trwa przez maksymalnie LISTEN_ON_START
    jednostek czasu, gdzie jednostka jest zgrubnym oszacowaniem na czas potrzebny do wysłania 255
    bajtów i jeszcze trochę.
    
    Jeżeli nie usłyszeliśmy żadnej komunikacji, uznajemy, że być może jesteśmy jedynym urządzeniem
    i przechodzimy do stanu SYNCED; oczywiście ustalamy clockDiff na 0.
    
    Jeżeli usłyszeliśmy jakąś ramkę, ale NIE jest to ramka sync, przechodzimy do stanu DEMAND_SYNC.
    
    Jeżeli usłyszeliśmy ramkę sync z mniejszym czasem, ignorujemy ją.
    
    Jeżeli usłyszeliśmy ramkę sync z większym czasem, dosynchronizowujemy się do niej, tzn 
    clockDiff := wartość_w_ramce - czas_jej_odebrania
    i przechodzimy do stanu SYNCED.
    
2. DEMAND_SYNC
    Jest to stan, w którym nakłaniamy sąsiednie urządzenia do podania nam obowiązującego w sieci
    czasu. Wysyłamy po prostu ramki sync z clockDiff = 0 (niepoprawnym) oraz kwadratowym backoffem
    tak długo, aż nie otrzymamy ramki sync z wyższym czasem. Wówczas ustalamy clockDiff i
    przechodzimy do stanu SYNCED.
    
3. SYNCED
    Jest to stan, w którym uznajemy, że jesteśmy dosynchronizowani do reszty sieci.
    
    Jeżeli otrzymamy ramkę sync z czasem niższym niż nasz, wysyłamy 5 ramek typu sync z kwadratowym
    backoffem.
    
UWAGI dla użytkowników protokołu
1. Stan SYNCED jest dość subiektywny; istnieje oczywiście możliwość, że w tym stanie NIE jesteśmy 
zsynchronizowani. Należy się więc liczyć z tym, że z tego stanu wypadniemy.

2. Niniejszy opis nie precyzuje bardzo wielu kwestii (np: co, gdy dostaniemy kolejne niepoprawne ramki
typu sync w trakcie synchronizowania kogoś?). Należy go traktować jako zarys opisu.

3. Jeżeli chcemy zostać powiadomieni o zmianie stanu, proszę dopisać stosowne linijki kodu w
funkcjach _gotSynced oraz _lostSync.

4. Jeżeli dopiszemy jakiś protokół wyższej warstwy, proponuję przemyślenie dwóch rzeczy:
    * przeniesienie wysłania ramki do wyższej warstwy (żeby nie psuć np. struktury rund i nie
        powodować niepotrzebnych kolizji)
    * automatyczną dedukcję struktury rund z istniejącej komunikacji (np wiedząc, że wszystkie
        ramki zostaną wysłane na początku jakiejś rundy)