/*******************************************************************************
 * FILENAME:	main.cpp
 * NAME:		Print Spooler Synchroization
 * AUTHOR: 		James Matthew Welch [JMW]
 * SCHOOL:		Arizona State University
 * CLASS:		CSE430::Operating Systems
 * INSTRUCTOR:	Dr. Violet Syrotiuk
 * SECTION:		19464
 * TERM:		Spring 2012
 * DESCRIPTION:
 * 		This program is an implementation of a print spooler using thread
 * synchronization with POSIX threads (pthreads) library.
 * 		In shared memory multicore architectures, threads can be used to
 * implement parallelism. A standardized C language threads programming
 * interface has been specified by the IEEE POSIX 1003.1c standard.
 * Implementations that adhere to this standard are referred to as POSIX
 * threads, or pthreads. The purpose of this project is to gain some experience
 * using pthreads to create and synchronize threads using semaphores.
 * 		In this project, a system of N PROCESSORS will be simulated, each of
 * which is connected to a PRINTER via a print SPOOLER, using a total of N+2
 * threads: one for each processor, one for the PRINTER, one for the SPOOLER.
 * Each thread simulating a PROCESSOR Pi, 1<=i<=n, executes a pseudo-program
 * found in the file "progi.txt", consisting of a sequence of commands.
 ******************************************************************************/

#include <iostream>
using std::cout;
using std::endl;
using std::cin;

#include <fstream>
using std::ifstream;

#include <istream>
using std::getline;

#include <string>
using std::string;

#include <pthread.h>

int main(int argc, char* argv[]){
	// local variables:
	ifstream infile;
	string expression;

	// default filename: multiple files of form "progi.txt"
	string inFileName = "../input/prog1.txt";

	// Open graph file for reading
	infile.open((char*)inFileName.c_str());

	if(infile.fail()){
		cout << endl <<"An error occurred while reading from the file \""
				<< inFileName << "\"." << endl;
		return(-1);
	}else{
		// read the pseudocode from the files available.  start with default
		while(infile.good()){
			getline(infile, expression); // read in the number of nodes from the first line
			cout << expression << endl;
		}
		infile.close();
	}
}
