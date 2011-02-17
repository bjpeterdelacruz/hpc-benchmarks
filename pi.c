/*!
 *
 *  \file    pi.c
 *  \brief   Calculates the value of pi
 *
 *  \author  BJ Peter DeLaCruz
 *
 *  \date    July 1, 2010
 *
 *  \version 1.0
 *
 *  \details \par How this program works:
 *           This program calculates pi using either the Bailey-Borwein-Plouffe or Gregory-Leibniz
 *           algorithm. Given the number of iterations N, the program will divide up the
 *           calculations between Q processes; each process, except the last one, will have N / Q
 *           calculations--the last process will have N / Q + (N mod Q) calculations. After each
 *           process is done with its calculations, the results are then sent to the master, which
 *           sums all the results together. Finally, the value of pi up to the 48th digit as well
 *           as the runtimes for each process are displayed.
 *
 *           \par References:
 *           \arg <A HREF="http://en.wikipedia.org/wiki/Bailey-Borwein-Plouffe_formula">Bailey-Borwein-Plouffe Formula</A>
 *           \arg <A HREF="http://en.wikipedia.org/wiki/Leibniz_formula_for_pi">Gregory-Leibniz Series</A>
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>

/*! Master process. Usually process 0. */
#define MASTER                     0
/*! Formula: Sum[ 1/(16^i) * ( 4/(8^i+1) - 2/(8^i+4) - 1/(8^i+5) - 1/(8^i+6) ) ] */
#define BAILEY_BORWEIN_PLOUFFE     1
/*! Formula: 4 * Sum[ (-1)^i/(2i+1) ] */
#define GREGORY_LEIBNIZ            2

/*!
 *  \param argv[1] Number of calculations
 *  \param argv[2] 1 for Bailey-Borwein-Plouffe or 2 for Gregory-Leibniz
 */
int main(int argc, char** argv) {

    /* Runtime of calculations done by one process */
    double runtime;
    /* Results of calculations done by one process */
    double sum = 0.0;
    /* Value of pi after calculations */
    double total_sum = 0.0;
    /*
     * Bailey-Borwein-Plouffe: 1/(16^i)
     * Gregory-Leibniz:</B> (-1)^i/(2i+1)
     */
    double var1;

    /* Runtimes of all processes */
    double* runtimes = NULL;

    /* Used for error handling */
    int error_code;
    /* Total number of processes used in this program */
    int NUMBER_OF_PROCESSES;
    /* Current process */
    int PROCESS_ID;
    /* Message identifier for sending/receiving range size for each process */
    int RANGE_TAG = 0;
    /* Message identifier for sending/receiving runtime for a process */
    int RUNTIME_TAG = 1;
    /* Process that sends data */
    int source;
    /* Message identifier for sending/receiving subtotal */
    int SUM_TAG = 2;

    /* Keeps track of number of calculations for a process */
    long counter;
    /* Total number of calculations */
    long ITERATIONS;
    /* Upper bound of range */
    long maximum;
    /* Lower bound of range */
    long minimum;
    /* Number of calculations for a process */
    long range_size;
    /* Number of calculations that last process has to compute in addition to \b range_size */
    long remainder;

    /* Contains number of calculations for each process */
    long* ranges = NULL;

    /* Used to start timing program execution */
    time_t program_start;
    /* Used to end timing program execution */
    time_t program_end;
    /* Used to start timing calculations done by one process */
    time_t start;
    /* Used to end timing calculations done by one process */
    time_t end;

    /* Either 1 for Bailey-Borwein-Plouffe formula or 2 for Gregory-Leibniz series */
    unsigned short CHOICE;

    /* Used in MPI_Recv */
    MPI_Status status;

    /***************************************************************************************************/

    if (argc != 3) {
       printf("Usage: ./pi ");
       printf("[number of iterations] [1 = Bailey-Borwein-Plouffe, 2 = Gregory-Leibniz]\n");
       printf("Please try again.\n");
       exit(1);
    }

    if ((ITERATIONS = atol(argv[1])) <= 0) {
       printf("Error: Invalid argument for number of iterations. Please try again.\n");
       exit(1);
    }

    if ((CHOICE = atoi(argv[2])) <= 0 && CHOICE >= 3) {
       printf("Error: Invalid argument for choice of method for calculating pi. Please try again.\n");
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

    ranges = (long*) calloc(NUMBER_OF_PROCESSES, sizeof(long));

    if (ranges == NULL) {
       printf("Memory allocation failure for ranges array!");
       printf("Unable to allocate memory on process %d.\n", PROCESS_ID);
       printf("Aborting...\n");
       MPI_Finalize();
       exit(1);
    }

    srand(time(NULL));

    /****************************************************************************************************
    ** Divide number of iterations into ranges for each process                                        **
    ****************************************************************************************************/
    program_start = time(NULL);
    range_size = ITERATIONS / NUMBER_OF_PROCESSES;
    remainder = ITERATIONS % NUMBER_OF_PROCESSES;

    minimum = range_size * PROCESS_ID + 1;

    if (PROCESS_ID == MASTER) {
       minimum = 0;
    }

    maximum = range_size * (PROCESS_ID + 1);

    if (PROCESS_ID == NUMBER_OF_PROCESSES - 1) {
       maximum += remainder;
       range_size += remainder;
    }

    /****************************************************************************************************
    ** Bailey-Borwein-Plouffe formula                                                                  **
    ****************************************************************************************************/
    if (CHOICE == BAILEY_BORWEIN_PLOUFFE) {
       double var2, /* -(4 / (8k + 1)) */
              var3, /* -(2 / (8k + 4)) */
              var4, /* -(1 / (8k + 5)) */
              var5; /* -(1 / (8k + 6)) */

       start = time(NULL);
       for (counter = minimum; counter < maximum; counter++) {
           var1 = 1.0 / (pow(16.0, counter));
           var2 = 4.0 / (8.0 * counter + 1.0);
           var3 = 2.0 / ((8.0 * counter) + 4.0);
           var4 = 1.0 / (8.0 * counter + 5.0);
           var5 = 1.0 / (8.0 * counter + 6.0);
           sum += (var1 * (var2 - var3 - var4 - var5));
       }
       end = time(NULL);
    }
    /****************************************************************************************************
    ** Gregory-Leibniz series                                                                          **
    ****************************************************************************************************/
    else if (CHOICE == GREGORY_LEIBNIZ) {
       start = time(NULL);
       for (counter = minimum; counter < maximum; counter++) {
           var1 = (pow(-1.0, counter)) / (2.0 * counter + 1);
           sum += var1;
       }
       sum *= 4.0;
       end = time(NULL);
    }

    runtime = difftime(end, start);

    /****************************************************************************************************
    ** Send results to Master                                                                          **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       total_sum = sum;
       runtimes[PROCESS_ID] = runtime;
       ranges[PROCESS_ID] = range_size;
       for (source = 1; source < NUMBER_OF_PROCESSES; source++) {
           MPI_Recv(&sum, 1, MPI_DOUBLE, source, SUM_TAG, MPI_COMM_WORLD, &status);
           total_sum += sum;
           MPI_Recv(&runtimes[source], 1, MPI_DOUBLE, source, RUNTIME_TAG, MPI_COMM_WORLD, &status);
           MPI_Recv(&ranges[source], 1, MPI_LONG, source, RANGE_TAG, MPI_COMM_WORLD, &status);
       }
    }
    else {
       MPI_Send(&sum, 1, MPI_DOUBLE, MASTER, SUM_TAG, MPI_COMM_WORLD);
       MPI_Send(&runtime, 1, MPI_DOUBLE, MASTER, RUNTIME_TAG, MPI_COMM_WORLD);
       MPI_Send(&range_size, 1, MPI_LONG, MASTER, RANGE_TAG, MPI_COMM_WORLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    program_end = time(NULL);

    /****************************************************************************************************
    ** Print results                                                                                   **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       printf("\n");
       printf("The value of pi is %.48f.\n\n", total_sum);
       printf("======================================================================\n");
       printf("== Runtimes (seconds)                                               ==\n");
       printf("======================================================================\n\n");
       printf("Process          Number of iterations          Runtime\n");
       printf("-------          --------------------          -------\n\n");
       for (source = 0; source < NUMBER_OF_PROCESSES; source++) {
           printf("%7d          %20lu          %7.2f\n", source, ranges[source], runtimes[source]);
       }
       printf("\n");
       printf("======================================================================\n");
       printf("== Summary                                                          ==\n");
       printf("======================================================================\n\n");
       printf("Total number of processes:                    %10d\n\n", NUMBER_OF_PROCESSES);
       printf("Method used for calculating pi: ");
       switch (CHOICE) {
              case BAILEY_BORWEIN_PLOUFFE: printf("  Bailey-Borwein-Plouffe\n\n"); break;
              case GREGORY_LEIBNIZ:        printf("         Gregory-Leibniz\n\n"); break;
       }
       printf("Total number of iterations:         %20lu\n\n", ITERATIONS);
       printf("Total runtime:                                   %10.2f seconds\n\n", difftime(program_end, program_start));
    }

    /***************************************************************************************************/

    free(ranges);
    free(runtimes);

    MPI_Finalize();

    return 0;

}