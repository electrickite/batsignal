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
	$(CC) -o $(NAME) $(OBJ) $(LDLIBS) $(LDFLAGS)

$(OBJ): $(SRC)

clean:
	@echo Cleaning build files
	$(RM) $(NAME) $(OBJ)

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

compile-test:
	@echo Starting compile test on Arch, Debian \(Stable, Testing\), Ubuntu, Fedora
	docker build --label=$(NAME)-arch --tag=$(NAME)-arch --file=test/Dockerfile.arch .
	docker run -it $(NAME)-arch
	docker build --label=$(NAME)-debian-stable --tag=$(NAME)-debian-stable --file=test/Dockerfile.debian-stable .
	docker run -it $(NAME)-debian-stable
	docker build --label=$(NAME)-debian-testing --tag=$(NAME)-debian-testing --file=test/Dockerfile.debian-testing .
	docker run -it $(NAME)-debian-testing
	docker build --label=$(NAME)-ubuntu-latest --tag=$(NAME)-ubuntu-latest --file=test/Dockerfile.ubuntu-latest .
	docker run -it $(NAME)-ubuntu-latest
	docker build --label=$(NAME)-fedora-latest --tag=$(NAME)-fedora-latest --file=test/Dockerfile.fedora-latest .
	docker run -it $(NAME)-fedora-latest
	@echo Completed compile testing

test: compile-test

clean-images:
	@echo Cleaning docker images
	-docker container prune --force --filter="label=$(NAME)-arch"
	-docker rmi -f $(NAME)-arch
	-docker container prune --force --filter="label=$(NAME)-debian-stable"
	-docker rmi -f $(NAME)-debian-stable
	-docker container prune --force --filter="label=$(NAME)-debian-testing"
	-docker rmi -f $(NAME)-debian-testing
	-docker container prune --force --filter="label=$(NAME)-ubuntu-latest"
	-docker rmi -f $(NAME)-ubuntu-latest
	-docker container prune --force --filter="label=$(NAME)-fedora-latest"
	-docker rmi -f $(NAME)-fedora-latest

clean-all: clean clean-images
