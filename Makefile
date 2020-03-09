CC	 	= g++
LD	 	= g++
CFLAGS	 	= -std=c++11 -Wall -g

LDFLAGS	 	= 
DEFS 	 	=

all:	clean sendfile recvfile

sendfile: sendfile.cc common.cc
	$(CC) $(DEFS) $(CFLAGS) $(LIB) -pthread sendfile.cc -o sendfile

recvfile: recvfile.cc common.cc
	$(CC) $(DEFS) $(CFLAGS) $(LIB) -pthread recvfile.cc -o recvfile

clean:
	rm -f *.o
	rm -f *~
	rm -f core.*.*
	rm -f recvfile
	rm -f sendfile
	rm -rf *.dSYM