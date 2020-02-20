
OS = -DMAC
#OS = -DLINUX

#CFLAGS = -O -Wall ${INC} ${OS}
CFLAGS = -g -Wall -DDEBUG ${INC} ${OS}
#CFLAGS = -g -Wall -Werror -DDEBUG ${INC} ${OS}

PROGS =	wavtags

wavtags: wavtags.o libwav.o libid3.o utf16.o
	cc -o $@ wavtags.o libwav.o libid3.o utf16.o

utf16.o: utf16.c utf16.h myendian.h

myendian.h: Tools/endian
	./Tools/endian > $@

Tools/endian: Tools/endian.c
	cc -o $@ Tools/endian.c

clean:
	rm -f *.o Tools/endian

clobber: clean
	rm -f ${PROGS}
