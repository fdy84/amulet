CC 	= gcc
AR = ar
INSTALL	= install
IFLAGS  = -I/usr/local/include
CFLAGS	= $(cflags)
PREDEF = 
LDFLAGS = -L/usr/local/lib

LINK	= -Wl
OBJS	= json_parser.o jpw.o jpw_objects.o
DIRS	= 
WARN    = $(warn)


.PHONY: clean

.c.o:
	$(CC) $(WARN) -c $*.c $(CFLAGS) $(IFLAGS) $(PREDEF)

#jpw: $(OBJS)
#	$(CC) $(IFLAGS) -o $@ $(DEBUG) $(OBJS) $(LINK) $(LDFLAGS)

libjpw.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	mv -f $@ ../../../lib/

clean:
	rm -f $(OBJS) ../../../lib/libjpw.a


