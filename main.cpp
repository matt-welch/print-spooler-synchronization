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
const int g_MAX_PROCESSOR_THREADS = 10;

pthread_mutex_t g_lock_numTerm_count; // lock mutex for preventing access to buffer
pthread_mutex_t g_lock_spoolerQueue;
queue< string > g_spoolerQueue;
sem_t g_spoolerQueueBusy;

pthread_mutex_t g_lock_numJobsSpooled;
int g_numJobsSpooled = 0;

pthread_mutex_t g_lock_printerQueue;
queue< string > g_printerQueue;
sem_t g_printerQueueBusy;

pthread_mutex_t g_lock_numJobsSentToPrint;
int g_numJobsSentToPrint = 0;

int g_numJobsPrinted = 0;

sem_t g_buffersFull[g_MAX_PROCESSOR_THREADS];  // semaphore to tell spooler that jobs are available

sem_t g_jobsAvailable;
sem_t g_printsAvailable;

int g_numTerminated = 0; // needs a mutex

// 10 by default unless a number is passed in as a cmd line arg
int g_NUM_PROCESSOR_THREADS = 10;
pthread_mutex_t g_lock_threadcount;

int main(int argc, char* argv[]){
	// get the number of processors to spawn from the command line args
	// parse input arguments
	if (argc > 1){ // argc should be 2 for correct execution
		pthread_mutex_lock(   &g_lock_threadcount );
			g_NUM_PROCESSOR_THREADS = atoi(argv[1]);
		pthread_mutex_unlock( &g_lock_threadcount );
	}


#ifdef DEBUG
	cout << "Number of processor threads requested = " << g_NUM_PROCESSOR_THREADS << endl;
#endif

	// create thread handles
	pthread_mutex_lock(   &g_lock_threadcount );
		pthread_t *tid = new pthread_t(g_NUM_PROCESSOR_THREADS);
	pthread_mutex_unlock( &g_lock_threadcount );

	pthread_t spooler_tid;
	pthread_t printer_tid;

	// initialize mutex & semaphores
	sem_init( &g_jobsAvailable, 0, 0);
	sem_init( &g_printsAvailable, 0, 0);

	sem_init( &g_spoolerQueueBusy, 0, 1);
	sem_init( &g_printerQueueBusy, 0, 0);

	pthread_mutex_lock(   &g_lock_threadcount );
		int numThreads = g_NUM_PROCESSOR_THREADS;
	pthread_mutex_unlock( &g_lock_threadcount );

	for(int i = 0; i < numThreads; ++i){
		sem_init(&g_buffersFull[i], 0, 0);
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
	for(int i = 1; i <= numThreads; ++i){
		// here's where the magic (synchronization) happens
		if( pthread_create(&tid[i], NULL, Processor, (void*) i) != 0 ){
			printf( "Error: unable to create processor thread\n" );
		}
	}

	// processor go off and work through their jobs then join when they're done
	for(int i = 1; i <= numThreads; ++i)
		pthread_join( tid[ i ], NULL );
	// join spooler
	pthread_join(spooler_tid, NULL);
	// join printer
	pthread_join(printer_tid, NULL);

#if 1
	printf ( "\nmain() terminating\n" );

	cout << "num Jobs spooled: " << g_numJobsSpooled << endl;
	cout << "num Jobs senttoPrint: " << g_numJobsSentToPrint << endl;
	cout << "num Jobs printed: " << g_numJobsPrinted << endl;
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

	// computes may be slowed down to spread things apart (normal = 1)
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
						pthread_mutex_lock(   &g_lock_spoolerQueue);
							// send localBuffer to global buffer
							g_spoolerQueue.push(localBuffer.str());
#ifdef DEBUG
							printf("---Job spooled---%s---endjobspool---\n",localBuffer.str().c_str());
#endif
							// sem post on return
						pthread_mutex_unlock( &g_lock_spoolerQueue);

						pthread_mutex_lock( &g_lock_numJobsSpooled);
							g_numJobsSpooled++;
						pthread_mutex_unlock(&g_lock_numJobsSpooled);

						// signal spooler that ANY processor has a job available so it may begin
						sem_post(&g_jobsAvailable);
					}

					// reset stuffToPrint
					stuffToPrint = false;
				}else if(fcnName =="Terminate"){
					// exit since should be the last line in the program
					infile.close();

					pthread_mutex_lock(   &g_lock_numTerm_count);
						// increment number of terminated threads
						g_numTerminated++;
					pthread_mutex_unlock( &g_lock_numTerm_count);

					// post jobs available in case no jobs with printing occurred
					sem_post(&g_jobsAvailable);
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
	// wait for a job to be available ( g_jobsAvailable )
	pthread_mutex_lock(   &g_lock_threadcount );
		int threadCount = g_NUM_PROCESSOR_THREADS;  // TODO mutex??
	pthread_mutex_unlock( &g_lock_threadcount );

	int deadCount = 0;
	int numJobsLeft = 0;
	string job;
	bool stuffToPrint = false;
	int localNumJobsSentToPrint, localNumJobsSpooled;

	// wait on jobsAvailable to prevent spooler from running w/ no jobs available
	sem_wait(&g_jobsAvailable);

	// update jobs left
	pthread_mutex_lock(   &g_lock_spoolerQueue);
		numJobsLeft = g_spoolerQueue.size();
	pthread_mutex_unlock( &g_lock_spoolerQueue);

	pthread_mutex_lock( &g_lock_numJobsSpooled);
		localNumJobsSpooled = g_numJobsSpooled;
	pthread_mutex_unlock(&g_lock_numJobsSpooled);

	pthread_mutex_lock(   &g_lock_numJobsSentToPrint);
		localNumJobsSentToPrint = g_numJobsSentToPrint;
	pthread_mutex_unlock( &g_lock_numJobsSentToPrint);

	while(deadCount < threadCount || localNumJobsSentToPrint < localNumJobsSpooled){
		stuffToPrint = false;

#ifdef DEBUG
		// slow down the spooler output when processes are done
		if(deadCount == threadCount)
			sleep(1);
#endif

		pthread_mutex_lock(   &g_lock_spoolerQueue);
			numJobsLeft = g_spoolerQueue.size();
			if(numJobsLeft > 0){
				job = g_spoolerQueue.front();
				g_spoolerQueue.pop();
				stuffToPrint = true;
			}
		pthread_mutex_unlock( &g_lock_spoolerQueue);

		// move string to printer queue
		if(stuffToPrint){
			pthread_mutex_lock(   &g_lock_printerQueue);
				g_printerQueue.push(job);
			pthread_mutex_unlock( &g_lock_printerQueue);

			// update the number of jobs sent to print
			pthread_mutex_lock(   &g_lock_numJobsSentToPrint);
				g_numJobsSentToPrint++;
				localNumJobsSentToPrint = g_numJobsSentToPrint;
			pthread_mutex_unlock( &g_lock_numJobsSentToPrint);

			// notify printer it can go ahead
			sem_post(&g_printsAvailable);
		}


		// update the number of terminated processes
		pthread_mutex_lock(   &g_lock_numTerm_count);
			deadCount = g_numTerminated;
		pthread_mutex_unlock( &g_lock_numTerm_count);

		// update number of jobs spooled to the global value
		pthread_mutex_lock( &g_lock_numJobsSpooled);
			localNumJobsSpooled = g_numJobsSpooled;
		pthread_mutex_unlock(&g_lock_numJobsSpooled);
	}

	// notify printer in case no printing jobs occurred to release printer
	sem_post( &g_printsAvailable);

#ifdef DEBUG
	printf("Spooler terminating...\n");
#endif
	return NULL;
}

void *Printer( void *){

	pthread_mutex_lock(   &g_lock_threadcount );
		int threadCount = g_NUM_PROCESSOR_THREADS;
	pthread_mutex_unlock( &g_lock_threadcount );

	string output;
	int deadCount = 0;
	int numPrintsLeft = 0;

	// create local copies of the counter variables
	int localNumJobsSentToPrint, localNumJobsSpooled, local_numJobsPrinted;

	sem_wait(&g_printsAvailable);
#ifdef DEBUG
	printf(" PRINTER waiting on g_lock_printerQueue\n");
#endif

	pthread_mutex_lock(   &g_lock_printerQueue);
		numPrintsLeft = g_printerQueue.size();
	pthread_mutex_unlock( &g_lock_printerQueue);

	pthread_mutex_lock(   &g_lock_numJobsSentToPrint);
		localNumJobsSentToPrint = g_numJobsSentToPrint;
	pthread_mutex_unlock( &g_lock_numJobsSentToPrint);

	pthread_mutex_lock(    &g_lock_spoolerQueue);
		localNumJobsSpooled = g_numJobsSpooled;
	pthread_mutex_unlock( &g_lock_spoolerQueue);
#ifdef DEBUG
	printf(" PRINTER waiting on g_lock_spoolerQueue\n");
#endif
/*	pthread_mutex_lock(   &g_lock_spoolerQueue );
		localNumJobsSpooled = g_numJobsSpooled;
	pthread_mutex_unlock( &g_lock_spoolerQueue );*/

	// repeat while all processor threads have not terminated & jobs left to print
	while(deadCount < threadCount || local_numJobsPrinted < localNumJobsSpooled){
		// keep popping jobs off the front of the queue until there are none
		//printf(" PRINTER waiting on g_lock_printerQueue\n");

#ifdef DEBUG
		// slow down the printer output when processes are done
		if(deadCount == threadCount)
			sleep(1);
#endif

		pthread_mutex_lock(   &g_lock_printerQueue );
			numPrintsLeft = g_printerQueue.size();
			if(numPrintsLeft > 0){
				output = g_printerQueue.front();
				g_printerQueue.pop();
				cout << output;
				cout.flush();
				g_numJobsPrinted++;
				local_numJobsPrinted = g_numJobsPrinted;
			}
		pthread_mutex_unlock( &g_lock_printerQueue );

		pthread_mutex_lock(   &g_lock_numJobsSentToPrint);
			localNumJobsSentToPrint = g_numJobsSentToPrint;
		pthread_mutex_unlock( &g_lock_numJobsSentToPrint);

		//printf(" PRINTER waiting on g_lock_numTerm_count\n");
		pthread_mutex_lock(   &g_lock_numTerm_count);
			deadCount = g_numTerminated;
		pthread_mutex_unlock( &g_lock_numTerm_count);

		pthread_mutex_lock(     &g_lock_spoolerQueue);
			localNumJobsSpooled = g_numJobsSpooled;
		pthread_mutex_unlock( &g_lock_spoolerQueue);

//		printf("Spool[%d], SENT[%d], Prints[%d]\n",localNumJobsSpooled,localNumJobsSentToPrint,g_numJobsPrinted);

	}
#ifdef DEBUG
	printf("Printer terminating...\n");
#endif
	return NULL;
}
