# See LICENSE file for copyright and license details.

VERSION = 1.0

PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

INCS = -I. -I/usr/include
LIBS = -L/usr/lib -lc

CPPFLAGS = -DVERSION=\"${VERSION}\" -D_GNU_SOURCE
CFLAGS = -std=c99 -pedantic -Wall -Wextra -Os ${INCS} ${CPPFLAGS} $(shell pkg-config --cflags libnotify)
LDFLAGS = ${LIBS} $(shell pkg-config --libs libnotify)

CC = cc
LD = ld
