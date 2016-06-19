CC = cc
CFLAGS = -O -Wuninitialized -Wimplicit
.SUFFIXES:
OBJS = miniscm.o extra.o get_line.o
DBG =

all : miniscm

miniscm : $(OBJS)
	$(CC) $(CFLAGS) $(DBG) -o miniscm $(OBJS)

%.o: %.c
	$(CC) -c $(CFLAGS) $(DBG) $<

miniscm.o: miniscm.c miniscm.h get_line.h
extra.o: extra.c miniscm.h
get_line.o: get_line.c get_line.h
