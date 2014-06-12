# Prostackie potwierdzenia dla projektu backoffów
Autor: Marcin Grzywaczewski

## Jak?
Korzystając z ustalonego formatu ramek, wysyłamy zwykłą ramkę z odpowiednim typem (ja wybrałem typ 'c', jak confirm - powierdzenie). 
W takiej sytuacji odpowiedni protokół 'wie', że ma odpowiedzieć potwierdzeniem na taką ramkę.

## Format danych:
Format danych ramki z potwierdzeniami to:
XX.....

Gdzie XX to dwubajtowy (domyślnie, do zmiany w parametrze konstruktora) unikalny identyfikator ramki. Identyfikator takiej ramki musi być unikalny dla każdego odbiorcy.

Potwierdzenie takiej ramki wygląda następująco:
XX

Gdzie XX - ten sam identyfikator.

## Jak bawić się tymi pakietami?

Należy stworzyć sobie instancję protokołu AcksProto, który pozwala nam wysyłać odpowiednie pakiety. Jest on także obiektem rejestrowalnym w Dispatcherze od Karola Konaszyńskiego.

Po wywołaniu Send(doKogo, wiadomość, deadline) otrzymujemy obiekt Promise - możemy definiować w nim callbacki (argumenty jakie otrzymamy to czas otrzymania przez urządzenie potwierdzenia [rzeczywisty] oraz sama ramka) dot. sukcesu (czyli otrzymaliśmy potwierdzenie na czas) i porażki (czyli nie dostaliśmy potwierdzenia na czas). Służą do tego metody onSuccess oraz onFailure w tym promise. Obie przyjmują tylko callback. Co więcej, niezależnie od stanu pakietu, mamy 100% pewności, że nasz callback się wykona - jeżeli np. zarejestrowaliśmy callback dot. sukcesu po tym, jak już osiągnęliśmy sukces, taki callback również zostanie wykonany.
