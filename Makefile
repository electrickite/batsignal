.POSIX:

NAME = batsignal

CC.$(CC)=$(CC)
CC.=cc
CC.c99=cc
CC:=$(CC.$(CC))

RM = rm -f
INSTALL = install
SED = sed
GREP = grep
CUT = cut

VERSION != $(GREP) VERSION version.h | $(CUT) -d \" -f2

PREFIX = /usr/local

MANPREFIX.$(PREFIX)=$(PREFIX)/share/man
MANPREFIX./usr/local=/usr/local/man
MANPREFIX.=/usr/share/man
MANPREFIX=$(MANPREFIX.$(PREFIX))

INCLUDES != pkg-config --cflags libnotify
CFLAGS_EXTRA = -pedantic -Wall -Wextra -Werror -Wno-unused-parameter -Os
CFLAGS := $(CFLAGS_EXTRA) -std=c11 $(INCLUDES) $(CFLAGS)

LIBS != pkg-config --libs libnotify
LIBS := $(LIBS) -lm
LDFLAGS_EXTRA = -s
LDFLAGS := $(LDFLAGS_EXTRA) $(LDFLAGS)

all: $(NAME) $(NAME).1

$(NAME).o: version.h

$(NAME): $(NAME).o
	$(CC) -o $(NAME) $(LDFLAGS) $(NAME).o $(LIBS)

$(NAME).1: $(NAME).1.in version.h
	$(SED) "s/VERSION/$(VERSION)/g" < $(NAME).1.in > $@

install: all
	@echo Installing in $(DESTDIR)$(PREFIX)
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -d $(DESTDIR)$(MANPREFIX)/man1
	$(INSTALL) -m 0755 $(NAME) $(DESTDIR)$(PREFIX)/bin/
	$(INSTALL) -m 0644 $(NAME).1 $(DESTDIR)$(MANPREFIX)/man1/

install-service: install
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/lib/systemd/user
	$(INSTALL) -m 0644 $(NAME).service $(DESTDIR)$(PREFIX)/lib/systemd/user/

uninstall:
	@echo Removing files from $(DESTDIR)$(PREFIX)
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(NAME)
	$(RM) $(DESTDIR)$(MANPREFIX)/man1/$(NAME).1
	$(RM) $(DESTDIR)$(PREFIX)/lib/systemd/user/$(NAME).service

clean-all: clean clean-images

clean:
	@echo Cleaning build files
	$(RM) $(NAME) $(NAME).o $(NAME).1

clean-images: arch-clean debian-stable-clean debian-testing-clean ubuntu-latest-clean fedora-latest-clean

%-clean: test/Dockerfile.%
	@echo Removing images for $*
	-docker container prune --force --filter="label=$(NAME)-$*"
	-docker rmi -f $(NAME)-$*

test: compile-test

compile-test: arch-test debian-stable-test debian-testing-test ubuntu-latest-test fedora-latest-test
	@echo Completed compile testing

%-test: test/Dockerfile.%
	@echo Starting compile test for $*
	docker build --label=$(NAME)-$* --tag=$(NAME)-$* --file=$< .
	docker run -it $(NAME)-$*
