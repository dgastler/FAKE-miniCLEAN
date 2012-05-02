LANG=C
CFLAGS += -O2 -Wall
LDFLAGS += -O2 -pthread -lm

OBJECTS=main.o cpacket.o
PROGRAMS=main
ALL: $(PROGRAMS)

main : $(OBJECTS)

clean :
	rm $(OBJECTS)
	rm $(PROGRAMS)

%.o : %.c
	$(CC) $(CFLAGS) -c $<