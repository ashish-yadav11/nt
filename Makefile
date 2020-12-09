#PREFIX = /usr/local
PREFIX = ${HOME}/.local

CC = gcc
CFLAGS = -O3 -Wall -Werror

nt: nt.c
	${CC} -o $@ ${CFLAGS} $<

clean:
	rm -f nt

install: nt
	mkdir -p ${DESTDIR}${PREFIX}/bin
	install -m 0755 nt ${DESTDIR}${PREFIX}/bin/nt

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/nt

.PHONY: clean install uninstall
