CC=gcc -Wall ${CFLAGS}

BIN=bin
SRC=src

all: player report

player: $(BIN)/player

report: $(SRC)/report.tex
	pdflatex -output-directory=$(BIN) -jobname=$@ $^

$(BIN)/player: $(SRC)/player.c $(BIN)/audio.o
	$(CC) -o $@ $^

$(BIN)/audio.o: $(SRC)/sysprog-audio/audio.c
	$(CC) -c -o $@ $^

.PHONY: clean

clean:
	rm $(BIN)/*.o
	rm $(BIN)/*.aux
	rm $(BIN)/*.log

mrproper:
	rm $(BIN)/*
