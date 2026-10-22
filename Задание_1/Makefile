CC     = cc
CFLAGS = -O2 -Wall -Wextra -std=c11

mandelbrot: mandelbrot.c timer.h
	$(CC) $(CFLAGS) -o $@ mandelbrot.c -lpthread

clean:
	rm -f mandelbrot mandelbrot.csv

.PHONY: clean
