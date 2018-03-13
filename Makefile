# uhidctl Makefile
#
UNAME := $(shell uname)

DESTDIR ?=
PREFIX  ?= /usr/local
BINDIR ?= $(PREFIX)/bin

INSTALL		:= install
INSTALL_DIR	:= $(INSTALL) -m 755 -d
INSTALL_PROGRAM	:= $(INSTALL) -m 755
RM		:= rm -rf

CC ?= gcc
CFLAGS ?= -g -O0
CFLAGS += -Wall -Wextra -std=c99 -pedantic
GIT_VERSION := $(shell git describe --match "v[0-9]*" --abbrev=8 --dirty --tags | cut -c2-)
CFLAGS += -DPROGRAM_VERSION=\"$(GIT_VERSION)\"
ifeq ($(GIT_VERSION),)
	GIT_VERSION := $(shell cat VERSION)
endif

HIDAPI := hidapi

ifeq ($(UNAME),Linux)
	# Use hidapi-libusb backend on Linux
	HIDAPI := hidapi-libusb

	# Use hardening options on Linux
	LDFLAGS += -Wl,-zrelro,-znow
endif

# Use pkg-config if available
ifneq (,$(shell which pkg-config))
	CFLAGS  += $(shell pkg-config --cflags ${HIDAPI})
	LDFLAGS += $(shell pkg-config --libs ${HIDAPI})
else
# But it should still build if pkg-config is not available (e.g. Linux or Mac homebrew)
	LDFLAGS += -l${HIDAPI}
endif

PROGRAM = uhidctl

$(PROGRAM): $(PROGRAM).c
	$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

install:
	$(INSTALL_DIR) $(DESTDIR)$(BINDIR)
	$(INSTALL_PROGRAM) $(PROGRAM) $(DESTDIR)$(BINDIR)

clean:
	$(RM) $(PROGRAM).o $(PROGRAM).dSYM $(PROGRAM)
