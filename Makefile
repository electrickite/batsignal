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
	$(CC) $(LDLIBS) -o $(NAME) $(OBJ) $(LDFLAGS)

$(OBJ): $(SRC)

clean:
	@echo cleaning
	$(RM) $(NAME) $(OBJ)

install: all
	@echo installing in $(DESTDIR)$(PREFIX)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(NAME) $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(NAME)
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < $(NAME).1 > $(DESTDIR)$(MANPREFIX)/man1/$(NAME).1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/$(NAME).1

uninstall:
	@echo removing files from $(DESTDIR)$(PREFIX)
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(NAME)
	$(RM) $(DESTDIR)$(MANPREFIX)/man1/$(NAME).1
