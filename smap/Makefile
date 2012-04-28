CC = gcc
AR = ar


CFLAG = -g -W -Wall -fPIC
INCPATH = -I.

TEST_TARGET = libsmap.a
TEST_OBJ = smap.o rwlock.o
TEST_SRC = smap.c rwlock.c

all : $(TEST_TARGET)

$(TEST_OBJ): $(TEST_SRC)
	$(CC) $(CFLAG) $(INCPATH) -c $(TEST_SRC)

$(TEST_TARGET) : $(TEST_OBJ)
	$(AR) rcv libsmap.a $(TEST_OBJ)
# $(AR) $(TRGLINKOPT) cq $@ $^
# $(CC) -o $@ $(TEST_OBJ) $(LIBS)

# clean
clean:
	rm -f *.o $(TEST_TARGET) 
