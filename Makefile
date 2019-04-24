
OS = -DMAC
#OS = -DLINUX

CFLAGS = -O -Wall ${INC} ${OS}
#CFLAGS = -g -Wall -DDEBUG ${INC} ${OS}
#CFLAGS = -g -Wall -Werror -DDEBUG ${INC} ${OS}

PROGS =	wavtags

wavtags: wavtags.o libwav.o
	cc -o $@ wavtags.o libwav.o

clean:
	rm -f *.o

clobber: clean
	rm -f ${PROGS}
