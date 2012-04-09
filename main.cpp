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
#include <semaphore.h>

#include <vector>
using std::vector;

#include <map>
using std::map;

#include <queue>
using std::queue;

#include <cstdlib> // atoi(), exit()
using std::atoi;

#include <sys/time.h>
using namespace std;

typedef string Instruction;
typedef vector<Instruction> Program;

// local function declarations
void * Processor(void *);
void * Printer(void *);
void * Spooler(void *);
const int MAX_PROCESSOR_THREADS = 10;

map< int, vector< string > > buffer;  // needs a mutex
vector< string > printBuffer;  // needs a mutex

queue< string > printerQueue;
sem_t printerQueueBusy;

queue< string > spoolerQueue;
sem_t spoolerQueueBusy;

pthread_mutex_t lock_numTerm_count; // lock mutex for preventing access to buffer
pthread_mutex_t lock_spoolerQueue;
pthread_mutex_t lock_printerQueue;

sem_t buffersFull[MAX_PROCESSOR_THREADS];  // semaphore to tell spooler that jobs are available

sem_t jobsAvailable;
sem_t printsAvailable;

int numTerminated = 0; // needs a mutex
int NUM_PROCESSOR_THREADS = 0;
int numJobsSpooled = 0;
int numJobsSentToPrint = 0;
int numJobsPrinted = 0;

int main(int argc, char* argv[]){
	// get the number of processors to spawn from the command line args
	// parse input arguments
	if (argc > 1){ // argc should be 2 for correct execution
		NUM_PROCESSOR_THREADS = atoi(argv[1]);
	}else{
		NUM_PROCESSOR_THREADS = 1;
	}

#ifdef DEBUG
	cout << "Number of processor threads requested = " << NUM_PROCESSOR_THREADS << endl;
#endif

	// create thread handles
	pthread_t *tid = new pthread_t(NUM_PROCESSOR_THREADS);
	pthread_t spooler_tid;
	pthread_t printer_tid;

	// initialize mutex & semaphores
	sem_init( &jobsAvailable, 0, 0);
	sem_init( &printsAvailable, 0, 0);

	sem_init( &spoolerQueueBusy, 0, 1);
	sem_init( &printerQueueBusy, 0, 0);
	for(int i = 0; i < NUM_PROCESSOR_THREADS; ++i){
		sem_init(&buffersFull[i], 0, 0);
	}

	// create spooler thread, lock it to wait for available buffers to print
	if( pthread_create(&spooler_tid, NULL, Spooler, NULL) != 0 ){
		printf( "Error: unable to create spooler thread\n" );
	}

	// create printer thread, lock it to wait for spooler
	if( pthread_create(&printer_tid, NULL, Printer, NULL) != 0 ){
		printf( "Error: unable to create printer thread\n" );
	}

	// process commands for all Programs
	for(int i = 1; i <= NUM_PROCESSOR_THREADS; ++i){
		// here's where the magic (synchronization) happens
		if( pthread_create(&tid[i], NULL, Processor, (void*) i) != 0 ){
			printf( "Error: unable to create processor thread\n" );
		}
	}

	// processor go off and work through their jobs then join when they're done
	for(int i = 1; i <= NUM_PROCESSOR_THREADS; ++i)
		pthread_join( tid[ i ], NULL );
	// join spooler
	pthread_join(spooler_tid, NULL);
	// join printer
	pthread_join(printer_tid, NULL);

#ifdef DEBUG
	printf ( "\nmain() terminating\n" );

	cout << "num Jobs spooled: " << numJobsSpooled << endl;
	cout << "num Jobs senttoPrint: " << numJobsSentToPrint << endl;
	cout << "num Jobs printed: " << numJobsPrinted << endl;
#endif
	cout.flush();
	cout << endl;
	// program's done
	return 0;
}

void *Processor( void * arg){
	int *id = (int*)&arg;
	int intTID = *id;
	ifstream infile;
	stringstream inFileNameStream, message;
	string inFileName;
	Program currentProgram;
	stringstream localBuffer;
	bool stuffToPrint = false;
	// TODO FIX computes are N*2 to spread things apart
	const int slowdownFactor = 1;

	// variables to hold program etime
	struct timeval start, end;
	long seconds, useconds;

	// build filename: multiple files of form "progi.txt"
	inFileNameStream << "../input/prog" << intTID << ".txt";
	inFileName = inFileNameStream.str();

	// Open pseudo-program file for reading
	infile.open((char*)inFileName.c_str());

	if(infile.fail()){
		cout << "\nNo program matching \"" << inFileName <<
				"\" exists.  Thread terminating...\n";
		cout.flush();
		pthread_cancel(intTID);
	}else{
		// read the pseudocode programs from the files available

#ifdef DEBUG
		message << "\nProcessor Thread: Program " << intTID << ": \""
				<< inFileName << "\":::\n";
		cout << message.str();
#endif
		string expression;
		while(infile.good()){
			// clear exprTokens, token
			string fcnName, fcnArg;
			stringstream exprTokens;
			// get next line from the program
			getline(infile, expression);
			if(expression != ""){
				// put expression into a stringstream for token extraction
				exprTokens << expression;

				// extract tokens (assume always only two per line)
				exprTokens >> fcnName;
				exprTokens >> fcnArg;
#ifdef DEBUG
					message << "+" << fcnName << "(" << fcnArg << ")\n";
#endif
				if(fcnName == "NewJob"){
					// reset stuffToPrint to false since it's a new job and we don't know what's coming
					stuffToPrint = false;

					// start timer
					gettimeofday(&start, NULL);

					// clear the buffer
					localBuffer.str("");

					// add string to the message describing the job output
					localBuffer << "\nP" << intTID << "::Job " << fcnArg << "::" << endl;
				}else if(fcnName == "Compute"){
					// compute factorial of fcnArg
					int N = atoi(fcnArg.c_str()) * slowdownFactor;
					int product = 1;
					for(int i = N; i > 1; --i)
						product = product * i;
#ifdef DEBUG
					message << "\t" << N << "! = " << product << endl;
#endif
				}else if(fcnName == "Print"){
//					printf("I found a print argument:%s\n",fcnArg.c_str());
					// buffer print args
					localBuffer << fcnArg << endl;
					stuffToPrint = true;
				}else if(fcnName == "EndJob"){
					// send buffer to spooler/ signal spooler
					// only if last print job from this processor is done
					if(stuffToPrint){
						// if debug mode, append elapsed time to the output

						// print program elapsed time
						gettimeofday(&end, NULL);
						seconds  = end.tv_sec  - start.tv_sec;
						useconds = end.tv_usec - start.tv_usec;
						double preciseTime = seconds + useconds/1000000.0;

						localBuffer << "[etime: " << preciseTime << " s]\n" ;

						/* this is the CS where Processor threads will send their
						 * output to the Spooler thread */

						// sem wait on spooler
						pthread_mutex_lock(&lock_spoolerQueue);
							// send localBuffer to global buffer
							spoolerQueue.push(localBuffer.str());
#ifdef DEBUG
							printf("---Job spooled---%s---endjobspool---\n",localBuffer.str().c_str());
#endif
							numJobsSpooled++;
							// sem post on return
						pthread_mutex_unlock(&lock_spoolerQueue);

						// signal spooler that ANY processor has a job available so it may begin
						sem_post(&jobsAvailable);
					}

					// reset stuffToPrint
					stuffToPrint = false;
				}else if(fcnName =="Terminate"){
					// exit since should be the last line in the program
					infile.close();

					pthread_mutex_lock( &lock_numTerm_count);
						// increment number of terminated threads
						numTerminated++;
					pthread_mutex_unlock( &lock_numTerm_count);

					// post jobs available in case no jobs with printing occurred
					sem_post(&jobsAvailable);
//					pthread_exit(NULL);
#ifdef DEBUG
					printf("Processor %d terminating...\n",intTID);
#endif
					}
			}
		}
		infile.close();
	}
	return NULL;
}

void *Spooler( void *){
	// wait for a job to be available ( jobsAvailable )
	int threadCount = NUM_PROCESSOR_THREADS;
	int deadCount = 0;
	int numJobsLeft = 0;
	string job;
	stringstream message;
	bool stuffToPrint = false;

	sem_wait(&jobsAvailable);
	pthread_mutex_lock(&lock_spoolerQueue);
		numJobsLeft = spoolerQueue.size();
	pthread_mutex_unlock( &lock_spoolerQueue);

	while(deadCount < threadCount || numJobsSentToPrint < numJobsSpooled){
		message.str("");
		stuffToPrint = false;
		pthread_mutex_lock(&lock_spoolerQueue);
			numJobsLeft = spoolerQueue.size();

			if(numJobsLeft > 0){
				job = spoolerQueue.front();
				spoolerQueue.pop();
				stuffToPrint = true;
			}
		pthread_mutex_unlock(&lock_spoolerQueue);

		if(stuffToPrint){
			pthread_mutex_lock( &lock_printerQueue);
				printerQueue.push(job);
				numJobsSentToPrint++;
			pthread_mutex_unlock( &lock_printerQueue);
#ifdef DEBUG
			message << "SPOOLER job:: \n" << job;
			cout << message.str();
#endif
			sem_post(&printsAvailable);
		}

		pthread_mutex_lock( &lock_numTerm_count);
			deadCount = numTerminated;
		pthread_mutex_unlock( &lock_numTerm_count);
	}
	// post to printsAvailable in case no printing jobs occurred to release printer
	sem_post( &printsAvailable);
#ifdef DEBUG
	printf("Spooler terminating...\n");
#endif
	return NULL;
}

void *Printer( void *){
	int threadCount = NUM_PROCESSOR_THREADS;
	string output;
	int deadCount = 0;
	int numPrintsLeft = 0;
	// repeat while all processor threads have not terminated

	pthread_mutex_lock(&lock_printerQueue);
		numPrintsLeft = printerQueue.size();
	pthread_mutex_unlock( &lock_printerQueue);


	while(deadCount < threadCount || numJobsPrinted < numJobsSpooled){
		// keep popping jobs off the front of the queue until there are none
		pthread_mutex_lock( &lock_printerQueue );
			numPrintsLeft = printerQueue.size();
			if(numPrintsLeft > 0){
				output = printerQueue.front();
				printerQueue.pop();
				cout << output;
				cout.flush();
				numJobsPrinted++;
			}
		pthread_mutex_unlock( &lock_printerQueue );

		pthread_mutex_lock( &lock_numTerm_count);
			deadCount = numTerminated;
		pthread_mutex_unlock( &lock_numTerm_count);
	}
#ifdef DEBUG
	printf("Printer terminating...\n");
#endif
	return NULL;
}
