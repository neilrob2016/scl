
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
	INCPATH=-I/usr/local/openssl/include
	LIBPATH=-L/usr/local/openssl/lib
endif
CC=cc

# With SSL installed
ARGS=$(INCPATH) -Wall -pedantic -D IP6 -D SCTP -D SSLYR
LIBS=$(LIBPATH) -lcrypto -lssl

# Without SSL installed
#ARGS=$(INCPATH) -Wall -pedantic -D IP6 -D SCTP 
#LIBS=$(LIBPATH) -lcrypto

OBJS= main.o io.o network.o telopt.o ssl.o term.o
BIN=scl

$(BIN): build_date $(OBJS) Makefile
	$(CC) $(OBJS) $(LIBS) -o $(BIN)

main.o: main.c globals.h build_date.h
	$(CC) $(ARGS) -c main.c

io.o: io.c globals.h
	$(CC) $(ARGS) -c io.c

network.o: network.c globals.h
	$(CC) $(ARGS) -c network.c

telopt.o: telopt.c globals.h
	$(CC) $(ARGS) -c telopt.c

ssl.o: ssl.c globals.h
	$(CC) $(ARGS) -c ssl.c

term.o: term.c globals.h
	$(CC) $(ARGS) -c term.c

build_date:
	echo "#define BUILD_DATE \"`date -u +'%F %T %Z'`\"" > build_date.h

clean:
	rm -f $(BIN) $(OBJS) build_date.h 
