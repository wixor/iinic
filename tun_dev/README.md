tun_dev
=======

Kod tutaj zawarty pozwala na utworzenie urządzenia TUN i przekazywanie pakietów IPv4 (w obie strony) z tego urządzenia do Pythona. Po drodze są nieładnie wykorzystane sockety Unix domain. `tun_dev.py` to (bardzo) przykładowy kod odbierający pakiety od `tun_dev` i wyciągający z nich typ protokołu wyższej warstwy, nadawcę i odbiorcę. Ten skrypt wymaga aby jako argument podać mu nazwę socketa `tun_dev` (`tun_dev` wypluwa to w `unix socket: ...`) co oczywiście jest rozwiązniem przejściowym.

Przykład
--------

W jednej konsoli:

```
# ./tun_dev 192.168.8.1
```

W `jednej + 1` konsoli:

```
# route add 192.168.9.1 tun0
# ./tun_dev.py to_co_wypluł_tun_dev_w_unix_socket
```

W `jednej + 2` konsoli:

```
$ ping 192.168.9.1
```
