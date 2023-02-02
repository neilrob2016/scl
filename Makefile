
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
	INCPATH=-I/usr/local/openssl/include
	LIBPATH=-L/usr/local/openssl/lib
endif
CC=cc

# With SSL installed
ARGS=$(INCPATH) -Wall -Wextra -pedantic -D IP6 -D SCTP -D SSLYR
#ARGS=$(INCPATH) -Wall -pedantic -D IP6 -D SCTP -D SSLYR
LIBS=$(LIBPATH) -lcrypto -lssl

# Without SSL installed
#ARGS=$(INCPATH) -Wall -pedantic -D IP6 -D SCTP 
#LIBS=$(LIBPATH) -lcrypto

OBJS=main.o io.o network.o telopt.o ssl.o term.o
DEPS=globals.h Makefile
BIN=scl

$(BIN): build_date $(OBJS) Makefile
	$(CC) $(OBJS) $(LIBS) -o $(BIN)

main.o: main.c build_date.h $(DEPS)
	$(CC) $(ARGS) -c main.c

io.o: io.c $(DEPS)
	$(CC) $(ARGS) -c io.c

network.o: network.c $(DEPS)
	$(CC) $(ARGS) -c network.c

telopt.o: telopt.c $(DEPS)
	$(CC) $(ARGS) -c telopt.c

ssl.o: ssl.c $(DEPS)
	$(CC) $(ARGS) -c ssl.c

term.o: term.c $(DEPS)
	$(CC) $(ARGS) -c term.c

build_date:
	echo "#define BUILD_DATE \"`date -u +'%F %T %Z'`\"" > build_date.h

clean:
	rm -f $(BIN) $(OBJS) build_date.h 
