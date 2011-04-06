CC 	= gcc
INSTALL	= install
IFLAGS  = -I/usr/local/include 
CFLAGS	= -g
PREDEF = 
LDFLAGS	= -L/usr/local/lib -lpthread

LINK	= -Wl
OBJS	= rspool.o test_main.o


WARN_GCC4 = -Wsystem-headers -Werror -Wall -Wno-format-y2k -W \
	-Wno-unused-parameter -Wstrict-prototypes \
	-Wmissing-prototypes -Wpointer-arith -Wreturn-type \
	-Wcast-qual -Wwrite-strings -Wswitch -Wshadow \
	-Wcast-align -Wunused-parameter -Wchar-subscripts \
	-Winline -Wnested-externs -Wredundant-decls -Wno-pointer-sign

WARN    = $(WARN_GCC4)

.PHONY: clean default

default: test

.c.o:	
	$(CC) $(WARN) -c $*.c $(CFLAGS) $(IFLAGS) $(PREDEF)

test: $(OBJS)
	$(CC) $(IFLAGS) -o $@ $(DEBUG) $(OBJS) $(LINK) $(LDFLAGS)

clean:
	rm -f *.o rspool

