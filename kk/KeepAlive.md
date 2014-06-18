KeepAlive -- instrukcja obsługi

Protokół KeepAlive ('k') sprawia, że urządzenia rozmawiają:


#1: Hej, ja żyję!

#2: OK, będę pamiętał.


Urządzenie #2 zapamiętuje informację o istnieniu #1 i zapisuje w pamięci na TTL sekund.
Po TTL sekundach urządzenie #2 uznaje, że skoro tak dawno nie słyszało nic od #1, to w takim razie musi on nie żyć.

Każde urządzenie wysyła ramkę KeepAlive co TTL/postsPerLife. 
Jeśli prawdopodobieństwo dojścia ramki jest nie mniejsze niż 1/postsPerLife, to sieć powinna działać przyzwoicie.


Parametry modyfikowalne (z poziomu kodu):
   TTL -- ile czasu będzie istniał wpis w liście sąsiedztwa
   postsPerLife -- ile razy w ciągu TTL maszynka wysyła TTL

