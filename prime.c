/*!
 *
 *  \file    prime.c
 *  \brief   Finds all prime numbers between 0 and N
 *
 *  \author  BJ Peter DeLaCruz
 *
 *  \date    July 1, 2010
 *
 *  \version 1.0
 *
 *  \details \par How this program works:
 *           Given a number N, this program will divide that number into N / Q search ranges for
 *           each of the Q processes. Each process will test each number between the minimum and
 *           maximum limits of its range, inclusive, for primality by dividing it by the divisor,
 *           which starts at three, gets incremented by two, and stops at the square root of N. If
 *           the number is divisible, then that number is not prime but rather composite, and the
 *           loop terminates; otherwise, it is prime, and the process adds one to its total count.
 *           Finally, the runtimes and total counts of each process are displayed.
 *
 *           \par Reference:
 *           <A HREF="http://www.troubleshooters.com/codecorn/primenumbers/primenumbers.htm">Fun With Prime Numbers</A>
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>

/*! Master process. Usually process 0. */
#define MASTER      0
#define TRUE        1
#define FALSE       0

/*!
 *  \param argv[1] Highest number to test for primality
 */
int main(int argc, char** argv) {

    /* Used to calculate search runtime for a process */
    double runtime;

    /* Contains the search runtimes for each process */
    double* runtimes = NULL;

    /* Used for error handling */
    int error_code;
    /* Total number of processes used in this program */
    int NUMBER_OF_PROCESSES;
    /* Message identifier for sending/receiving number of primes found */
    int PRIME_TAG = 0;
    /* Current process */
    int PROCESS_ID;
    /* Used to give last process remaining numbers to test */
    int remainder;
    /* Message identifier for sending/receiving runtimes for a process */
    int RUNTIME_TAG = 1;
    /* Process that sends data to other processes */
    int source;

    /* Contains number of primes found by each process */
    long* number_of_primes = NULL;

    /* Used to determine whether current number is prime */
    long divisor;
    /* Highest number to test for primality. Entered by the user as a command-line argument. */
    long MAXIMUM;
    /* Upper bound of range to search for prime numbers for one process */
    long maximum;
    /* Lower bound of range to search for prime numbers for one process */
    long minimum;
    /* Range for a process to search for prime numbers */
    long range_size;
    /* Used to count all prime numbers found between 0 and N */
    long total_number_of_primes = 0;

    /* Used to test if current number is prime */
    unsigned char is_prime;

    /* Used to start timing program execution */
    time_t program_start;
    /* Used to end timing program execution */
    time_t program_end;
    /* Used to start timing search for prime numbers for one process */
    time_t start;
    /* Used to end timing search for prime numbers for one process */
    time_t end;

    /* Used in MPI_Recv */
    MPI_Status status;

    /***************************************************************************************************/

    if (argc != 2) {
       printf("Usage: ./prime ");
       printf("[highest number to test for primality]\n");
       printf("Please try again.\n");
       exit(1);
    }

    if ((MAXIMUM = atol(argv[1])) <= 0) {
       printf("Error: Invalid argument for highest number to test for primality. Please try again.\n");
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

    runtimes = (double*) calloc(NUMBER_OF_PROCESSES, sizeof(double));

    if (runtimes == NULL) {
       printf("Memory allocation failure for runtimes array!");
       printf("Unable to allocate memory on process %d.\n", PROCESS_ID);
       printf("Aborting...\n");
       MPI_Finalize();
       exit(1);
    }

    number_of_primes = (long*) calloc(NUMBER_OF_PROCESSES, sizeof(long));

    if (number_of_primes == NULL) {
       printf("Memory allocation failure for number_of_primes array!");
       printf("Unable to allocate memory on process %d.\n", PROCESS_ID);
       printf("Aborting...\n");
       MPI_Finalize();
       exit(1);
    }

    srand(time(NULL));

    /****************************************************************************************************
    ** Divide number of iterations into ranges for each process and then start finding prime numbers   **
    ****************************************************************************************************/
    program_start = time(NULL);
    range_size = MAXIMUM / NUMBER_OF_PROCESSES;
    remainder = MAXIMUM % NUMBER_OF_PROCESSES;

    if (PROCESS_ID == MASTER) {
       total_number_of_primes = 1;
       minimum = 3;
       maximum = range_size;
    }
    else {
       minimum = range_size * PROCESS_ID + 1;
       if (minimum % 2 == 0) {
          minimum++;
       }
       maximum = range_size * (PROCESS_ID + 1);
       if (PROCESS_ID == NUMBER_OF_PROCESSES - 1) {
          maximum += remainder;
       }
    }

    /* TODO: Algorithm is inefficient and needs improvement. Runtime is O(n^2). */
    start = time(NULL);
    while (minimum <= maximum) {
          for (divisor = 3, is_prime = TRUE; divisor * divisor <= minimum && is_prime == TRUE; divisor += 2) {
              if (minimum % divisor == 0) {
                 is_prime = FALSE;
              }
          }

          if (is_prime) {
             total_number_of_primes++;
          }

          minimum += 2;
    }
    end = time(NULL);

    runtime = difftime(end, start);

    /****************************************************************************************************
    ** Send runtimes and number of primes found to Master                                              **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       number_of_primes[PROCESS_ID] = total_number_of_primes;
       runtimes[PROCESS_ID] = runtime;
       for (source = 1; source < NUMBER_OF_PROCESSES; source++) {
           MPI_Recv(&number_of_primes[source], 1, MPI_LONG, source, PRIME_TAG, MPI_COMM_WORLD, &status);
           MPI_Recv(&runtimes[source], 1, MPI_DOUBLE, source, RUNTIME_TAG, MPI_COMM_WORLD, &status);
       }
    }
    else {
       MPI_Send(&total_number_of_primes, 1, MPI_LONG, MASTER, PRIME_TAG, MPI_COMM_WORLD);
       MPI_Send(&runtime, 1, MPI_DOUBLE, MASTER, RUNTIME_TAG, MPI_COMM_WORLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    program_end = time(NULL);

    /****************************************************************************************************
    ** Print results                                                                                   **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       printf("\n");
       printf("This program found prime numbers up to %lu.\nThe results are displayed below.\n", MAXIMUM);
       printf("\n");
       printf("Process          Total found          Runtime (seconds)\n");
       printf("-------          -----------          -----------------\n\n");
       total_number_of_primes = 0;
       for (source = 0; source < NUMBER_OF_PROCESSES; source++) {
           printf("%7d          %11lu          %17.2f\n", source, number_of_primes[source], runtimes[source]);
           total_number_of_primes += number_of_primes[source];
       }
       printf("\n");
       printf("======================================================================\n");
       printf("== Summary                                                          ==\n");
       printf("======================================================================\n\n");
       printf("Total number of processes:           %10d\n\n", NUMBER_OF_PROCESSES);
       printf("Prime numbers found: %26lu\n\n", total_number_of_primes);
       printf("Total runtime:                          %10.2f seconds\n\n", difftime(program_end, program_start));
    }

    /***************************************************************************************************/

    free(number_of_primes);
    free(runtimes);

    MPI_Finalize();

    return 0;

}