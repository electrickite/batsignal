.POSIX:

NAME = batsignal

VERSION = $(shell grep VERSION version.h | cut -d \" -f2)

PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

CFLAGS_EXTRA = -pedantic -Wall -Wextra -Werror -Wno-unused-parameter -Os -s
CFLAGS := $(CFLAGS_EXTRA) $(CFLAGS) -std=c99 $(shell pkg-config --cflags libnotify)
LDFLAGS := $(shell pkg-config --libs libnotify) -lm $(LDFLAGS)

all: $(NAME) $(NAME).1

$(NAME).o: version.h

$(NAME): $(NAME).o
	$(CC) -o $(NAME) $(NAME).o $(LDFLAGS)

$(NAME).1: $(NAME).1.in
	sed "s/VERSION/$(VERSION)/g" < $< > $@

install: all
	@echo Installing in $(DESTDIR)$(PREFIX)
	install -Dm755 -t $(DESTDIR)$(PREFIX)/bin $(NAME)
	install -Dm644 -t $(DESTDIR)$(MANPREFIX)/man1 $(NAME).1

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
