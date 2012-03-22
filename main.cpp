/*******************************************************************************
 * FILENAME:	main.cpp
 * NAME:		Print Spooler Synchronization Driver
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

#include <sstream>
using std::stringstream;

#include <pthread.h>

#include <vector>
using std::vector;

#include <map>
using std::map;

#include <cstdlib> // exit()

typedef string Instruction;
typedef vector<Instruction> Program;

// local function declarations
map<int, Program> readProgramSource();
void * Processor(void *);
void * Printer(void *);
void * Spooler(void *);
const int MAX_PROCESSOR_THREADS = 10;
pthread_mutex_t CS_lock;

int main(int argc, char* argv[]){
	// local variables:
	pthread_t tid[MAX_PROCESSOR_THREADS];

	// read all program source files into a map
//	map<int, Program> TaskSet = readProgramSource();

	// process commands for all Programs
	for(int i = 1; i <= MAX_PROCESSOR_THREADS; ++i){
		// here's where the magic (synchronization) happens
		if( pthread_create(&tid[i], NULL, Processor, (void*) i) != 0 )
		{
			printf( "Error: unable to create processor thread\n" );
		}
	}

	for(int i = 1; i <= MAX_PROCESSOR_THREADS; ++i)
		pthread_join( tid[ i ], NULL );

	printf ( "\nmain() terminating\n" );

	cout.flush();
	cout << endl;
	// program's done
	return 0;
}

void *Processor( void * arg){
	int *id = (int*)&arg;
	int intID = *id;
	/*-----------------------------------------------------------*/
	ifstream infile;
	Instruction expression;
	stringstream inFileNameStream;
	string inFileName;
	stringstream message;

	// build filename: multiple files of form "progi.txt"
	inFileNameStream << "../input/prog" << intID << ".txt";
	inFileName = inFileNameStream.str();

	// Open pseudo-program file for reading
	infile.open((char*)inFileName.c_str());

	if(infile.fail()){
		message << "\nNo program matching \"" << inFileName <<
				"\" exists.\n";
	}else{
		// read the pseudocode programs from the files available
		message << "\nPrintSpooler: Program " << intID << ": \""
				<< inFileName << "\":::\n";
		Program currentProgram;
		while(infile.good()){
			getline(infile, expression);
			if(expression != ""){
				currentProgram.push_back(expression);
				message << expression << endl;
			}
		}
		infile.close();
	}
	/*-----------------------------------------------------------*/

	pthread_mutex_lock( &CS_lock);
		cout << "\nI'm Processor Thread number " << intID;
//		cout << message.str();
		cout.flush();
	pthread_mutex_unlock( &CS_lock);
	return NULL;
}

void *Printer( void *){
	return NULL;
}

void *Spooler( void *){
	return NULL;
}

map<int, Program> readProgramSource(void){
	ifstream infile;
	Instruction expression;
	stringstream inFileNameStream;
	string inFileName;
	map<int, Program> TaskSet;
	const int MAX_PROGRAMS = 10;

	for(int i = 1; i <= MAX_PROGRAMS; ++i){
		// build filename: multiple files of form "progi.txt"
		inFileNameStream << "../input/prog" << i << ".txt";
		inFileName = inFileNameStream.str();

		// Open pseudo-program file for reading
		infile.open((char*)inFileName.c_str());

		if(infile.fail()){
			cout << endl <<"No program matching \"" << inFileName <<
					"\" exists." << endl;
		}else{
			// read the pseudocode programs from the files available
			cout << endl << "PrintSpooler: Program " << i << ": \""
					<< inFileName << "\":::" << endl;
			Program currentProgram;
			while(infile.good()){
				getline(infile, expression);
				if(expression != ""){
					currentProgram.push_back(expression);
#ifdef DEBUG
					cout << expression << endl;
#endif
				}
			}
			infile.close();
			TaskSet[i] = currentProgram;
		}
		inFileNameStream.str("");
	}
	return TaskSet;
}
