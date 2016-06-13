CC = g++
CFLAGS = -c -Wall
CCLINK = $(CC)
OBJS = tftp_server.o
RM = rm -f




all: ttftps

ttftps: $(OBJS)
	$(CC) -o ttftps $(OBJS)

tftp_server.o: tftp_server.cpp tftp_server.h
	$(CC) $(CFLAGS) -o $@ tftp_server.cpp

clean:
	rm -f ttftp *.o *~ ttftps
