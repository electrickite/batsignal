.POSIX:

NAME = batsignal

VERSION = $(shell grep VERSION version.h | cut -d \" -f2)

PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

CC = cc
LD = ld
CFLAGS = -std=c99 -pedantic -Wall -Wextra -Werror -Wno-unused-parameter -Os -s -D_GNU_SOURCE $(shell pkg-config --cflags libnotify)
LDLIBS = $(shell pkg-config --libs libnotify)

SRC = main.c version.h
OBJ = main.o

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) -o $(NAME) $(OBJ) $(CFLAGS) $(EXTRA_CFLAGS) $(LDLIBS) $(LDFLAGS)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

install: all
	@echo Installing in $(DESTDIR)$(PREFIX)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(NAME) $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(NAME)
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < $(NAME).1 > $(DESTDIR)$(MANPREFIX)/man1/$(NAME).1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/$(NAME).1

uninstall:
	@echo Removing files from $(DESTDIR)$(PREFIX)
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(NAME)
	$(RM) $(DESTDIR)$(MANPREFIX)/man1/$(NAME).1

clean-all: clean clean-images

clean:
	@echo Cleaning build files
	$(RM) $(NAME) $(OBJ)

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
