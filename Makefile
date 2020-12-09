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
	install -m 0755 ntq.sh ${DESTDIR}${PREFIX}/bin/ntq
	ln -sf /usr/bin/atrm ${DESTDIR}${PREFIX}/bin/ntrm

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/nt ${DESTDIR}${PREFIX}/bin/ntq ${DESTDIR}${PREFIX}/bin/ntrm

.PHONY: clean install uninstall
