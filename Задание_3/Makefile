CC = gcc
CFLAGS = -O2 -std=c11 -Wall -Wextra -D_XOPEN_SOURCE=700 -pthread

all: custom native

custom: pth_ll_rwl.c my_rwlock.c my_rwlock.h my_rand.c my_rand.h
	$(CC) $(CFLAGS) -DUSE_CUSTOM pth_ll_rwl.c my_rwlock.c my_rand.c -o custom

native: pth_ll_rwl.c my_rand.c my_rand.h
	$(CC) $(CFLAGS) pth_ll_rwl.c my_rand.c -o native

bench: all
	python3 benchmark.py

clean:
	rm -f custom native

.PHONY: all bench clean
