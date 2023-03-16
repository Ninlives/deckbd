PREFIX = /usr

deckbd: deckbd.c
	$(CC) deckbd.c -o deckbd $(shell pkg-config --libs --cflags libevdev glib-2.0)

install: deckbd
	install -m 755 -D -t $(PREFIX)/bin deckbd
