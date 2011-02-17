/*!
 *
 *  \file    fileio.c
 *  \brief   A simple parallel file I/O benchmark
 *
 *  \author  BJ Peter DeLaCruz
 *
 *  \date    June 15, 2010
 *
 *  \version 1.0
 *
 *  \details \par How this program works:
 *           Each process in this program reads in a portion of all of the integers from a file,
 *           sorts them, and then sends the results to the master. The master puts the lists
 *           together to make one final list and sorts it. Finally, all of the processes write
 *           different parts of the final list to one file. The result is a file that contains the
 *           same numbers as the original file but with the numbers sorted in ascending order.
 *
 *           \note
 *           \arg Be sure to compile and execute \b filegen before running this program as
 *           \b filegen will create a file that will be used by this program. (The file will only
 *           contain numbers.)
 *           \arg Shell sort is used to sort the subarrays and the entire array.
 *
 *           \par Reference:
 *           <A HREF="http://goanna.cs.rmit.edu.au/~stbird/Tutorials/ShellSort.html">Shell Sort Algorithm</A>
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
 *
 *  \par Description:
 *  Finds interval for shell sort algorithm and then sorts.
 *
 *  \param my_chars Array of characters read in from file that are to be sorted
 *  \param length Size of array
 *
 */
void shell_sort(char* my_chars, int length);

/*!
 *
 *  \par Description:
 *  Sorts array in ascending order.
 *
 *  \param a Array of characters to sort
 *  \param length Size of array
 *  \param interval Distance to next element from current element
 *
 */
void shell_sort_pass(char a[], int length, int interval);

/*!
 *  \param argv[1] Size of array that will contain characters read in from file
 */
int main(int argc, char** argv) {

    /* Input filename */
    char input_filename[128];
    /* Output filename */
    char output_filename[128];

    /* Contains all characters that were read in from file */
    char* characters = NULL;
    /* Contains a subset of all characters that were read in from file */
    char* my_chars = NULL;

    /* The time it takes for a process to read in characters, sort them, and write them to file */
    double runtime;
    /* The time it takes for a process to sort all characters */
    double sort_runtime;

    /* The time it takes for all processes to read in characters from file */
    double* read_times = NULL;
    /* The time it takes for all processes to sort characters */
    double* sort_times = NULL;
    /* The time it takes for all processes to write characters to file */
    double* write_times = NULL;

    /* Message identifier for sending/receiving arrays to/from processes */
    int ARRAY_TAG = 0;
    /* Used for error handling */
    int error_code;
    /* Size of subarray */
    int MY_SIZE;
    /* Total number of processes used in this program */
    int NUMBER_OF_PROCESSES;
    /* Current element in array */
    int position;
    /* Current process */
    int PROCESS_ID;
    /* Message identifier for sending/receiving read times to/from processes */
    int READ_TAG = 1;
    /* Size of numbers array */
    int SIZE;
    /* Process that sends data to other processes */
    int source;

    /* Used to start timing reading, sorting, and writing for each process */
    time_t start;
    /* Used to end timing reading, sorting, and writing for each process */
    time_t end;
    /* Used to start timing program execution */
    time_t program_start;
    /* Used to end timing program execution */
    time_t program_end;

    /* Input file */
    MPI_File input_file;
    /* Output file */
    MPI_File output_file;
    /* Used in MPI_Recv */
    MPI_Status status;

    /***************************************************************************************************/

    if (argc != 2) {
       printf("Usage: ./fileio [size of array]\nPlease try again.\n");
       exit(1);
    }

    if ((SIZE = atoi(argv[1])) <= 0) {
       printf("Error: Invalid argument for size of array. Please try again.\n");
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

    if (SIZE % NUMBER_OF_PROCESSES != 0) {
       printf("Array size = %d\tNumber of processes = %d\n", SIZE, NUMBER_OF_PROCESSES);
       printf("Number of processes does NOT divide array size. Please try again.\n");
       printf("[For example: Array size = 24. Number of processes = 8.]\n");
       MPI_Finalize();
       exit(1);
    }

    characters = (char*) calloc(SIZE, sizeof(char));

    if (characters == NULL) {
       printf("Memory allocation failed for characters array! ");
       printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
       MPI_Finalize();
       exit(1);
    }

    MY_SIZE = SIZE / NUMBER_OF_PROCESSES;

    my_chars = (char*) calloc(MY_SIZE, sizeof(char));

    if (my_chars == NULL) {
       printf("Memory allocation failed for my_chars array! ");
       printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
       MPI_Finalize();
       exit(1);
    }

    read_times = (double*) calloc(NUMBER_OF_PROCESSES, sizeof(double));

    if (read_times == NULL) {
       printf("Memory allocation failed for read_times array! ");
       printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
       MPI_Finalize();
       exit(1);
    }

    sort_times = (double*) calloc(NUMBER_OF_PROCESSES, sizeof(double));

    if (sort_times == NULL) {
       printf("Memory allocation failed for sort_times array! ");
       printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
       MPI_Finalize();
       exit(1);
    }

    write_times = (double*) calloc(NUMBER_OF_PROCESSES, sizeof(double));

    if (write_times == NULL) {
       printf("Memory allocation failed for write_times array! ");
       printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
       MPI_Finalize();
       exit(1);
    }

    /***************************************************************************************************/

    program_start = time(NULL);

    /****************************************************************************************************
    ** Read in entire list of characters, and then send read times to Master                           **
    ****************************************************************************************************/
    sprintf(input_filename, "unsorted.txt");

    if (PROCESS_ID == MASTER) {
       printf("\nReading in file... ");
    }

    start = time(NULL);
    error_code = MPI_File_open(MPI_COMM_WORLD, input_filename, MPI_MODE_RDONLY, MPI_INFO_NULL, &input_file);
    #ifdef DEBUG
        error_code = MPI_File_read(input_file, &characters[0], SIZE, MPI_CHAR, &status);
    #endif
    error_code = MPI_File_read_at(input_file, PROCESS_ID * MY_SIZE, &my_chars[0], MY_SIZE, MPI_CHAR, &status);
    error_code = MPI_File_close(&input_file);
    end = time(NULL);

    if (error_code != 0) {
       printf("Error reading in file.\n");
       MPI_Finalize();
       exit(1);
    }

    if (PROCESS_ID == MASTER) {
       read_times[MASTER] = difftime(end, start);
       for (source = 1; source < NUMBER_OF_PROCESSES; source++) {
           MPI_Recv(&read_times[source], 1, MPI_DOUBLE, source, READ_TAG, MPI_COMM_WORLD, &status);
       }
       printf("Success!\n");
    }
    else {
       runtime = difftime(end, start);
       MPI_Send(&runtime, 1, MPI_DOUBLE, MASTER, READ_TAG, MPI_COMM_WORLD);
    }

    #ifdef DEBUG
       if (PROCESS_ID == MASTER) {
           /***** Equivalent to MPI_File_read_at code above *****/
           /****************************************************************************************************
           for (position = 0; position < MY_SIZE; position++) {
               my_chars[position] = characters[PROCESS_ID * MY_SIZE + position];
           }
           ****************************************************************************************************/

           printf("\n");
           printf("======================================================================\n");
           printf("== Initial array                                                    ==\n");
           printf("======================================================================\n\n");
           for (position = 0; position < SIZE; position++) {
               if (position % 50 == 0) {
                  printf("\n");
               }
               printf("%c ", characters[position]);
           }
           printf("\n");
       }
    #endif

    /****************************************************************************************************
    ** Sort array, and then send sort times to Master                                                  **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       printf("\nSorting %d subarrays of size %d each with %d processes... ", NUMBER_OF_PROCESSES, MY_SIZE, NUMBER_OF_PROCESSES);
    }

    start = time(NULL);
    shell_sort(my_chars, MY_SIZE);
    end = time(NULL);

    if (PROCESS_ID == MASTER) {
       sort_times[MASTER] = difftime(end, start);
       for (source = 1; source < NUMBER_OF_PROCESSES; source++) {
           MPI_Recv(&sort_times[source], 1, MPI_DOUBLE, source, READ_TAG, MPI_COMM_WORLD, &status);
       }
       printf("Done!\n");
    }
    else {
       runtime = difftime(end, start);
       MPI_Send(&runtime, 1, MPI_DOUBLE, MASTER, READ_TAG, MPI_COMM_WORLD);
    }

    /****************************************************************************************************
    ** Sort entire array after getting sorted subarrays from workers                                   **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       int destination; /* process that receives data from master */

       for (position = 0; position < MY_SIZE; position++) {
           characters[position] = my_chars[position];
       }

       for (source = 1; source < NUMBER_OF_PROCESSES; source++) {
           MPI_Recv(&characters[source * MY_SIZE], MY_SIZE, MPI_CHAR, source, ARRAY_TAG, MPI_COMM_WORLD, &status);
       }

       printf("\nReceived %d subarrays from workers. Process %d now sorting array... ", NUMBER_OF_PROCESSES - 1, PROCESS_ID);

       start = time(NULL);
       shell_sort(characters, SIZE);
       end = time(NULL);

       sort_runtime = difftime(end, start);
       printf("Done!\n");

       for (destination = 1; destination < NUMBER_OF_PROCESSES; destination++) {
           MPI_Send(&characters[destination * MY_SIZE], MY_SIZE, MPI_CHAR, destination, ARRAY_TAG, MPI_COMM_WORLD);
       }

    }
    else {
       MPI_Send(&my_chars[0], MY_SIZE, MPI_CHAR, MASTER, ARRAY_TAG, MPI_COMM_WORLD);
       MPI_Recv(&characters[PROCESS_ID * MY_SIZE], MY_SIZE, MPI_CHAR, MASTER, ARRAY_TAG, MPI_COMM_WORLD, &status);
    }

    /****************************************************************************************************
    ** Write sorted array to output file                                                               **
    ****************************************************************************************************/
    sprintf(output_filename, "sorted.txt");

    if (PROCESS_ID == MASTER) {
       printf("\n%d processes now writing different parts of sorted array to file... ", NUMBER_OF_PROCESSES);
    }

    start = time(NULL);
    error_code = MPI_File_open(MPI_COMM_WORLD, output_filename, MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &output_file);
    error_code = MPI_File_write_ordered(output_file, &characters[PROCESS_ID * MY_SIZE], MY_SIZE, MPI_CHAR, &status);
    error_code = MPI_File_close(&output_file);
    end = time(NULL);

    if (error_code != 0) {
       printf("Error writing file.\n");
       MPI_Finalize();
       exit(1);
    }

    if (PROCESS_ID == MASTER) {
       write_times[MASTER] = difftime(end, start);
       for (source = 1; source < NUMBER_OF_PROCESSES; source++) {
           MPI_Recv(&write_times[source], 1, MPI_DOUBLE, source, READ_TAG, MPI_COMM_WORLD, &status);
       }
       printf("Success!\n");
    }
    else {
       runtime = difftime(end, start);
       MPI_Send(&runtime, 1, MPI_DOUBLE, MASTER, READ_TAG, MPI_COMM_WORLD);
    }

    /****************************************************************************************************
    ** Read in output file, then double-check to see if its contents were sorted                       **
    ****************************************************************************************************/
    sprintf(input_filename, "sorted.txt");

    if (PROCESS_ID == MASTER) {
       printf("\nNow reading in output file and checking to see if it was written to correctly... ");
    }

    error_code = MPI_File_open(MPI_COMM_WORLD, input_filename, MPI_MODE_RDONLY, MPI_INFO_NULL, &input_file);
    error_code = MPI_File_read(input_file, &characters[0], SIZE, MPI_CHAR, &status);
    error_code = MPI_File_close(&input_file);

    if (error_code != 0) {
       printf("Error reading in file.\n");
       MPI_Finalize();
       exit(1);
    }

    /****************************************************************************************************
    ** Print results                                                                                   **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       unsigned char is_sorted; /* TRUE if entire array is sorted in ascending order */

       for (position = 0, is_sorted = TRUE; position < SIZE - 1 && is_sorted != FALSE; position++) {
           is_sorted = (characters[position] <= characters[position+ 1]) ? TRUE : FALSE;
       }

       if (is_sorted) {
           printf("Success!\n\nThe contents of the output file were sorted. Displaying results...\n\n");
       }
       else {
           printf("\n\nThe contents of the file were NOT sorted. Aborting...\n\n");
           MPI_Finalize();
           exit(1);
       }

       program_end = time(NULL);

       printf("======================================================================\n");
       printf("== Read times                                                       ==\n");
       printf("======================================================================\n\n");
       printf("Note: Each process reads in %d characters from a file and\n", SIZE);
       printf("      also another %d characters from the same file.\n\n", SIZE / NUMBER_OF_PROCESSES);
       printf("Process\t\tNumber of characters\t\tSeconds\n");
       printf("-------\t\t--------------------\t\t-------\n");
       for (position = 0; position < NUMBER_OF_PROCESSES; position++) {
           printf("%7d\t\t%20d\t\t%7.2f\n", position, SIZE + SIZE / NUMBER_OF_PROCESSES, read_times[position]);
       }
       printf("\n");

       printf("======================================================================\n");
       printf("== Sort times                                                       ==\n");
       printf("======================================================================\n\n");
       printf("Process\t\t          Array size\t\tSeconds\n");
       printf("-------\t\t          ----------\t\t-------\n");
       for (position = 0; position < NUMBER_OF_PROCESSES; position++) {
           printf("%7d\t\t%20d\t\t%7.2f\n", position, SIZE / NUMBER_OF_PROCESSES, sort_times[position]);
       }
       printf("\n");

       printf("======================================================================\n");
       printf("== Write times                                                      ==\n");
       printf("======================================================================\n\n");
       printf("Process\t\tNumber of characters\t\tSeconds\n");
       printf("-------\t\t--------------------\t\t-------\n");
       for (position = 0; position < NUMBER_OF_PROCESSES; position++) {
           printf("%7d\t\t%20d\t\t%7.2f\n", position, SIZE / NUMBER_OF_PROCESSES, write_times[position]);
       }
       printf("\n");

       #ifdef DEBUG
           printf("======================================================================\n");
           printf("== Sorted array                                                     ==\n");
           printf("======================================================================\n\n");
           for (position = 0; position < SIZE; position++) {
               if (position % 50 == 0) {
                  printf("\n");
               }
               printf("%c ", characters[position]);
           }
           printf("\n");
       #endif

       printf("======================================================================\n");
       printf("== Summary                                                          ==\n");
       printf("======================================================================\n\n");
       printf("Total number of processes:                %10d\n\n", NUMBER_OF_PROCESSES);
       printf("Array size:                               %10d\n", SIZE);
       printf("Size of each subarray\n");
       printf("     (array size / number of processes):  %10d\n\n", MY_SIZE);
       printf("Time for process %d to sort entire array:     %10.2f seconds\n\n", PROCESS_ID, sort_runtime);
       printf("Total runtime:                               %10.2f seconds\n\n", difftime(program_end, program_start));
    }

    /***************************************************************************************************/

    free(write_times);
    free(sort_times);
    free(read_times);
    free(my_chars);
    free(characters);

    remove(output_filename);

    MPI_Finalize();

    return 0;

}

void shell_sort(char *my_chars, int length) {
    int ciura_intervals[] = {701, 301, 132, 57, 23, 10, 4, 1};
    double extend_ciura_multiplier = 2.3;

    int interval_idx = 0;
    int interval = ciura_intervals[0];
    if (length > interval) {
        while (length > interval) {
            interval_idx--;
            interval = (int) (interval * extend_ciura_multiplier);
        }
    } else {
        while (length < interval) {
            interval_idx++;
            interval = ciura_intervals[interval_idx];
        }
    }

    while (interval > 1) {
        interval_idx++;
        if (interval_idx >= 0) {
            interval = ciura_intervals[interval_idx];
        } else {
            interval = (int) (interval / extend_ciura_multiplier);
        }

        shell_sort_pass(my_chars, length, interval);
    }
}

void shell_sort_pass(char a[], int length, int interval) {
    int i;
    for (i=0; i < length; i++) {
        /* Insert a[i] into the sorted sublist */
        int j, v = a[i];

        for (j = i - interval; j >= 0; j -= interval) {
            if (a[j] <= v) break;
            a[j + interval] = a[j];
        }
        a[j + interval] = v;
   }
}