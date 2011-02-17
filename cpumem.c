/*!
 *
 *  \file    cpumem.c
 *  \brief   Benchmarks the CPU and virtual memory
 *
 *  \author  BJ Peter DeLaCruz
 *
 *  \date    June 25, 2010
 *
 *  \version 1.0
 *
 *  \details \par How this program works:
 *           This program benchmarks the performance of the CPU and virtual memory. First, in the
 *           CPU test, each process creates N pthreads. Each pthread takes the square root of a
 *           random number between 0 and \b RAND_MAX, and repeats this calculation M times. The
 *           program times how long it takes each pthread to perform all M calculations and then
 *           displays the results. Next, in the virtual memory test, each process allocates an
 *           array whose size varies during each of the P runs and are between the minimum and
 *           maximum sizes, which are specified by the user; and fills all of the elements in the
 *           array with the same value. Each process then sleeps for Q seconds, and after it wakes
 *           up, it checks the array to make sure it contains the same values before the process
 *           went to sleep. The program times how long it takes each process to perform the memory
 *           test P times and then displays the results.
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>
/*! Pthreads are used in CPU test */
#include <pthread.h>

/*! Master process. Usually process 0. */
#define MASTER      0
#define TRUE        1
#define FALSE       0
/*! Used in memory test. PASS if memory allocation was successful and no errors were encountered */
#define PASS        0
/*! Used in memory test. FAIL if memory allocation was not successful or errors were encountered */
#define FAIL       -1

/*!
 *  \brief Arguments for the CPU test
 */
typedef struct cpu_test_a {
    int process_id;
    long runs;
} cpu_test_a;

/*!
 *  \brief Output from the CPU test
 */
typedef struct cpu_test_o {
    int process_id;
    double runtime;
    long pthread_id;
} cpu_test_o;

/*!
 *  \brief Arguments for the memory test
 */
typedef struct mem_test_a {
    int process_id;
    long array_size;
    struct timespec* sleep_time;
} mem_test_a;

/*!
 *  \brief Output from the memory test
 */
typedef struct mem_test_o {
    int process_id;
    char result;
} mem_test_o;

/***************************************************************************************************/

/*!
 *
 *  \par Description:
 *  Tests a node's CPU power by taking the square root of a number between 0 and RAND_MAX a certain
 *  number of times.
 *
 *  \param cpu_test_args Struct that contains the ID of a process
 *
 */
void* cpu_test(void* cpu_test_args);

/*!
 *
 *  \par Description:
 *  Tests a node's virtual memory by allocating an array of characters on the heap, filling the
 *  array with the letter 'B', and checking to see whether the virtual memory is corrupted.
 *
 *  \param mem_test_args Struct that contains the ID of a process, array size, and a pointer to
 *                       a timespec struct, which is used for making a process go to sleep for N seconds
 *
 *  \return A \c mem_test_o struct that contains the results of the test and the ID of the process
 *
 */
mem_test_o* mem_test(mem_test_a* mem_test_args);

/*!
 *
 *  \par Description:
 *  Assigns the same value to all elements in array.
 *
 *  \param array Empty array
 *  \param length Number of elements in array
 *
 */
void initialize(char* array, long length);

/*!
 *  \param argv[1] Number of pthreads to use for CPU test
 *  \param argv[2] Number of times to repeat CPU test
 *  \param argv[3] Minimum size of array for memory test
 *  \param argv[4] Maximum size of array for memory test
 *  \param argv[5] Number of seconds to sleep during memory test
 *  \param argv[6] Number of times to repeat memory test
 */
int main(int argc, char** argv) {

    /* Arguments for CPU test for all processes */
    cpu_test_a* cpu_test_args = NULL;

    /* Runtime of a process */
    double runtime;

    /* Contains runtimes of pthreads for a process */
    double* pthread_runtimes = NULL;
    /* Contains runtimes of pthreads for all processes */
    double* all_pthread_runtimes = NULL;
    /* Contains runtimes for memory test for all processes */
    double* mem_test_runtimes = NULL;

    int counter; /* loop counter */
    /* Used for error handling */
    int error_code;
    /* Message identifier for sending/receiving pthread IDs to/from processes */
    int ID_TAG = 0;
    /* Number of pthreads to use per process */
    int NUMBER_OF_PTHREADS;
    /* Total number of processes used in this program */
    int NUMBER_OF_PROCESSES;
    /* Number of times to repeat memory test */
    int NUMBER_OF_RUNS;
    /* Current element in array */
    int position;
    /* Current process */
    int PROCESS_ID;
    /* Message identifier for sending/receiving runtime to/from a processes */
    int RUNTIME_TAG = 1;
    /* Process that sends data to other processes */
    int source;

    /* Contains IDs of processes to whom pthreads belong for CPU test */
    int* all_process_ids = NULL;

    /* Upper bound for number of characters to allocate during memory test */
    long MAX_SIZE;
    /* Lower bound for number of characters to allocate during memory test */
    long MIN_SIZE;

    /* Contains IDs of pthreads for a process */
    long* pthread_ids = NULL;
    /* Contains IDs of pthreads for all processes */
    long* all_pthread_ids = NULL;

    /* Arguments for memory test for all processes */
    mem_test_a* mem_test_args = (mem_test_a*) calloc(1, sizeof(mem_test_a));

    if (mem_test_args == NULL) {
       printf("Memory allocation failed for mem_test_args struct! Aborting...\n");
       exit(1);
    }

    /* Amount of time process sleeps while holding allocated memory */
    mem_test_args->sleep_time = (struct timespec*) calloc(1, sizeof(struct timespec));

    if (mem_test_args->sleep_time == NULL) {
       printf("Memory allocation failed for mem_test_args->sleep_time struct! Aborting...\n");
       exit(1);
    }

    mem_test_args->sleep_time->tv_sec = 0;
    mem_test_args->sleep_time->tv_nsec = 0;

    /* Output from memory test for all processes */
    mem_test_o* mem_test_output = NULL;

    /* Used to start timing runtime of a process */
    time_t start;
    /* Used to end timing runtime of a process */
    time_t end;
    /* Used to start timing program execution */
    time_t program_start;
    /* Used to end timing program execution */
    time_t program_end;

    /* Output from CPU test for all processes */
    void* cpu_test_results = NULL;

    /* Used in MPI_Recv */
    MPI_Status status;

    /***************************************************************************************************/

    if (argc != 7) {
       printf("Usage: ./cpumem ");
       printf("[number of threads to use for CPU test] [number of times to repeat CPU test] ");
       printf("[minimum size of array for memory test] [maximum size of array for memory test] ");
       printf("[seconds to sleep during memory test] [number of times to repeat memory test]\n");
       printf("Please try again.\n");
       exit(1);
    }

    if ((NUMBER_OF_PTHREADS = atoi(argv[1])) == 0) {
       printf("Error: Invalid argument for number of threads for CPU test. Please try again.\n");
       exit(1);
    }

    if (atol(argv[2]) == 0) {
       printf("Error: Invalid argument for number of times to repeat CPU test. Please try again.\n");
       exit(1);
    }

    if ((MIN_SIZE = atoi(argv[3])) == 0) {
       printf("Error: Invalid argument for minimum size of array for memory test. Please try again.\n");
       exit(1);
    }

    if ((MAX_SIZE = atoi(argv[4])) == 0) {
       printf("Error: Invalid argument for maximum size of array for memory test. Please try again.\n");
       exit(1);
    }

    if (MAX_SIZE <= MIN_SIZE) {
       printf("Error: Maximum size of array must be greater than minimum size of array. Please try again.\n");
       exit(1);
    }

    if ((mem_test_args->sleep_time->tv_sec = atoi(argv[5])) == 0) {
       printf("Error: Invalid argument for number of seconds to sleep during memory test. Please try again.\n");
       exit(1);
    }

    if ((NUMBER_OF_RUNS = atoi(argv[6])) == 0) {
       printf("Error: Invalid argument for number of times to repeat memory test. Please try again.\n");
       exit(1);
    }

    /***************************************************************************************************/

    error_code = MPI_Init(&argc, &argv);
    error_code = MPI_Comm_size(MPI_COMM_WORLD, &NUMBER_OF_PROCESSES);
    error_code = MPI_Comm_rank(MPI_COMM_WORLD, &PROCESS_ID);

    if (error_code != 0) {
       printf("Error encountered while initializing MPI and obtaining task information.\n");
       MPI_Finalize();
       exit(1);
    }

    srand(time(NULL));

    /****************************************************************************************************
    ** Using N pthreads, one for each CPU test, prepare and start CPU tests                            **
    ****************************************************************************************************/
    program_start = time(NULL);

    mem_test_args->process_id = PROCESS_ID;

    pthread_t my_pthreads[NUMBER_OF_PTHREADS];

    cpu_test_args = (cpu_test_a*) calloc(1, sizeof(cpu_test_a));

    if (cpu_test_args == NULL) {
       printf("Memory allocation failed for cpu_test_args array! ");
       printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
       MPI_Finalize();
       exit(1);
    }

    cpu_test_args->process_id = PROCESS_ID;
    cpu_test_args->runs = atol(argv[2]);

    if (PROCESS_ID == MASTER) {
       printf("\n");
       printf("Creating %d threads for each of the %d processes for CPU test... ", NUMBER_OF_PTHREADS, NUMBER_OF_PROCESSES);
    }

    for (counter = 0; counter < NUMBER_OF_PTHREADS; counter++) {
          error_code = pthread_create(&my_pthreads[counter], NULL, cpu_test, (void*) cpu_test_args);

          if (error_code != 0) {
             printf("Error encountered while creating pthread.\n");
             MPI_Finalize();
             exit(1);
          }
    }

    /****************************************************************************************************
    ** Get results from CPU tests                                                                      **
    ****************************************************************************************************/
    pthread_ids = (long*) calloc(NUMBER_OF_PTHREADS, sizeof(long));

    if (pthread_ids == NULL) {
       printf("Memory allocation failed for pthread_ids array! ");
       printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
       MPI_Finalize();
       exit(1);
    }

    pthread_runtimes = (double*) calloc(NUMBER_OF_PTHREADS, sizeof(double));

    if (pthread_runtimes == NULL) {
       printf("Memory allocation failed for pthread_runtimes array! ");
       printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
       MPI_Finalize();
       exit(1);
    }

    for (counter = 0; counter < NUMBER_OF_PTHREADS; counter++) {
          error_code = pthread_join(my_pthreads[counter], &cpu_test_results);

          if (error_code != 0) {
             printf("Error encountered while creating pthread.\n");
             MPI_Finalize();
             exit(1);
          }

          if (cpu_test_results == NULL) {
             printf("CPU test failed!\n");
             printf("Memory allocation failed for cpu_test_results struct! ");
             printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
             MPI_Finalize();
             exit(1);
          }

          pthread_ids[counter] = ((cpu_test_o*) cpu_test_results)->pthread_id;
          pthread_runtimes[counter] = ((cpu_test_o*) cpu_test_results)->runtime;

          free(cpu_test_results);
    }

    if (PROCESS_ID == MASTER) {
       printf("Success!\n\n");
    }

    /****************************************************************************************************
    ** Send results to Master                                                                          **
    ****************************************************************************************************/

    if (PROCESS_ID == MASTER) {
       all_process_ids = (int*) calloc(NUMBER_OF_PROCESSES * NUMBER_OF_PTHREADS, sizeof(double));

       if (all_process_ids == NULL) {
          printf("Memory allocation failed for all_process_ids array! ");
          printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
          MPI_Finalize();
          exit(1);
       }

       all_pthread_ids = (long*) calloc(NUMBER_OF_PROCESSES * NUMBER_OF_PTHREADS, sizeof(long));

       if (all_pthread_ids == NULL) {
          printf("Memory allocation failed for all_pthread_ids array! ");
          printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
          MPI_Finalize();
          exit(1);
       }

       all_pthread_runtimes = (double*) calloc(NUMBER_OF_PROCESSES * NUMBER_OF_PTHREADS, sizeof(double));

       if (all_pthread_runtimes == NULL) {
          printf("Memory allocation failed for all_pthread_runtimes array! ");
          printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
          MPI_Finalize();
          exit(1);
       }

       for (counter = 0; counter < NUMBER_OF_PTHREADS; counter++) {
           all_process_ids[counter] = PROCESS_ID;
           all_pthread_ids[counter] = pthread_ids[counter];
           all_pthread_runtimes[counter] = pthread_runtimes[counter];
       }

       for (source = 1; source < NUMBER_OF_PROCESSES; source++) {
           MPI_Recv(&pthread_ids[0], NUMBER_OF_PTHREADS, MPI_LONG, source, ID_TAG, MPI_COMM_WORLD, &status);
           MPI_Recv(&pthread_runtimes[0], NUMBER_OF_PTHREADS, MPI_DOUBLE, source, RUNTIME_TAG, MPI_COMM_WORLD, &status);
           for (counter = source * NUMBER_OF_PTHREADS, position = 0; position < NUMBER_OF_PTHREADS; counter++, position++) {
               all_process_ids[counter] = source;
               all_pthread_ids[counter] = pthread_ids[position];
               all_pthread_runtimes[counter] = pthread_runtimes[position];
           }
       }

       #ifdef DEBUG
           for (counter = 0; counter < NUMBER_OF_PROCESSES * NUMBER_OF_PTHREADS; counter++) {
               printf("\nThread %10lu   ::   Process %5d   ::   ", all_pthread_ids[counter], all_process_ids[counter]);
               printf("%.2f seconds\n", all_pthread_runtimes[counter]);
           }
       #endif
    }
    else {
       MPI_Send(&pthread_ids[0], NUMBER_OF_PTHREADS, MPI_LONG, MASTER, ID_TAG, MPI_COMM_WORLD);
       MPI_Send(&pthread_runtimes[0], NUMBER_OF_PTHREADS, MPI_DOUBLE, MASTER, RUNTIME_TAG, MPI_COMM_WORLD);
    }

    /****************************************************************************************************
    ** Perform memory test N times                                                                     **
    ****************************************************************************************************/
    runtime = 0.0;

    if (PROCESS_ID == MASTER) {
       printf("Now executing memory test with all %d processes using various array sizes... ", NUMBER_OF_PROCESSES);
    }

    counter = 1;

    do {

       free(mem_test_output);

       mem_test_args->array_size = rand() % (MAX_SIZE - MIN_SIZE + 1) + MIN_SIZE;

       #ifdef DEBUG
           printf("Process %5d: Executing memory test with array size %lu... ", PROCESS_ID, mem_test_args->array_size);
           printf("Run %d of %d.\n", counter, NUMBER_OF_RUNS);
       #endif

       start = time(NULL);
       mem_test_output = mem_test(mem_test_args);
       end = time(NULL);

       #ifdef DEBUG
           printf("Process %5d: Success!\n\n", PROCESS_ID);
       #endif

       runtime += difftime(end, start);

    } while (counter++ < NUMBER_OF_RUNS &&
             mem_test_output != NULL &&
             mem_test_output->result == PASS);

    if (mem_test_output == NULL || mem_test_output->result == FAIL) {
       if (mem_test_output == NULL) {
          printf("Memory test failed!\n");
          printf("Memory allocation failed for mem_test_output struct! ");
          printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
       }
       else {
          printf("Memory test failed! Memory corrupted on process %d.\nAborting program...\n", PROCESS_ID);
       }
       MPI_Finalize();
       exit(1);
    }

    free(mem_test_output);

    if (PROCESS_ID == MASTER) {
       printf("Success!\n\n");
    }

    /****************************************************************************************************
    ** Send results to Master                                                                          **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       mem_test_runtimes = (double*) calloc(NUMBER_OF_PROCESSES, sizeof(double));

       if (mem_test_runtimes == NULL) {
          printf("Memory allocation failed for mem_test_runtimes array! ");
          printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
          MPI_Finalize();
          exit(1);
       }

       mem_test_runtimes[MASTER] = runtime;

       for (source = 1; source < NUMBER_OF_PROCESSES; source++) {
           MPI_Recv(&mem_test_runtimes[source], 1, MPI_DOUBLE, source, RUNTIME_TAG, MPI_COMM_WORLD, &status);
       }

       #ifdef DEBUG
           for (counter = 1; counter < NUMBER_OF_PROCESSES; counter++) {
               printf("\nProcess %5d   ::   ", counter);
               printf("%.2f seconds\n", mem_test_runtimes[counter]);
           }
       #endif
    }
    else {
       MPI_Send(&runtime, 1, MPI_DOUBLE, MASTER, RUNTIME_TAG, MPI_COMM_WORLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    program_end = time(NULL);

    /****************************************************************************************************
    ** Print results                                                                                   **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       printf("======================================================================\n");
       printf("== CPU test results                                                 ==\n");
       printf("======================================================================\n\n");
       printf("The CPU test consists of taking the square root of a random number\n");
       printf("between 0 and %lu, inclusive, and repeating this process\n%lu times. ", (long) RAND_MAX, cpu_test_args->runs);
       printf("The results are shown below.\n\n");
       printf("Total number of processes:                   %10d\n", NUMBER_OF_PROCESSES);
       printf("Number of threads per process:               %10d\n\n", NUMBER_OF_PTHREADS);
       printf("Total number of threads:                     %10d\n\n", NUMBER_OF_PTHREADS * NUMBER_OF_PROCESSES);
       printf("Process summary\n");
       printf("---------------\n\n");
       runtime = 0.0;
       for (source = 0, counter = 0; source < NUMBER_OF_PROCESSES; source++) {
           printf("Process %5d:\n", source);
           for (; counter < (source * NUMBER_OF_PTHREADS) + NUMBER_OF_PTHREADS; counter++) {
               printf("\t\tThread %10lu:   %10.2f seconds\n", all_pthread_ids[counter], all_pthread_runtimes[counter]);
               runtime += all_pthread_runtimes[counter];
           }
           printf("\n");
       }
       printf("Average runtime:                     %10.2f seconds\n\n", runtime / (NUMBER_OF_PROCESSES * NUMBER_OF_PTHREADS));
       printf("======================================================================\n");
       printf("== Memory test results                                              ==\n");
       printf("======================================================================\n\n");
       printf("In the memory test, each process allocates an array whose size is\n");
       printf("between %lu and %lu, fills the elements in it with\n", MIN_SIZE, MAX_SIZE);
       printf("the same value, sleeps for %lu seconds, and then checks to see\n", mem_test_args->sleep_time->tv_sec);
       printf("if the array is not corrupted after the process wakes up. The test\n");
       printf("is repeated %d times. The results are shown below.\n\n", NUMBER_OF_RUNS);
       printf("Total number of processes:                   %10d\n\n", NUMBER_OF_PROCESSES);
       printf("Process summary\n");
       printf("---------------\n\n");
       runtime = 0.0;
       for (source = 0; source < NUMBER_OF_PROCESSES; source++) {
           printf("Process %5d:\n", source);
           printf("\t\tAverage runtime:     %10.2f seconds\n", mem_test_runtimes[source] / NUMBER_OF_RUNS);
           printf("\t\tTotal runtime:       %10.2f seconds\n\n", mem_test_runtimes[source]);
           runtime += mem_test_runtimes[source];
       }
       printf("\nAverage runtime:                     %10.2f seconds\n\n", runtime / NUMBER_OF_PROCESSES);
       printf("======================================================================\n");
       printf("== Summary                                                          ==\n");
       printf("======================================================================\n\n");
       printf("Total number of processes:                   %10d\n", NUMBER_OF_PROCESSES);
       printf("Number of threads per process:               %10d\n\n", NUMBER_OF_PTHREADS);
       printf("Total number of threads used for CPU test:   %10d\n\n", NUMBER_OF_PTHREADS * NUMBER_OF_PROCESSES);
       printf("Total runtime:                                  %10.2f seconds\n\n", difftime(program_end, program_start));
    }

    /***************************************************************************************************/

    if (PROCESS_ID == MASTER) {
       free(mem_test_runtimes);
       free(all_pthread_runtimes);
       free(all_pthread_ids);
       free(all_process_ids);
    }
    free(pthread_runtimes);
    free(pthread_ids);
    free(cpu_test_args);

    MPI_Finalize();

    free(mem_test_args->sleep_time);
    free(mem_test_args);

    return 0;

}

void* cpu_test(void* cpu_test_args) {

    int count = 0;
    int process_id = ((cpu_test_a*) cpu_test_args)->process_id;
    long runs = ((cpu_test_a*) cpu_test_args)->runs;
    long pthread_id = (long) pthread_self();
    time_t start, end;

    #ifdef DEBUG
        printf("Process %5d: Thread %lu now calculating square roots...\n", process_id, pthread_id);
    #endif

    start = time(NULL);

    while (count < runs) {
          sqrt(rand());
          count++;
    }

    end = time(NULL);

    #ifdef DEBUG
        printf("Thread %10lu   ::   Process %5d   ::   %.2f seconds\n", pthread_id, process_id, difftime(end, start));
    #endif

    cpu_test_o* cpu_test_output = (cpu_test_o*) calloc(1, sizeof(cpu_test_o));

    if (cpu_test_output == NULL) {
       return NULL;
    }

    cpu_test_output->process_id = process_id;
    cpu_test_output->pthread_id = pthread_id;
    cpu_test_output->runtime = difftime(end, start);

    return cpu_test_output;

}

mem_test_o* mem_test(mem_test_a* mem_test_args) {

    int process_id = mem_test_args->process_id;
    long array_size = mem_test_args->array_size;
    struct timespec* sleep_time = mem_test_args->sleep_time;

    int i;
    unsigned char is_not_corrupted = TRUE;

    #ifdef DEBUG
        printf("Process %5d: Now starting virtual memory test...\n", process_id);
    #endif

    char* temp = (char*) calloc(array_size, sizeof(char));

    if (temp == NULL) {
       return NULL;
    }

    initialize(temp, array_size);

    nanosleep(sleep_time, NULL);

    for (i = 0; i < array_size && is_not_corrupted == TRUE; i++) {
        is_not_corrupted = (temp[i] == 'B') ? TRUE : FALSE;
    }

    free(temp);

    mem_test_o* mem_test_output = (mem_test_o*) calloc(1, sizeof(mem_test_o));

    if (mem_test_output == NULL) {
       return NULL;
    }

    mem_test_output->process_id = process_id;
    mem_test_output->result = (is_not_corrupted) ? PASS : FAIL;

    return mem_test_output;

}

void initialize(char* array, long length) {
     int i;
     for (i = 0; i < length; i++) {
         array[i] = 'B';
     }
}