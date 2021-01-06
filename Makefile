PREFIX = /usr/local

CC = gcc
CFLAGS = -O3 -Wall -Wextra

nt: nt.c config.h
	${CC} -o $@ ${CFLAGS} nt.c

config.h:
	cp config.def.h config.h

clean:
	rm -f nt

install: nt
	mkdir -p ${DESTDIR}${PREFIX}/bin
	install -m 0755 nt ${DESTDIR}${PREFIX}/bin/nt
	install -m 0755 ntq.sh ${DESTDIR}${PREFIX}/bin/ntq
	install -m 0755 ntrm.sh ${DESTDIR}${PREFIX}/bin/ntrm
	install -m 0755 ntping.sh ${DESTDIR}${PREFIX}/bin/ntping

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/nt ${DESTDIR}${PREFIX}/bin/ntq ${DESTDIR}${PREFIX}/bin/ntrm ${DESTDIR}${PREFIX}/bin/ntping

.PHONY: clean install uninstall
