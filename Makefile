CC=gcc -Wall ${CFLAGS}

all: bin/player

bin/player: src/player.c bin/audio.o
	$(CC) -o $@ $^

bin/audio.o: src/sysprog-audio/audio.c
	$(CC) -c -o $@ $^

.PHONY: clean

clean:
	rm bin/*.o

mrproper:
	rm bin/*
