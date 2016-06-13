CC = g++
CFLAGS = -c -Wall
CCLINK = $(CC)
OBJS = tftp_server.o
RM = rm -f




all: ttftp

ttftp: $(OBJS)
	$(CC) -o ttftp $(OBJS)

tftp_server.o: tftp_server.cpp tftp_server.h
	$(CC) $(CFLAGS) -o $@ tftp_server.cpp

clean:
	rm -f ttftp *.o *~
