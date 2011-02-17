/*!
 *
 *  \file    sndrcv.c
 *  \brief   Benchmarks a virtual linear array of processes
 *
 *  \author  BJ Peter DeLaCruz
 *
 *  \date    June 16, 2010
 *
 *  \version 1.0
 *
 *  \details \par How this program works:
 *           This program creates a huge 1-dimensional array of characters. Process <B>N</B> then sends it to
 *           process <B>(N + 1 + Q) mod Q</B> (where Q = number of processes), which sends it to yet another
 *           process, process <B>(N + 2 + Q) mod Q</B>. This action is repeated until process
 *           <B>(N - 1 + Q) mod Q</B> is reached. Then these steps are repeated P times. After the Pth run,
 *           the total runtime and the average time it takes to send the array from the first process (head)
 *           to the last process (tail) are calculated. Also, at the beginning of each run, the head is
 *           picked at random.
 *
 *           \par Examples:
 *           8 processes.
 *           \arg 0 --> 1 --> 2 --> 3 --> 4 --> 5 --> 6 --> 7
 *           \arg 3 --> 4 --> 5 --> 6 --> 7 --> 0 --> 1 --> 2
 *           \arg 7 --> 0 --> 1 --> 2 --> 3 --> 4 --> 5 --> 6
 *
 *           \note
 *           This program will work with any number of processes.
 *
 *           \par Reference:
 *           <A HREF="http://navet.ics.hawaii.edu/~casanova/courses/ics632_fall08/slides/ics632_1Dcomm.ppt">Communication on a Ring</A>
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>

/*! Master process. Usually process 0. */
#define MASTER      0

/*!
 *
 *  \par Description:
 *  A virtual linear array of processes is used to send data from one process to
 *  another. Process <B>N</B>, where N = broadcast_id, sends data to process
 *  <B>(N + 1 + Q) mod Q</B> (where Q = number of processes), which sends it to process
 *  <B>(N + 2 + Q) mod Q</B>, and so forth, until process <B>(N - 1 + Q) mod Q</B> is reached.
 *
 *  \par Examples:
 *  8 processes.
 *  \arg 0 --> 1 --> 2 --> 3 --> 4 --> 5 --> 6 --> 7
 *  \arg 5 --> 6 --> 7 --> 0 --> 1 --> 2 --> 3 --> 4
 *  \arg 3 --> 4 --> 5 --> 6 --> 7 --> 0 --> 1 --> 2
 *
 *  \param broadcast_id First process that sends array (head)
 *  \param number_of_processes Total number of processes
 *  \param array Data to be broadcasted
 *  \param size Number of elements in array
 *
 */
void broadcast(int broadcast_id, int number_of_processes, char* array, int size);

/*!
 *
 *  \par Description:
 *  Assigns values to elements in array.
 *
 *  \param array Empty array, to be used in \c broadcast function
 *  \param length Number of elements in array
 *
 */
void initialize(char* array, int length);

/*!
 *  \param argv[1] Size of array that will contain characters
 *  \param argv[2] Number of times that the program will run
 */
int main(int argc, char** argv) {

    /* List of runtimes per run */
    double* times = NULL;

    /* Used for error handling */
    int error_code;
    /* Process that sends data first */
    int head;
    /* Total number of processes used in this program */
    int NUMBER_OF_PROCESSES;
    /* Number of times main loop will run and also size of heads array */
    int NUMBER_OF_RUNS;
    /* Current process */
    int PROCESS_ID;
    /* Main loop counter. Counts from <B>0 to N</B>, where N = NUMBER_OF_RUNS */
    int program_counter;
    /* Message identifier for sending/receiving runtime for a process */
    int RUNTIME_TAG = 0;
    /* Size of characters array */
    int SIZE;

    /* Random data that is broadcasted from head */
    char* characters = NULL;
    /* Keeps track of processes that were heads */
    int* heads = NULL;

    /* Used to start timing program execution */
    time_t program_start;
    /* Used to end timing program execution */
    time_t program_end;
    /* Runtime of current run; entire program */
    time_t runtime;
    /* Used to start timing broadcast from head to tail */
    time_t start;
    /* Used to end timing broadcast from head to tail */
    time_t end;

    /* Used in MPI_Recv */
    MPI_Status status;

    /***************************************************************************************************/

    if (argc != 3) {
       printf("Usage: ./sndrcv [size of array] [number of runs]\nPlease try again.\n");
       exit(1);
    }

    if ((SIZE = atoi(argv[1])) == 0) {
       printf("Error: Invalid argument for size of array. Please try again.\n");
       exit(1);
    }

    if ((NUMBER_OF_RUNS = atoi(argv[2])) == 0) {
       printf("Error: Invalid argument for number of runs. Please try again.\n");
       exit(1);
    }

    /***************************************************************************************************/

    characters = (char*) calloc(SIZE, sizeof(char));

    if (characters == NULL) {
       printf("Memory allocation failed for characters array! Aborting...\n");
       exit(1);
    }

    heads = (int*) calloc(NUMBER_OF_RUNS, sizeof(int));

    if (heads == NULL) {
       printf("Memory allocation failed for heads array! Aborting...\n");
       exit(1);
    }

    times = (double*) calloc(NUMBER_OF_RUNS, sizeof(double));

    if (times == NULL) {
       printf("Memory allocation failed for times array! Aborting...\n");
       exit(1);
    }

    error_code = MPI_Init(&argc, &argv);
    error_code = MPI_Comm_size(MPI_COMM_WORLD, &NUMBER_OF_PROCESSES);
    error_code = MPI_Comm_rank(MPI_COMM_WORLD, &PROCESS_ID);

    if (error_code != 0) {
       printf("Error encountered while initializing MPI and obtaining task information.\n");
       MPI_Finalize();
       exit(1);
    }

    srand(time(NULL));

    /***************************************************************************************************/

    program_start = time(NULL);

    for (program_counter = 0; program_counter < NUMBER_OF_RUNS; program_counter++) {
        /***** Randomize contents of array and also which process gets to send data first *****/
        initialize(characters, SIZE);

        head = rand() % NUMBER_OF_PROCESSES;
        *(heads + program_counter) = head;

        start = time(NULL);
        broadcast(head, NUMBER_OF_PROCESSES, characters, SIZE);
        end = time(NULL);

        if (PROCESS_ID == MASTER && head != MASTER) {
           MPI_Recv(&times[program_counter], 1, MPI_DOUBLE, MPI_ANY_SOURCE, RUNTIME_TAG, MPI_COMM_WORLD, &status);
        }
        else if (head == MASTER) {
           times[program_counter] = difftime(end, start);
        }
        else if (PROCESS_ID == head) {
           runtime = difftime(end, start);
           MPI_Send(&runtime, 1, MPI_DOUBLE, MASTER, RUNTIME_TAG, MPI_COMM_WORLD);
        }

    }

    MPI_Barrier(MPI_COMM_WORLD);
    program_end = time(NULL);

    /****************************************************************************************************
    ** Print results                                                                                   **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       int tail; /* last process */

       printf("\n");
       printf("======================================================================\n");
       printf("== Run Information                                                  ==\n");
       printf("======================================================================\n\n");
       printf("Total number of runs: %d\n\n", NUMBER_OF_RUNS);
       printf("Notes:\n");
       printf("-- Head is the process that sent the array first.\n");
       printf("-- Tail is the process that received the array last.\n");
       printf("-- Runtime is measured in seconds.\n\n");
       printf("Process\t\tHead\t\tTail\t\t    Runtime\n");
       printf("-------\t\t----\t\t----\t\t    -------\n");
       runtime = 0.0;
       for (program_counter = 0; program_counter < NUMBER_OF_RUNS; program_counter++) {
           head = *(heads + program_counter);
           tail = (head - 1 + NUMBER_OF_PROCESSES) % NUMBER_OF_PROCESSES;
           printf("%7d\t\t%4d\t\t%4d\t\t    %7.2f\n", program_counter, head, tail, times[program_counter]);
           runtime += times[program_counter];
       }
       printf("\n");
       printf("======================================================================\n");
       printf("== Summary                                                          ==\n");
       printf("======================================================================\n\n");
       printf("Total number of processes:                    %10d\n\n", NUMBER_OF_PROCESSES);
       printf("Array size:                                   %10d\n\n",  SIZE);
       printf("Average time to send array from head to tail:    %10.2f seconds\n\n", runtime / (double) NUMBER_OF_RUNS);
       printf("Total runtime:                                   %10.2f seconds\n\n", difftime(program_end, program_start));
    }

    /***************************************************************************************************/

    MPI_Finalize();

    free(times);
    free(heads);
    free(characters);

    return 0;

}

void broadcast(int broadcast_id, int number_of_processes, char* array, int size) {

    int my_id, successor_id, predecesor_id, message_tag = 0;

    MPI_Status status;

    if (MPI_Comm_rank(MPI_COMM_WORLD, &my_id) != 0) {
       printf("Error getting process ID.\n");
       MPI_Finalize();
       exit(1);
    }

    predecesor_id = (my_id - 1 + number_of_processes) % number_of_processes;
    successor_id = (my_id + 1 + number_of_processes) % number_of_processes;

    if (my_id == broadcast_id) {
       MPI_Send(array, size, MPI_CHAR, successor_id, message_tag, MPI_COMM_WORLD);
       #ifdef DEBUG
           printf("Process %d broadcasting...\n", my_id);
       #endif
    }
    else {
       MPI_Recv(array, size, MPI_CHAR, predecesor_id, message_tag, MPI_COMM_WORLD, &status);
       #ifdef DEBUG
           printf("Process %d received array from process %d.\n", my_id, predecesor_id);
           if (broadcast_id == successor_id) {
              printf("End of broadcast.\n");
           }
       #endif
       if (broadcast_id != successor_id) {
          MPI_Send(array, size, MPI_CHAR, successor_id, message_tag, MPI_COMM_WORLD);
          #ifdef DEBUG
              printf("Process %d sent array to process %d.\n", my_id, successor_id);
          #endif
       }
    }
}

void initialize(char* array, int size) {
     int i;
     for (i = 0; i < size; i++) {
         array[i] = 'B';
     }
}