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

projet-syr2-pinsard.tar.gz: report
	tar zcf $@ src/* Makefile LICENSE README.md bin/report.pdf

.PHONY: clean mrproper shmclean player server client report

clean:
	rm -f $(BIN)/*.o
	rm -f $(BIN)/*.aux
	rm -f $(BIN)/*.log

mrproper: clean
	rm -f $(BIN)/*
	rm -f projet-syr2-pinsard.tar.gz

shmclean:
	ipcs -m | awk '{ if ($$6=="0") system("ipcrm shm "$$2""); }'
