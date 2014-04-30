# Infrastruktura dookoła ramek

Aktualnie pracuję nad abstrakcją formatów ramek różnego rodzaju. Z racji tego, że taka abstrakcja jest bezużyteczna bez jakiejś działającej implementacji, posłużę się i zbuduję te abstrakcje nad kodem Karola Konaszyńskiego (w folderze kk/).

## Architektura kodu

Chciałbym głównie wydzielić trzy kawałki abstrakcji w kodzie:

* Budowanie ramki danego formatu (`FrameBuilder`)
* Odczytywanie ramki danego formatu (`FrameDeserializer`)
* Pobieranie/Wysyłanie ramek z/do karty sieciowej (`FrameLayer`)

Możemy to zrobić na dwa sposoby - jeden to sprawić, żeby każda implementacja podążała za schematem, drugi to taki, że tworzę generyczne obiekty do realizacji tych zadań i wstrzykuje zależności od realnych formatów ramek. Wybrałem ten drugi sposób, bo nie chcę psuć nikomu zabawy z pisaniem kodu formatu ramek jak mu się żywnie podoba - w szczególności w takim podejściu mogę sobie zbudować Adapter nad dowolnie zorganizowanym kodem i nakarmić nim abstrakcje.

### Budowanie ramki danego formatu:

Każdy format ramki unicastowej musi na 100% definiować odbiorcę oraz nadawcę wiadomości, żeby w ogóle taka ramka miała sens. W wielu sytuacjach np. broadcast ma swój specjalny adres, podobnie urządzenia, które nie mają jeszcze przydzielonego adresu, a chciałyby coś wysłać (patrz: DHCP).

Oczywiście, formatów ramek jest wiele - mogą definiować mnóstwo dodatkowych rzeczy, takich jak typ ramki, jakieś dodatkowe metadane itp. Interfejs FrameBuildera powinien uwzględniać takie sytuacje, stąd też w tym obiekcie przewiduje następujący interfejs (format ramek jest podawany w konstruktorze - jest to klasa od której oczekuje się istnienia metody buildFrame, więc jest to pewnego rodzaju "adapter"):

* `to(receiverAddr)` - do kogo chcemy wysyłać? `receiverAddr` jest dowolny i zależny od formatu ramki 'pod spodem'.
* `from(senderAddr)` - jako kto chcemy się przedstawić? W przypadku `senderAddr` sytuacja jest analogiczna do `receiverAddr`.
* `with(key, value)` - jakaś dodatkowa wartość, którą chcemy przekazać do formatu ramki pod spodem.
* `create(data)` - ostateczna metoda, która buduje nam ramkę korzystając ze wcześniej zdefiniowanych informacji na jej temat. Format ramek pod spodem powinien wiedzieć jak odpowiedzieć na metodę "buildFrame" - w szczególności oczekuje się od niego że zacznie krzyczeć jak nie dostarczymy mu wszystkich danych - dowolny exception przeleci dalej i zostanie opakowany w bardziej 'generyczną' odmianę z poziomu FrameBuildera ;). Ew. taka metoda może sobie trzymać jakieś dobre domyślne wartości i nie krzyczeć jak mu się jakichś dodatkowych informacji nt. tego konkretnego formatu ramki nie dostarczy. Ta metoda rzuca FrameCreationFailed jak implementacji pod spodem nie spodoba się zestaw danych, jakich jej dostarczyliśmy.

Przykład użycia:
      builder = FrameBuilder(KKFrameFormatAdapter())
      builder.from(1).to(2).with("frameType", "E")
      builder.create("Ramka 1") # => zwraca ramkę
      builder.create("Ramka 2") # => następna ramka... te same metadane, inna zawartość danych.
      builder.to(3)
      builder.create("Ramka 3") # => ta ramka już pójdzie do innego gościa - reszta metadanych się nie zmiena.
      # itd...

### Odczytywanie ramki danego formatu:

Dostaliśmy strumień bajtów, teraz chcemy przerobić to na obiekt ramki. Tym zajmuje się klasa `FrameDeserializer`. Jest to klasa abstrakcyjna, którą należy dziedziczyć (albo lepiej - totalnie olać dziedziczenie i po prostu dostarczyć metody deserialize) przy każdym formacie ramki. Interfejs jest niewielki:

* deserialize(framebytes) - bierze strumień bajtów i zwraca ramkę. W szczególności może zwrócić None, gdy podane bajty nijak nie składają się w ramkę danego formatu lub też jest ona niewłaściwa (CRC niezgodne, itp.)

## Pobieranie/wysyłanie ramek z/do karty sieciowej:

No, skoro potrafimy już budować i czytać ramki, powinniśmy teraz dostarczyć abstrakcję do ich wysyłania i odbierania. O ile wysyłanie w naszym podejściu jest trywialne (zakładamy, że ramka danego formatu umie się zserializować do stringa), to czytanie wcale takie łatwe nie jest. Dlatego też każdy format ramki musi dostarczyć nam metody na czytanie ramek z bufora (obiekt z metodą `readFrame(nicHandler, deadline)`) - podajemy to w konstruktorze. Taki strumień bajtów jest odczytywany przez deserializer, po czym dostajemy już gotową ramkę. W tym sensie FrameLayer łączy nam wszystkie części architektury.

Przykład użycia:
      builder = FrameBuilder(KKFrameFormatCreateAdapter()).from(myAddr).to(receiverAddr)
      deserializer = KKFrameFormatDeserializer()
      reader = KKFrameReader() # czyta ramki i zwraca strumienie bajtów nimi będące.
      
      frameLayer = FrameLayer(reader, builder, deserializer)
      frameLayer.send("Hello World!", deadline)
      frameLayer.builder().to(receiver2Addr)
      frameLayer.send("Hello World!", deadline)

      frame = frameLayer.recv(deadline) # zwraca nam gotową ramkę lub None jak nie udało się przeczytać.

## Konkluzje:

Trochę mi się nie podoba aktualny stan - być może jednak trzeba zrezygnować z frame buildera i założyć, że każdy wie tutaj co to duck typing? :) Pozostawiam to Waszej opinii.




