CC=gcc -Wall ${CFLAGS}

BIN=bin
SRC=src

all: player server client

player: $(BIN)/player

server: $(BIN)/audioserver

client: $(BIN)/audioclient

report: $(SRC)/report.tex
	pdflatex -output-directory=$(BIN) -jobname=$@ $^

$(BIN)/%: $(SRC)/%.c $(BIN)/audio.o $(BIN)/deadbeef.o
	$(CC) -o $@ $^

$(BIN)/audio.o: $(SRC)/sysprog-audio/audio.c
	$(CC) -c -o $@ $^

$(BIN)/deadbeef.o: $(SRC)/deadbeef.c
	$(CC) -c -o $@ $^

.PHONY: clean mrproper player server client report

clean:
	rm -f $(BIN)/*.o
	rm -f $(BIN)/*.aux
	rm -f $(BIN)/*.log

mrproper:
	rm -f $(BIN)/*
