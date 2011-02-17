CC = mpicc

all: cpumem filegen fileio block mm oe pi prime shearsort sndrcv

cpumem: cpumem.c
	$(CC) -o cpumem cpumem.c

filegen: filegen.c
	$(CC) -o filegen filegen.c

fileio: fileio.c
	$(CC) -o fileio fileio.c

block: fileio_block.c
	$(CC) -o fileio_block fileio_block.c

mm: mm.c
	$(CC) -o mm mm.c

oe: oetsort.c
	$(CC) -o oetsort oetsort.c

pi: pi.c
	$(CC) -o pi pi.c

prime: prime.c
	$(CC) -o prime prime.c

shearsort: shearsort.c
	$(CC) -o shearsort shearsort.c

sndrcv: sndrcv.c
	$(CC) -o sndrcv sndrcv.c

clean:
	rm -f cpumem filegen fileio fileio_block mm oetsort pi prime shearsort sndrcv

rebuild: clean all
