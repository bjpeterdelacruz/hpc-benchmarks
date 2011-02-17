/*!
 *
 *  \file    fileio_block.c
 *  \brief   Benchmarks parallel file I/O by reading and writing blocks of data
 *
 *  \author  BJ Peter DeLaCruz
 *
 *  \date    June 22, 2010
 *
 *  \version 1.0
 *
 *  \details \par How this program works:
 *           This program will read in N blocks of data from a file and then write those same N
 *           blocks to a new file. N is chosen at random, between 0 and RAND_MAX. The user
 *           specifies the number of times that the program will run and also how many blocks to
 *           read and write during each run. Both reading and writing are timed for each of the Q
 *           processes. Finally, data for each process, including the average time (in seconds) it
 *           took to read and write all blocks in one run, are displayed.
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
 *  \param argv[1] Smallest block size
 *  \param argv[2] Largest block size
 *  \param argv[3] Number of blocks to read from and write to file
 *  \param argv[4] Number of times that the program will run
 */
int main(int argc, char** argv) {

    /* Input filename */
    char input_filename[128];
    /* Output filename */
    char output_filename[128];

    /* Data that is read in from a file and will be written to another file */
    char* characters = NULL;

    /* Average read time for all blocks that were read in from a file */
    double average_total_read_time;
    /* Average write time for all blocks that were written to a file */
    double average_total_write_time;

    /* Holds read times for all blocks that belong to a process */
    double* read_times = NULL;
    /* Holds write times for all blocks that belong to a process */
    double* write_times = NULL;

    /* Used for error handling */
    int error_code;
    /* Number of blocks to read in from a file and write to a file */
    int NUMBER_OF_BLOCKS;
    /* Total number of processes used in this program */
    int NUMBER_OF_PROCESSES;
    /* Number of times main loop will run and also size of heads array */
    int NUMBER_OF_RUNS;
    /* Current process */
    int PROCESS_ID;
    /* Message identifier for sending/receiving read times to/from a process */
    int READ_TAG = 0;
    /* Process that sends data to other processes */
    int source;
    /* Message identifier for sending/receiving write times to/from a process */
    int WRITE_TAG = 1;

    /* Used to start timing reading in blocks of data from a file */
    time_t read_start;
    /* Used to end timing reading in blocks of data from a file */
    time_t read_end;
    /* Used to start timing writing blocks of data to a file */
    time_t write_start;
    /* Used to end timing writing blocks of data to a file */
    time_t write_end;
    /* Used to start timing program execution */
    time_t program_start;
    /* Used to end timing program execution */
    time_t program_end;

    /* Size of data to be read in from a file and written to another file */
    unsigned long int block_size;
    unsigned long int counter; /* loop counter */
    /* Position of current element in array */
    unsigned long int position;
    /* Main loop counter. Counts from 0 to N, where N = NUMBER_OF_RUNS. */
    unsigned long int program_counter;
    /* Largest block size */
    unsigned long int MAX_SIZE;
    /* Smallest block size */
    unsigned long int MIN_SIZE;

    /* Holds block sizes */
    unsigned long int* blocks = NULL;

    /* Input file */
    MPI_File input_file;
    /* Output file */
    MPI_File output_file;
    /* Used in MPI_Recv */
    MPI_Status status;

    /***************************************************************************************************/

    if (argc != 5) {
       printf("Usage: ./fileio_block ");
       printf("[minimum block size] [maximum block size] [number of blocks] [number of runs]\n");
       printf("Please try again.\n");
       exit(1);
    }

    if ((MIN_SIZE = atol(argv[1])) <= 0) {
       printf("Error: Invalid argument for minimum block size. Please try again.\n");
       exit(1);
    }

    if ((MAX_SIZE = atol(argv[2])) <= 0) {
       printf("Error: Invalid argument for maximum block size. Please try again.\n");
       exit(1);
    }

    if (MAX_SIZE <= MIN_SIZE) {
       printf("Error: Maximum block size must be greater than minimum block size. Please try again.\n");
       exit(1);
    }

    if ((NUMBER_OF_BLOCKS = atoi(argv[3])) <= 0) {
       printf("Error: Invalid argument for number of blocks. Please try again.\n");
       exit(1);
    }

    if ((NUMBER_OF_RUNS = atoi(argv[4])) <= 0) {
       printf("Error: Invalid argument for number of runs. Please try again.\n");
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

    characters  = (char*) calloc(MAX_SIZE, sizeof(char));

    if (characters == NULL) {
       printf("Memory allocation failure for characters array! ");
       printf("Unable to allocate memory on process %d.\n", PROCESS_ID);
       printf("Aborting...\n");
       MPI_Finalize();
       exit(1);
    }

    read_times = (double*) calloc(NUMBER_OF_BLOCKS * NUMBER_OF_RUNS, sizeof(double));

    if (read_times == NULL) {
       printf("Memory allocation failure for read_times array! ");
       printf("Unable to allocate memory on process %d.\n", PROCESS_ID);
       printf("Aborting...\n");
       MPI_Finalize();
       exit(1);
    }

    write_times = (double*) calloc(NUMBER_OF_BLOCKS * NUMBER_OF_RUNS, sizeof(double));

    if (write_times == NULL) {
       printf("Memory allocation failure for write_times array! ");
       printf("Unable to allocate memory on process %d.\n", PROCESS_ID);
       printf("Aborting...\n");
       MPI_Finalize();
       exit(1);
    }

    blocks = (unsigned long int*) calloc(NUMBER_OF_BLOCKS * NUMBER_OF_RUNS, sizeof(unsigned long int));

    if (blocks == NULL) {
       printf("Memory allocation failure for blocks array! ");
       printf("Unable to allocate memory on process %d.\n", PROCESS_ID);
       printf("Aborting...\n");
       MPI_Finalize();
       exit(1);
    }

    /****************************************************************************************************
    ** Begin file I/O                                                                                  **
    ****************************************************************************************************/

    sprintf(input_filename, "unsorted.txt");
    sprintf(output_filename, "blocks.txt");

    program_start = time(NULL);

    for (position = 0, program_counter = 0; program_counter < NUMBER_OF_RUNS; program_counter++) {

        for (block_size = MIN_SIZE, counter = 0; counter < NUMBER_OF_BLOCKS; counter++, position++) {

            if (MAX_SIZE <= RAND_MAX) {
               block_size = rand() % (MAX_SIZE - MIN_SIZE + 1) + MIN_SIZE;
               blocks[position] = block_size;
            }
            else if (block_size <= MAX_SIZE) {
               blocks[position] = block_size++;
            }

            read_start = time(NULL);

            /****************************************************************************************************
            ** Read in N blocks of data from file                                                              **
            ****************************************************************************************************/
            error_code = MPI_File_open(MPI_COMM_WORLD, input_filename, MPI_MODE_RDONLY, MPI_INFO_NULL, &input_file);
            error_code = MPI_File_read(input_file, &characters[0], block_size, MPI_CHAR, &status);
            error_code = MPI_File_close(&input_file);

            read_end = time(NULL);

            if (error_code != 0) {
               printf("Error encountered while reading in data.\n");
               MPI_Finalize();
               exit(1);
            }

            read_times[position] = difftime(read_end, read_start);

            write_start = time(NULL);

            /****************************************************************************************************
            ** Write N blocks of data to file                                                                  **
            ****************************************************************************************************/
            error_code = MPI_File_open(MPI_COMM_WORLD, output_filename, MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &output_file);
            error_code = MPI_File_write_ordered(output_file, &characters[0], block_size, MPI_CHAR, &status);
            error_code = MPI_File_close(&output_file);

            write_end = time(NULL);

            if (error_code != 0) {
               printf("Error encountered while writing data to file.\n");
               MPI_Finalize();
               exit(1);
            }

            write_times[position] = difftime(write_end, write_start);

            remove(output_filename); /***** Delete file after every run *****/

        }

    }

    program_end = time(NULL);

    /****************************************************************************************************
    ** Print results                                                                                   **
    ****************************************************************************************************/
    for (source = 0; source < NUMBER_OF_PROCESSES; source++) {
        if (PROCESS_ID == MASTER) {
           if (source > 0) {
              MPI_Recv(&read_times[0], NUMBER_OF_BLOCKS * NUMBER_OF_RUNS, MPI_DOUBLE, source, READ_TAG, MPI_COMM_WORLD, &status);
              MPI_Recv(&write_times[0], NUMBER_OF_BLOCKS * NUMBER_OF_RUNS, MPI_DOUBLE, source, WRITE_TAG, MPI_COMM_WORLD, &status);
           }
           printf("\n");
           printf("======================================================================\n");
           printf("== Process %5d                                                    ==\n", source);
           printf("======================================================================\n\n");
           printf("Block size\tRead time (seconds)\tWrite time (seconds)\n");
           printf("----------\t-------------------\t--------------------\n");
           for (position = 0, program_counter = 0; program_counter < NUMBER_OF_RUNS; program_counter++) {
               printf("\nRun  %5lu\n", program_counter + 1);
               printf("----------\n");
               average_total_read_time = 0.0;
               average_total_write_time = 0.0;
               for (counter = 0; counter < NUMBER_OF_BLOCKS; counter++, position++) {
                   printf("%10lu\t%19.2f\t%20.2f\n", blocks[position], read_times[position], write_times[position]);
                   average_total_read_time += read_times[position];
                   average_total_write_time += write_times[position];
               }
               average_total_read_time /= NUMBER_OF_BLOCKS;
               average_total_write_time /= NUMBER_OF_BLOCKS;
               printf("\nAverage read time:\t %10.2f seconds\n", average_total_read_time);
               printf("\nAverage write time:\t %10.2f seconds\n", average_total_write_time);
           }
        }
        else if (PROCESS_ID == source) {
           MPI_Send(&read_times[0], NUMBER_OF_BLOCKS * NUMBER_OF_RUNS, MPI_DOUBLE, MASTER, READ_TAG, MPI_COMM_WORLD);
           MPI_Send(&write_times[0], NUMBER_OF_BLOCKS * NUMBER_OF_RUNS, MPI_DOUBLE, MASTER, WRITE_TAG, MPI_COMM_WORLD);
           break;
        }
    }

    if (PROCESS_ID == MASTER) {
       printf("\n");
       printf("======================================================================\n");
       printf("== Summary                                                          ==\n");
       printf("======================================================================\n\n");
       printf("Total number of processes:           %10d\n\n", NUMBER_OF_PROCESSES);
       printf("Minimum block size:                  %10lu\n", MIN_SIZE);
       printf("Maximum block size:                  %10lu\n\n", MAX_SIZE);
       printf("Size of array (maximum block size):  %10lu\n\n", MAX_SIZE);
       printf("Total runtime:                          %10.2f seconds\n\n", difftime(program_end, program_start));
    }

    /***************************************************************************************************/

    free(blocks);
    free(write_times);
    free(read_times);
    free(characters);

    MPI_Finalize();

    return 0;

}