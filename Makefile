PREFIX = /usr/local

CC = gcc
CFLAGS = -O3 -Wall -Wextra

nt: nt.c config.h
	${CC} -o $@ ${CFLAGS} nt.c

config.h:
	cp config.def.h $@

clean:
	rm -f nt

BINDIR = ${DESTDIR}${PREFIX}/bin

install: nt
	mkdir -p ${BINDIR}
	cp -f nt ${BINDIR}/nt
	cp -f ntq.sh ${BINDIR}/ntq
	cp -f ntrm.sh ${BINDIR}/ntrm
	cp -f ntping.sh ${BINDIR}/ntping
	chmod 755 ${BINDIR}/nt ${BINDIR}/ntq ${BINDIR}/ntrm ${BINDIR}/ntping

uninstall:
	rm -f ${BINDIR}/nt ${BINDIR}/ntq ${BINDIR}/ntrm ${BINDIR}/ntping

.PHONY: clean install uninstall
