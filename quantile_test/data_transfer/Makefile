APP = data_transfer
SRCS-y := data_transfer.c xxhash.c

CC = gcc
PKGCONF ?= pkg-config
CFLAGS := $(CFLAGS) -Wall -Wextra $(shell $(PKGCONF) --cflags libdpdk)

LIBS = -lpcap
LDFLAGS_SHARED := $(shell $(PKGCONF) --libs libdpdk)
LDFLAGS_STATIC := $(shell $(PKGCONF) --static --libs libdpdk)

BUILD_DIR = build

$(APP): $(SRCS-y)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/$(APP) $(SRCS-y) $(LIBS) $(LDFLAGS) $(LDFLAGS_SHARED) 

clean:
	rm -f $(BUILD_DIR)/$(APP)