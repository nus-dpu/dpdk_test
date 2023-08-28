APP = QEWA_quantile
SRCS-y := QEWA_quantile.c

CC = gcc
# CFLAGS = -Wall -Wextra
PKGCONF ?= pkg-config
CFLAGS = $(shell $(PKGCONF) --cflags libdpdk)
LIBS = -lpcap
LDFLAGS_SHARED = $(shell $(PKGCONF) --libs libdpdk)
LDFLAGS_STATIC = $(shell $(PKGCONF) --static --libs libdpdk)

$(APP): $(SRCS-y)
	$(CC) $(CFLAGS) -o $(APP) $(SRCS-y) $(LIBS) $(LDFLAGS) $(LDFLAGS_SHARED) 

clean:
	rm -f $(APP)
