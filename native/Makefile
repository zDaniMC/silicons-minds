# Un solo comando:  make test
CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Werror

all: libhuella.so

# Exactamente la línea del documento (gcc -shared -fPIC -O2), más advertencias.
libhuella.so: huella.c huella.h
	$(CC) $(CFLAGS) -shared -fPIC -o $@ huella.c

test_huella: test_huella.c huella.c huella.h
	$(CC) $(CFLAGS) -o $@ test_huella.c huella.c

test: libhuella.so test_huella
	./run_tests.sh

clean:
	rm -f libhuella.so test_huella

.PHONY: all test clean
