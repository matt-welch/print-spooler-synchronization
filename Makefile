spooler: main.cpp
	g++ main.cpp -o spooler $(CONFIG)

clean:
	rm -f spooler *.o core core.*

tidy: clean
	rm -f *.*~ *~

DEBUG_FLAGS = -g3 -ggdb -O0 -Wall -pedantic -DDEBUG -lpthread
CONFIG		= -Wall -pedantic -lpthread
EASY_FLAGS	= -lpthread

debug: CONFIG=$(DEBUG_FLAGS)
debug: spooler

easy: CONFIG=$(EASY_FLAGS)
easy: spooler

test: debug
	./spooler
	