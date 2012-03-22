spooler: main.o
	g++ main.o -o spooler $(CONFIG)

main.o: main.cpp
	g++ -c main.cpp -o main.o $(CONFIG)
	
clean:
	rm -f spooler *.o core core.*

tidy: clean
	rm -f *.*~ *~

DEBUG_FLAGS = -g3 -ggdb -O0 -Wall -pedantic -DDEBUG -lpthread
CONFIG		= -Wall -pedantic -lpthread
EASY_FLAGS	= 

debug: CONFIG=$(DEBUG_FLAGS)
debug: spooler

easy: CONFIG=$(EASY_FLAGS)
easy: spooler

test: debug
	./spooler
	