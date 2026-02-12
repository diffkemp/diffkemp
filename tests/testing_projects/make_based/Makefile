# Makefile for testing of make based project.
CC = gcc
CFLAGS = -O2 -std=c99

.PHONY: default with-assembly with-linking clean

default: file.o

# Assembly files should not be compiled to `.bc`
with-assembly: default mod.o sub.o

# Compilation with linking, should be compiled to `.bcw`
with-linking: default file.so

# Note: mod.o (mod.S) is implicitly compiled using CC, but sub.o (sub.s)
#   is implicilty compiled using AS therefore for sub.o is neccessary to
#   add explicit target to use CC (for testing cc_wrapper behaviour).
sub.o: sub.s
	$(CC) -c $@ $^

# For testing of compilation of object file to shared library
file.so: file.o
	$(CC) $(CFLAGS) -o $@ $^ -shared

clean:
	rm -f *.o *.so *.bc
