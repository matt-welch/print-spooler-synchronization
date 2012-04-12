FILENAME:	README.txt
NAME:		Print Spooler Synchronization README
AUTHOR: 	James Matthew Welch [JMW]
SCHOOL:		Arizona State University
CLASS:		CSE430::Operating Systems
INSTRUCTOR:	Dr. Violet Syrotiuk
SECTION:	19464
TERM:		Spring 2012

DESCRIPTION:
	This program is an implementation of a print spooler using thread
synchronization with POSIX threads (pthreads) library.
	In shared memory multicore architectures, threads can be used to
implement parallelism. A standardized C language threads programming
interface has been specified by the IEEE POSIX 1003.1c standard.
Implementations that adhere to this standard are referred to as POSIX
threads, or pthreads. The purpose of this project is to gain some experience
using pthreads to create and synchronize threads using semaphores.
	In this project, a system of N PROCESSORS will be simulated, each of
which is connected to a PRINTER via a print SPOOLER, using a total of N+2
threads: one for each processor, one for the PRINTER, one for the SPOOLER.
Each thread simulating a PROCESSOR Pi, 1<=i<=n, executes a pseudo-program
found in the file "progi.txt", consisting of a sequence of commands.

ASSUMPTIONS ABOUT RUNTIME ENVIRONMENT: 
1) pthreads library is available in g++ through -lpthread flag
2) program files "progi.txt" are present in the local directory
2a)	instructions and arguments will be space-delimited in each progi.txt

COMPILATION INSTRUCTIONS: 
	After unzipping the files in a local directory, compile the program 
with the "make" command to build the default target with g++ flags: 
	-Wall -pedantic -lpthread -O0
	
EXECUTION INSTRUCTIONS:
	Run the program with the command: "./spooler N", where N is the number 
of processor threads desired (between 1 and 10).

 
