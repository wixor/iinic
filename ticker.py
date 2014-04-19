#!/usr/bin/python
# -*- encoding: utf-8

import sys, itertools, iinic

def main():
    if len(sys.argv) != 2:
        print 'usage: ticker.py [a|b]'
        sys.exit(1)

    # W tym miejscu tworzymy obiekt NIC, którego będziemy używać do komunikacji
    # z naszą kartą sieciową. Przekazujemy mu łącze, na drugim końcu którego
    # znajduje się owa karta. W tej chwili dostępne jest jedynie łącze przez
    # sieć (a więc nie z prawdziwą kartą, tylko z serwerem, ktory udaje, że
    # jest kartą). Docelowo zmienimy tutaj obiekt NetComm na USBComm i wszystko
    # zadziała samo z prawdziwym urządzeniem.
    nic = iinic.NIC(iinic.NetComm())

    # Tutaj określamy wiadomość, jaką będziemy wysyłali oraz co jaki czas
    # chcemy ją wysyłać. Czas jest określony w mikrosekundach i będzie pilnowany
    # przez timer na karcie sieciowej (co się za chwilę okaże).
    msg = sys.argv[1]*10 + '\n'
    delay = 1000000 if sys.argv[1] == 'a' else 850000

    # To jest pętla od i = 1 do nieskończoności
    for i in itertools.count(1):
        # "poczekaj, aż twój zegarek będzie pokazywał wartość delay * i"
        nic.timing(delay * i)
        # "wyślij wiadomość msg".
        nic.tx(msg)
        # "zatrzymaj mój program aż kolejka poleceń karty opróźni się".
        nic.sync()

        # Chciałbym tutaj zwrócić uwagę na to, jak możnaby niepoprawnie
        # napisać powyższy fragment kodu. Gdyby zamienić kolejnością
        # powyższe trzy instrukcje:
        #   nic.timing(delay * i)
        #   nic.sync()
        #   nic.tx(msg)
        # otrzymalibyśmy program, który pozornie robi to samo. Pozornie,
        # ponieważ pomiędzy wykonaniem się instrukcji sync() a wykonaniem
        # instrukcji tx() mija pewna nieokreślona ilość czasu. W takim
        # wypadku nie mielibyśmy żadnego sensownego ograniczenia na to,
        # kiedy zostanie wysłana wiadomość. W poprawnej wersji kodu
        # pokazanej powyżej, przy założeniu że wykonanie metody tx() trwa
        # istotnie krócej niż sekundę, czas wysłania jest dość precyzyjnie
        # regulowany przez zegar na karcie sieciowej.

# To uruchamia funkcję main jeżeli moduł został uruchomiony jako główny program
# (czyli np. przez `./ticker.py`).
if __name__ == '__main__':
    main()
