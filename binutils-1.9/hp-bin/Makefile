CFLAGS = -O -I../hp-include
EXECUTABLES = hpxt chatr

all : $(EXECUTABLES)

clean :
	rm -f *.o $(EXECUTABLES)

hpxt : hpxt.o ioutil.o
	$(CC) $(CFLAGS) -o hpxt hpxt.o ioutil.o

chatr : chatr.o ioutil.o
	$(CC) $(CFLAGS) -o chatr chatr.o ioutil.o

hpxt.o chatr.o : ioutil.h
