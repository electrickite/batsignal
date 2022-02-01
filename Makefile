.POSIX:

NAME = batsignal
VERSION != grep VERSION version.h | cut -d \" -f2

CC = cc
RM = rm -f
INSTALL = install
SED = sed

PREFIX = /usr/local
MANPREFIX = $(PREFIX)/man

INCLUDES != pkg-config --cflags libnotify
CFLAGS_EXTRA = -pedantic -Wall -Wextra -Werror -Wno-unused-parameter -Os
CFLAGS := $(CFLAGS_EXTRA) -std=c11 $(INCLUDES)

LIBS != pkg-config --libs libnotify
LIBS := $(LIBS) -lm
LDFLAGS_EXTRA = -s
LDFLAGS := $(LIBS) $(LDFLAGS_EXTRA)

all: $(NAME) $(NAME).1

$(NAME).o: version.h

$(NAME): $(NAME).o
	$(CC) -o $(NAME) $(NAME).o $(LDFLAGS)

$(NAME).1: $(NAME).1.in version.h
	$(SED) "s/VERSION/$(VERSION)/g" < $(NAME).1.in > $@

install: all
	@echo Installing in $(DESTDIR)$(PREFIX)
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -d $(DESTDIR)$(MANPREFIX)/man1/
	$(INSTALL) -m 0755 $(NAME) $(DESTDIR)$(PREFIX)/bin/
	$(INSTALL) -m 0644 $(NAME).1 $(DESTDIR)$(MANPREFIX)/man1/

uninstall:
	@echo Removing files from $(DESTDIR)$(PREFIX)
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(NAME)
	$(RM) $(DESTDIR)$(MANPREFIX)/man1/$(NAME).1

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
