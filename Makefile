CC = cc
# CFLAGS = -std=gnu11 -O3 -I.
CFLAGS = -std=gnu11 -g -I.

LIBS=-lm
ARCH=$(uname -m)

BINS = buddyalloc

all: $(BINS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

buddyalloc: buddyalloc.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f *.o $(BINS)
