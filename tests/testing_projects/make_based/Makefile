# Makefile for testing of make based project.
CC = gcc
CFLAGS = -O2 -std=c99

.PHONY: default with-assembly clean

default: file.o

# Assembly files should not be compiled to `.ll`
with-assembly: default mod.o sub.o

# Note: mod.o (mod.S) is implicitly compiled using CC, but sub.o (sub.s)
#   is implicilty compiled using AS therefore for sub.o is neccessary to
#   add explicit target to use CC (for testing cc_wrapper behaviour).
sub.o: sub.s
	$(CC) -c $@ $^

clean:
	rm -f *.o *.ll
