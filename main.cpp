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

#include <queue>
using std::queue;

#include <cstdlib> // atoi(), exit()
using std::atoi;

#include <sys/time.h>
using namespace std;

// local function declarations
void * Processor(void *);
void * Printer(void *);
void * Spooler(void *);
int factorial(int number);
const int g_MAX_PROCESSOR_THREADS = 10;

typedef struct _thread_parameters{
	int tid;
	queue<string> * program;
} thread_parameters;

//pthread_mutex_t g_lock_numTerminated = PTHREAD_MUTEX_INITIALIZER; // lock mutex for preventing access to buffer
//int g_numTerminated = 0; // needs a mutex

pthread_mutex_t g_lock_spoolerQueue = PTHREAD_MUTEX_INITIALIZER;
queue< string > g_spoolerQueue;

//pthread_mutex_t g_lock_numJobsSpooled = PTHREAD_MUTEX_INITIALIZER;
//int g_numJobsSpooled = 0;

pthread_mutex_t g_lock_printerQueue = PTHREAD_MUTEX_INITIALIZER;
queue< string > g_printerQueue;
int g_numJobsSentToPrint = 0;

//pthread_mutex_t g_lock_numJobsSentToPrint = PTHREAD_MUTEX_INITIALIZER;

//int g_numJobsPrinted = 0;

sem_t g_jobsAvailable;
sem_t g_printsAvailable;
sem_t g_spoolerEmpty; // init to 10
sem_t g_spoolerFull;  // init to  0
sem_t g_printerEmpty; // init to 10
sem_t g_printerFull;  // init to  0

pthread_mutex_t g_lock_numJobs = PTHREAD_MUTEX_INITIALIZER;
typedef struct _numJobs{
	int started;
	int terminated;
	int spooled;
	int sentToPrint;
	int printed;
}numJobs;
numJobs g_numJobs;

// 10 by default unless a number is passed in as a cmd line arg
pthread_mutex_t g_lock_NUM_PROCESSOR_THREADS;
int g_NUM_PROCESSOR_THREADS = 10;

pthread_mutex_t g_lock_file_access = PTHREAD_MUTEX_INITIALIZER;

queue<string>* parseProgram(int i){
	queue<string> * program = new queue<string>;
	// build filename: multiple files of form "progi.txt"

	ifstream infile;
	stringstream inFileNameStream, message;
	string inFileName;

	inFileNameStream << "prog" << i << ".txt";
	inFileName = inFileNameStream.str();

	// Open pseudo-program file for reading
	const char* cString = inFileName.c_str();
#ifdef DEBUG
	printf("parseProgram(%d)::About to open file \"%s\"\n",i, cString);
#endif
	pthread_mutex_lock(&g_lock_file_access);
		infile.open(cString);
	pthread_mutex_unlock(&g_lock_file_access);

	if(infile.fail()){
		cout << "\nNo program matching \"" << inFileName <<
				"\" exists.  Thread terminating...\n";
		cout.flush();
		//		pthread_cancel(pthread_self());
		program = NULL;
	}else{
		// read the pseudocode programs from the files available
#ifdef DEBUG
		message << "\nProcessor Thread: Program " << i << ": \""
				<< inFileName << "\":::\n";
		cout << message.str();
#endif
		string expression;
		while(infile.good()){
			// get next line from the program
			getline(infile, expression);
			program->push(expression);
		}
		infile.close();
	}
	return program;
}

////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[]){
	// get the number of processors to spawn from the command line args
	// parse input arguments
	if (argc > 1){ // argc should be 2 for correct execution
		pthread_mutex_lock(   &g_lock_NUM_PROCESSOR_THREADS );
			g_NUM_PROCESSOR_THREADS = atoi(argv[1]);
			if(g_NUM_PROCESSOR_THREADS > g_MAX_PROCESSOR_THREADS){
				g_NUM_PROCESSOR_THREADS = 10;
				cout << "Warning: Maximum number of processor threads is 10.\n";
			}else if(g_NUM_PROCESSOR_THREADS == 0){
				cout << "Warning: No processor threads requested.  Terminating..." << endl;
				cout.flush();
				return -1;
			}
		pthread_mutex_unlock( &g_lock_NUM_PROCESSOR_THREADS );
	}

	pthread_mutex_lock(&g_lock_numJobs);
		g_numJobs.started = 0;
		g_numJobs.terminated = 0;
		g_numJobs.spooled = 0;
		g_numJobs.sentToPrint = 0;
		g_numJobs.printed = 0;
	pthread_mutex_unlock(&g_lock_numJobs);


#ifdef DEBUG
	cout << "Number of processor threads requested = " << g_NUM_PROCESSOR_THREADS << endl;
#endif
	int numThreads;
	pthread_mutex_lock(   &g_lock_NUM_PROCESSOR_THREADS );
		numThreads = g_NUM_PROCESSOR_THREADS;
	pthread_mutex_unlock( &g_lock_NUM_PROCESSOR_THREADS );

	// create thread handles
	pthread_t *tid = new pthread_t(numThreads);
	pthread_t spooler_tid;
	pthread_t printer_tid;

	// initialize semaphores
	sem_init( &g_jobsAvailable, 0, 0);
	sem_init( &g_printsAvailable, 0, 0);

	// initialize producer-consumer semaphores to 10 slots each
	sem_init( &g_spoolerEmpty, 0, 10);
	sem_init( &g_spoolerFull, 0, 0);
	sem_init( &g_printerEmpty, 0, 10);
	sem_init( &g_printerFull, 0, 0);

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
		thread_parameters * data = new thread_parameters;
		data->tid = i;
		data->program = parseProgram(i);
		// here's where the magic (synchronization) happens
		int code = pthread_create(&tid[i], NULL, Processor, (void*)data);
		if( code != 0 ){
			printf( "Error: unable to create processor thread\n" );
		}
	}

	// processor go off and work through their jobs then join when they're done
	for(int i = 1; i <= numThreads; ++i)
		pthread_join( tid[ i ], NULL );
#ifdef DEBUG
	printf("MAIN::Processor threads joined\n");
#endif

	// join spooler
	pthread_join(spooler_tid, NULL);
#ifdef DEBUG
	printf("MAIN::Spooler thread joined\n");
#endif
	// join printer
	pthread_join(printer_tid, NULL);
#ifdef DEBUG
	printf("MAIN::Printer thread joined\n");
#endif

#ifdef DEBUG
	printf ( "\nmain() terminating\n" );
	cout << "num Jobs spooled: " << g_numJobs.spooled << endl;
	cout << "num Jobs senttoPrint: " << g_numJobs.sentToPrint<< endl;
	cout << "num Jobs printed: " << g_numJobs.printed << endl;
#endif
	cout.flush();
	cout << endl;
	// program's done

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
void *Processor( void * arg){
	int intTID;
//	thread_parameters* params = (thread_parameters*)arg;
//	intTID = params->tid;

	thread_parameters* params = (thread_parameters*)arg;
	queue<string> * program = params->program;
	intTID = params->tid;

	pthread_mutex_lock(&g_lock_numJobs);
		g_numJobs.started++;
	pthread_mutex_unlock(&g_lock_numJobs);

#ifdef DEBUG
	printf("PROCESSOR: ID (%d) beginning...\n", intTID);
#endif

	ifstream infile;
	stringstream inFileNameStream, message;
	string inFileName;
	stringstream localBuffer;
	bool stuffToPrint = false;

	// computes may be increased to spread things apart (normal = 1)
	const int slowdownFactor = 1;

#ifdef DEBUG
	// variables to hold program etime
	struct timeval start, end;
	long seconds, useconds;
#endif

	// build filename: multiple files of form "progi.txt"
	inFileNameStream << "prog" << intTID << ".txt";
	inFileName = inFileNameStream.str();

	// Open pseudo-program file for reading
	const char* cString = inFileName.c_str();
#ifdef DEBUG
	printf("PROCESSOR(%d)::About to open file \"%s\"\n",intTID, cString);
#endif
	pthread_mutex_lock(&g_lock_file_access);
		infile.open(cString);
	pthread_mutex_unlock(&g_lock_file_access);

/*
	if(infile.fail()){
		cout << "\nNo program matching \"" << inFileName <<
				"\" exists.  Thread terminating...\n";
		cout.flush();
//		pthread_cancel(pthread_self());
	}else{
		// read the pseudocode programs from the files available
#ifdef DEBUG
		message << "\nProcessor Thread: Program " << intTID << ": \""
				<< inFileName << "\":::\n";
		cout << message.str();
#endif
		string expression;
		queue<string> program;
		while(infile.good()){
			// get next line from the program
			getline(infile, expression);
			program.push(expression);
		}
		infile.close();
*/
	if(program == NULL){
		cout << "\nUnable to parse program.  Thread terminating...\n";
				cout.flush();
	}else{
		string expression;
		// parse expressions for instructions
		while(!program->empty()){
			expression = program->front();
			program->pop();
			// clear exprTokens, token
			string fcnName, fcnArg;
			stringstream exprTokens;

			if(expression != ""){
				// put expression into a stringstream for token extraction
				exprTokens << expression;

				// extract tokens (assume always only two per line)
				exprTokens >> fcnName;
				exprTokens >> fcnArg;
#ifdef DEBUG
	#ifdef VERBOSE
					message << "+" << fcnName << "(" << fcnArg << ")\n";
	#endif
#endif

				if(fcnName == "NewJob"){
					// reset stuffToPrint to false since it's a new job and we don't know what's coming
					stuffToPrint = false;
#ifdef DEBUG
					// start timer
					gettimeofday(&start, NULL);
#endif
					// clear the buffer
					localBuffer.str("");

					// add string to the message describing the job output
					localBuffer << "\nProcess " << intTID << ", Job " << fcnArg << ":" << endl;
				}else if(fcnName == "Compute"){
					// compute factorial of fcnArg
					int N = atoi(fcnArg.c_str()) * slowdownFactor;
					int product = factorial(N);
					product = product * 1;
#ifdef DEBUG
	#ifdef VERBOSE
					message << "\t" << N << "! = " << product << endl;
	#endif
#endif
					}else if(fcnName == "Print"){
					// buffer print args
					localBuffer << fcnArg << endl;
					stuffToPrint = true;
				}else if(fcnName == "EndJob"){
					// send buffer to spooler/ signal spooler
					// only if last print job from this processor is done
					if(stuffToPrint){
						// if debug mode, append elapsed time to the output
#ifdef DEBUG
						// print program elapsed time
						gettimeofday(&end, NULL);
						seconds  = end.tv_sec  - start.tv_sec;
						useconds = end.tv_usec - start.tv_usec;
						double preciseTime = seconds + useconds/1000000.0;

						localBuffer << "[etime: " << preciseTime << " s]\n" ;
#endif
						/* this is the CS where Processor threads will send their
						 * output to the Spooler thread */
#ifdef DEBUG
						printf("PROCESSOR(%d)::waiting on g_spoolerEmpty\n", intTID);
#endif
						// wait on available space in the spoolerbuffer
						sem_wait( &g_spoolerEmpty );
							// wait on spoolerQueue mutex
							pthread_mutex_lock(   &g_lock_spoolerQueue);
								// send localBuffer to global buffer
								g_spoolerQueue.push(localBuffer.str());
	//							g_numJobsSpooled++;
							pthread_mutex_lock(   &g_lock_numJobs);
								g_numJobs.spooled++;
							pthread_mutex_unlock( &g_lock_numJobs);
#ifdef DEBUG
							printf("PROCESSOR::spoolerQueue size = %lu\n", g_spoolerQueue.size());

	#ifdef VERBOSE
							printf("---Job spooled---%s---endjobspool---\n",localBuffer.str().c_str());
	#endif
#endif
							// sem post on return
							pthread_mutex_unlock( &g_lock_spoolerQueue);

						// signal an item has been produced
						sem_post( &g_spoolerFull );

						// signal spooler that ANY processor has a job available so it may begin
						sem_post(&g_jobsAvailable);
					}

					// reset stuffToPrint
					stuffToPrint = false;
				}else if(fcnName =="Terminate"){
					// exit since should be the last line in the program
					infile.close();

//					pthread_mutex_lock(   &g_lock_numTerminated);
//						// increment number of terminated threads
//						g_numTerminated++;
//					pthread_mutex_unlock( &g_lock_numTerminated);

					pthread_mutex_lock( &g_lock_numJobs);
						g_numJobs.terminated++;
					pthread_mutex_unlock( &g_lock_numJobs);

					// post jobs available in case no jobs with printing occurred
					sem_post(&g_jobsAvailable);
#ifdef DEBUG
					printf("PROCESOR:: (%d) terminating...\n",intTID);
#endif
				}
			}
		}
	}
//	delete params;
	sem_post(&g_spoolerFull);
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
void *Spooler( void *){

	int numThreads;
	pthread_mutex_lock(   &g_lock_NUM_PROCESSOR_THREADS );
		numThreads = g_NUM_PROCESSOR_THREADS;
	pthread_mutex_unlock( &g_lock_NUM_PROCESSOR_THREADS );

	string job;
	bool stuffToPrint = false;
//	int numJobsAvailable = 0, deadCount = 0, localNumJobsSentToPrint, localNumJobsSpooled = 0;

	// wait on jobsAvailable to prevent spooler from running w/ no jobs available
	sem_wait(&g_jobsAvailable);

	// local copy of jobs counter
	numJobs spoolerJobs;
	pthread_mutex_lock(&g_lock_numJobs);
		spoolerJobs = g_numJobs;
	pthread_mutex_unlock(&g_lock_numJobs);

	while(true){
		stuffToPrint = false;

#ifdef DEBUG
		printf("SPOOLER::waiting on g_spoolerFull\n");
#endif
		sem_wait( &g_spoolerFull);
			pthread_mutex_lock(   &g_lock_spoolerQueue);
#ifdef DEBUG
			printf("SPOOLER::spoolerQueue size = %lu\n", g_spoolerQueue.size());
#endif
			if(g_spoolerQueue.size() > 0){
				job = g_spoolerQueue.front();
				g_spoolerQueue.pop();
				stuffToPrint = true;
				sem_post( &g_spoolerEmpty);
			}else{
#ifdef DEBUG
				printf("SPOOLER::spooler should NOT be here!\n");
#endif
				stuffToPrint = false;
			}
			pthread_mutex_unlock( &g_lock_spoolerQueue);

		// move string to printer queue
		if(stuffToPrint){
#ifdef DEBUG
			printf("SPOOLER::waiting on g_printerEmpty\n");
#endif
			sem_wait( &g_printerEmpty);

			pthread_mutex_lock(   &g_lock_printerQueue);
				g_printerQueue.push(job);
				pthread_mutex_lock( &g_lock_numJobs);
					g_numJobs.sentToPrint++;
				pthread_mutex_unlock( &g_lock_numJobs);
			pthread_mutex_unlock( &g_lock_printerQueue);

			// notify printer it can go ahead
			sem_post( &g_printerFull);
			sem_post( &g_printsAvailable);
		}

		// update job statuses to the global value
		pthread_mutex_lock( &g_lock_numJobs);
			spoolerJobs = g_numJobs;
		pthread_mutex_unlock( &g_lock_numJobs);

#ifdef DEBUG
		printf("SPOOLER::Dead[%d], Spooled[%d], Sent[%d]\n",
				spoolerJobs.terminated,spoolerJobs.spooled,spoolerJobs.sentToPrint);
#endif
		if(spoolerJobs.terminated == numThreads && spoolerJobs.spooled == spoolerJobs.sentToPrint){
#ifdef DEBUG
			printf("SPOOLER::Finally the SPOOLER may rest\n");
#endif
			break;
		}
	}

	// notify printer in case no printing jobs occurred to release printer
	sem_post( &g_printsAvailable);
	// signal to make sure no processes are waiting on buffer
	sem_post( &g_spoolerEmpty);
	sem_post(&g_printerFull);

#ifdef DEBUG
	printf("SPOOLER::terminating...\n");
#endif
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
void *Printer( void *){

	int numThreads;
	pthread_mutex_lock(   &g_lock_NUM_PROCESSOR_THREADS );
		numThreads = g_NUM_PROCESSOR_THREADS;
	pthread_mutex_unlock( &g_lock_NUM_PROCESSOR_THREADS );

	string output;

	// create local copies of the counter variables
	numJobs printerJobs;

#ifdef DEBUG
	printf("PRINTER::waiting on jobsAvailable\n");
#endif
    // wait on jobsAvailable to prevent spooler from running w/ no jobs available
	sem_wait(&g_jobsAvailable);

#ifdef DEBUG
	printf("PRINTER::waiting on g_lock_spoolerQueue\n");
#endif

	// repeat while all processor threads have not terminated & jobs left to print
	while(true){
		// keep popping jobs off the front of the queue until there are none
#ifdef DEBUG
		printf("PRINTER::waiting on g_printerFull\n");
#endif
		sem_wait( &g_printerFull);
		pthread_mutex_lock(   &g_lock_printerQueue );
#ifdef DEBUG
			printf("PRINTER::printerQueue size = %lu\n", g_printerQueue.size());
#endif
			if(g_printerQueue.size() > 0){
				output = g_printerQueue.front();
				g_printerQueue.pop();
				pthread_mutex_lock(&g_lock_numJobs);
					g_numJobs.printed++;
				pthread_mutex_unlock(&g_lock_numJobs);
				sem_post( &g_printerEmpty);
			}else{
#ifdef DEBUG
				output = "PRINTER::printer should NOT be here!\n";
#endif
			}
		pthread_mutex_unlock( &g_lock_printerQueue );

		cout << output;
		cout.flush();

#ifdef DEBUG
		printf("PRINTER::waiting on g_lock_numJobs\n");
#endif
		pthread_mutex_lock( &g_lock_numJobs);
			printerJobs = g_numJobs;
		pthread_mutex_unlock( &g_lock_numJobs);

#ifdef DEBUG
		printf("PRINTER::Dead[%d], Spooled[%d], Sent[%d], Prints[%d]\n",
				printerJobs.terminated,printerJobs.spooled,printerJobs.sentToPrint,printerJobs.printed);
#endif
		if(printerJobs.terminated == numThreads && printerJobs.spooled == printerJobs.printed){
#ifdef DEBUG
			printf("PRINTER::Finally the PRINTER may rest\n");
#endif
			break;
		}
	}
#ifdef DEBUG
	printf("PRINTER::terminating...\n");
#endif
	return NULL;
}

int factorial(int N){
	int product = 1;
	for(int i = N; i > 1; --i)
		product = product * i;
	return product;
}
