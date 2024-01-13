.POSIX:

TARGET = batsignal

CC.$(CC)=$(CC)
CC.=cc
CC.c99=cc
CC:=$(CC.$(CC))

RM = rm -f
INSTALL = install
SED = sed
GREP = grep
CUT = cut

VERSION != $(GREP) VERSION main.h | $(CUT) -d \" -f2
PROGNAME != $(GREP) PROGNAME main.h | $(CUT) -d \" -f2
PROGUPPER != $(GREP) PROGUPPER main.h | $(CUT) -d \" -f2

PREFIX = /usr/local

MANPREFIX.$(PREFIX)=$(PREFIX)/share/man
MANPREFIX./usr/local=/usr/local/man
MANPREFIX.=/usr/share/man
MANPREFIX=$(MANPREFIX.$(PREFIX))

INCLUDES != pkg-config --cflags libnotify
CFLAGS_EXTRA = -pedantic -Wall -Wextra -Werror -Wno-unused-parameter -Os
CFLAGS := $(CFLAGS_EXTRA) $(INCLUDES) $(CFLAGS)

LIBS != pkg-config --libs libnotify
LIBS := $(LIBS) -lm
LDFLAGS_EXTRA = -s
LDFLAGS := $(LDFLAGS_EXTRA) $(LDFLAGS)

SRC = main.c options.c battery.c notify.c
OBJ = $(SRC:.c=.o)
HDR = $(SRC:.c=.h)

.PHONY: all install install-service clean test compile-test

all: $(TARGET) $(TARGET).1

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(LDFLAGS) $(OBJ) $(LIBS)

%.o: $(HDR)

$(TARGET).1: $(TARGET).1.in main.h
	$(SED) "s/VERSION/$(VERSION)/g" < $(TARGET).1.in | $(SED) "s/PROGNAME/$(PROGNAME)/g" | $(SED) "s/PROGUPPER/$(PROGUPPER)/g" > $@

install: all
	@echo Installing in $(DESTDIR)$(PREFIX)
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -d $(DESTDIR)$(MANPREFIX)/man1
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/
	$(INSTALL) -m 0644 $(TARGET).1 $(DESTDIR)$(MANPREFIX)/man1/

install-service: install
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/lib/systemd/user
	$(INSTALL) -m 0644 $(TARGET).service $(DESTDIR)$(PREFIX)/lib/systemd/user/

uninstall:
	@echo Removing files from $(DESTDIR)$(PREFIX)
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	$(RM) $(DESTDIR)$(MANPREFIX)/man1/$(TARGET).1
	$(RM) $(DESTDIR)$(PREFIX)/lib/systemd/user/$(TARGET).service

clean-all: clean clean-images

clean:
	@echo Cleaning build files
	$(RM) $(TARGET) $(OBJ) $(TARGET).1

clean-images: arch-clean debian-stable-clean debian-testing-clean ubuntu-latest-clean fedora-latest-clean

%-clean: test/Dockerfile.%
	@echo Removing images for $*
	-docker container prune --force --filter="label=$(TARGET)-$*"
	-docker rmi -f $(TARGET)-$*

test: compile-test

compile-test: arch-test debian-stable-test debian-testing-test ubuntu-latest-test fedora-latest-test
	@echo Completed compile testing

%-test: test/Dockerfile.%
	@echo Starting compile test for $*
	docker build --label=$(TARGET)-$* --tag=$(TARGET)-$* --file=$< .
	docker run -it $(TARGET)-$*
