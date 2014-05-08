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
    Klasa reprezentująca pojedynczą ramkę. Przechowuje bajty ramki, czas odebrania i moc odebrania. Opis metod:
    - fromReceived(msg: string, firstTiming: double, powers: [double])
        tworzy ramkę na podstawie odebranych bajtów, czasu odebranego i mocy odbioru poszczególnych bajtów. Metoda ta NIE sprawdza poprawności bajtów, 
        ta odpowiedzialność spoczywa na funkcji odbierającej (w klasie FrameLayer) i przez nią jedynie powinna być używana
    - toSend(ftype: char, fromId: unsigned short, toId: unsigned short, payload: string)
        tworzy ramkę gotową do wysłania na podstawie typu, identyfikatorów odbiorcy i nadawcy oraz payloadu. Dokleja do niej długość oraz sumę kontrolną.
    - bytes():
        zwraca wszystkie bajty ramki
    - __repr__():
        tworzy czytelną reprezentację ramki
    - type(), fromId(), toId(), content(), power(), timing(), payloadLength()
        metody zwracające odpowiednie wartości - nie wymagają komentarza ;)
* FrameLayer
    Klasa odpowiadająca za pierwszą warstwę, czyli odbieranie i wysyłanie ramek. Docelowo będzie też eksportować wszystkie funkcje obiektu NIC. 
    Opis metod:
    - __init__(nic: NIC, myId: int = None)
        Jeżeli myId nie został podany lub myId = None, identyfikator pobierany jest z obiektu nic metodą get_uniq_id().
    - getMyId()
        zwraca mój identyfikator
    - receiveFrame(deadline: double = None)
        funkcja odbierająca pojedynczą ramkę. Argument deadline ma taką samą semantykę, jak w przypadku funkcji NIC.rx(), tutaj dotyczy pierwszego bajtu. UPDATE: dotyczy paczki bajtów.
    - sendFrame(ftype: char, fromId: unsigned short, toId: unsigned short, content: string, timing: double = None)
        funkcja wysyłająca ramkę, a konkretnie wysyłająca do urządzenia/symulatora polecenie wysłania ramki - czyli ta funkcja NIE synchronizuje się z kartą.

+++ Dispatcher.py +++
Klasa odpowiedzialna za odbieranie ramek i przekazywanie ich odpowiednim protokołom. Umożliwia rejestrowanie protokołów. 
Przy rejestracji protokół podaje nazwę (po której będzie można go znaleźć z innych protokołów) oraz z jakimi typami ramek ma być skojarzony.
Ramki tego typu zostaną mu przekazane po odebraniu.
Klasa umożliwia też rejestrowanie callbacków z zadanymi terminami wykonania.
Opis funkcji:
-  registerProto(proto: Proto, name: string, frameTypes: string)
    Funkcja rejestrująca protokół proto o nazwie name skojarzony z typami ramek podanymi w stringu frameTypes. Funkcja sprawdza, czy dana nazwa lub typ nie zostały zajęte, w przeciwnym razie rzuca wyjątek.

- getProtoByName(name: string)
    Funkcja zwracająca protokół, który podał nazwę name przy rejestracji.

- scheduleCallback(callback, timing)
    Funkcja rejestrująca callback, który ma być wykonany w czasie timing (jest to czas systemowy, nie karty).
        
- scheduleRepeatingCallback(callback, firstCall, interval)
    Tak jak wyżej, ale callback będzie wykonywany w nieskończoność, poczynając od firstCall z odstępami interval.
    
- loop()
    Funkcja odpowiadająca za pętlę główną programu.
    
+++ Proto.py +++
Klasa bazowa każdego protokołu. Posiada pola dispatcher wskazującym na dispatchera (lub None, jeżeli protokół jest niezarejestrowany) oraz frameLayer wskazującym na obiekt FrameLayer (lub None w tym samym przypadku).
Opis metod:
- handleFrame(frame)
    Funkcja "wirtualna", którą trzeba zaimplementować w klasie pochodnej. Jest ona wywoływana przez dispatchera za każdym razem, gdy przyjdzie ramka skojarzona z danym protokołem.
    
- doRegistration(dispatcher)
    Funkcja wywoływana przez dispatchera przy rejestracji. Nie wywoływać tej funkcji manualnie!

- uponRegistration(dispatcher)
    Funkcja "wirtualna" do zaimplementowania w klasie pochodnej. Wywołwana przez dispatchera przy rejestracji.
    
+++ PingPongProto.py +++
Przykładowy protokół dla dwóch urządzeń. Po włączeniu, nasłuchujemy przez chwilę. Jeżeli odebraliśmy cokolwiek, uznajemy, że było to Ping X lub Pong X, gdzie X jest liczbą i odpowiadamy Pong X+1 lub Ping X+1 po sekundzie i tak w kółko. Jeżeli nie, uznajemy, że jesteśmy stroną inicjującą i co 2 sekundy wysyłamy "Ping 1" w eter. Po szczegóły polecam wczytać się w kod.



