CC 	= gcc
INSTALL	= install
IFLAGS  = -I/usr/local/include
CFLAGS	= -g
PREDEF = -DLINUX_API
LDFLAGS = -L/usr/local/lib -pthread

LINK	= -Wl
OBJS	= esm.o strlcat.o strlcpy.o
DIRS	= 
WARN1   = -Wsystem-headers -Wall -Wno-format-y2k -W \
	-Wno-unused-parameter -Wstrict-prototypes \
	-Wmissing-prototypes -Wpointer-arith -Wreturn-type \
	-Wcast-qual -Wwrite-strings -Wswitch -Wshadow \
	-Wcast-align -Wunused-parameter -Wchar-subscripts \
	-Winline -Wnested-externs -Wredundant-decls
	
WARN	= $(WARN1)

.PHONY: clean

.c.o:
	$(CC) $(WARN) -c $*.c $(CFLAGS) $(IFLAGS) $(PREDEF)

esm: $(OBJS)
	$(CC) $(IFLAGS) -o $@ $(DEBUG) $(OBJS) $(LINK) $(LDFLAGS)

clean:
	rm -f $(OBJS) esm


