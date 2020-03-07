CC	 	= g++
LD	 	= g++
CFLAGS	 	= -std=c++11 -Wall -g

LDFLAGS	 	= 
DEFS 	 	=

all:	sendfile recvfile

sendfile: sendfile.cc
	$(CC) $(DEFS) $(CFLAGS) $(LIB) -o sendfile sendfile.cc

recvfile: recvfile.cc
	$(CC) $(DEFS) $(CFLAGS) $(LIB) -o recvfile recvfile.cc

clean:
	rm -f *.o
	rm -f *~
	rm -f core.*.*
	rm -f recvfile
	rm -f sendfile
	rm -rf *.dSYM