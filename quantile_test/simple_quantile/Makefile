CC = gcc
# CFLAGS = -Wall -Wextra
PKGCONF ?= pkg-config
CFLAGS = $(shell $(PKGCONF) --cflags libdpdk)
LIBS = -lpcap
LDFLAGS_SHARED = $(shell $(PKGCONF) --libs libdpdk)
LDFLAGS_STATIC = $(shell $(PKGCONF) --static --libs libdpdk)

simple_quantile: simple_quantile.c
	$(CC) $(CFLAGS) -o simple_quantile simple_quantile.c $(LIBS) $(LDFLAGS) $(LDFLAGS_SHARED) 

clean:
	rm -f simple_quantile
