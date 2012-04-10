spooler: main.cpp
	g++ main.cpp -o spooler $(CONFIG) -lpthread

clean:
	rm -f spooler *.o core core.*

tidy: clean
	rm -f *.*~ *~

DEBUG_FLAGS = -g3 -ggdb -O0 -Wall -pedantic -DDEBUG
CONFIG		= -Wall -pedantic
EASY_FLAGS	= 

debug: CONFIG=$(DEBUG_FLAGS)
debug: spooler

easy: CONFIG=$(EASY_FLAGS)
easy: spooler

test: debug
	./spooler
