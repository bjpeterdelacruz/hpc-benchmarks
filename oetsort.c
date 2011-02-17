/*!
 *
 *  \file    oetsort.c
 *  \brief   Implementation of the 2-dimensional odd-even transposition sort algorithm
 *
 *  \author  BJ Peter DeLaCruz
 *
 *  \date    June 10, 2010
 *
 *  \version 1.0
 *
 *  \details \par How this program works:
 *           Given an N x N matrix, the program will first give N / Q rows to each process, where
 *           Q = number of processes. Processes with even IDs will sort the odd indices first in
 *           ascending order and then the even indices in ascending order also; those with odd IDs will
 *           do the same, except in descending order. The results are then sent back to the master.
 *           Next, the program gives N/8 columns to each processor, which will sort the odd indices
 *           first in ascending order and then the even indices in ascending order also. The results are
 *           then sent back to the master, which will check to see if the matrix is sorted in
 *           "snake-like" order (diagonally, in ascending order). If it is not, this process is repeated
 *           until it is sorted.
 *
 *           \note
 *           Unfortunately, the time complexity of this program is O(\f$n^2\f$) because bubble sort is
 *           used to sort the rows and columns, so it is recommended that the value for argv[1] should
 *           not be too large.
 *
 *           \par Reference:
 *           <A HREF="http://www.inf.fh-flensburg.de/lang/algorithmen/sortieren/twodim/oets2d/oets2dsorten.htm">2-dimensional Odd-even Transposition Sort Algorithm</A>
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
 *  Assigns random values to elements in matrix.
 *
 *  \param matrix Empty 2-dimensional array
 *  \param width Number of columns in matrix
 *  \param height Number of rows in matrix
 *
 */
void initialize(int* matrix, int width, int height);

/*!
 *
 *  \par Description:
 *  Sorts even indices in array in ascending order.
 *
 *  \param row 1-dimensional subarray in \b matrix
 *  \param length Size of subarray
 *
 */
void esort(int* row, int length);

/*!
 *
 *  \par Description:
 *  Sorts even indices in array in descending order.
 *
 *  \param row 1-dimensional subarray in \b matrix
 *  \param length Size of subarray
 *
 */
void ersort(int* row, int length);

/*!
 *
 *  \par Description:
 *  Sorts odd indices in array in ascending order.
 *
 *  \param row 1-dimensional subarray in \b matrix
 *  \param length Size of subarray
 *
 */
void osort(int* row, int length);

/*!
 *
 *  \par Description:
 *  Sorts odd indices in array in descending order.
 *
 *  \param row 1-dimensional subarray in \b matrix
 *  \param length Size of subarray
 *
 */
void orsort(int* row, int length);

/*!
 *
 *  \par Description:
 *  Prints matrix to the screen.
 *
 *  \param matrix 2-dimensional array
 *  \param width Number of columns in \b matrix
 *  \param height Number of rows in \b matrix
 *
 */
void print_matrix(int* matrix, int width, int height);

/*!
 *
 *  \par Description:
 *  Allocates storage space for a matrix on the heap.
 *
 *  \note
 *  Not called in program as matrices are currently being allocated on the stack.
 *
 *  \param height Number of rows in matrix
 *  \param width Number of columns in matrix
 *
 */
double** create_matrix(int height, int width);

/*!
 *
 *  \par Description:
 *  Frees up storage space occupied by a matrix on the heap.
 *
 *  \note
 *  Not called in program as matrices are currently being allocated on the stack.
 *
 *  \param matrix 2-dimensional array
 *  \param height Number of rows in matrix
 *
 */
void destroy_matrix(double** matrix, int height);

/*!
 *  \param argv[1] Dimension of square matrix, i.e. number of rows = number of columns
 */
int main(int argc, char** argv) {

    /* Message identifier for sending/receiving columns in a matrix */
    int COLUMN_TAG = 0;
    /* Dimension of square matrix */
    int DIMENSION;
    /* Used for error handling */
    int error_code;
    int i, j; /* loop counters */
    /* Message identifier for sending/receiving \b is_sorted flag */
    int IS_SORTED_TAG = 1;
    /* Total number of processes used in this program */
    int NUMBER_OF_PROCESSES;
    /* Number of rows that are assigned to each process */
    int NUMBER_OF_ROWS_PER_PROCESS;
    /* Current process */
    int PROCESS_ID;
    /* Message identifier for sending/receiving rows in a matrix */
    int ROW_TAG = 1;

    #ifdef DEBUG
        /* Counts number of runs needed to sort matrix */
        int counter = 0;
    #endif DEBUG

    /* Contains a row or column from the matrix */
    int* numbers = NULL;

    /* Used to check whether matrix is sorted diagonally in ascending order */
    unsigned char is_sorted;

    /* Used to start timing program execution */
    time_t program_start;
    /* Used to end timing program execution */
    time_t program_end;

    /* Derived datatype for sending a column in a matrix to a process */
    MPI_Datatype column_type;
    /* Used in MPI_Recv */
    MPI_Status status;

    /***************************************************************************************************/

    if (argc != 2) {
       printf("Usage: ./oetsort [dimension of square matrix]\nPlease try again.\n");
       exit(1);
    }

    if ((DIMENSION = atoi(argv[1])) == 0) {
       printf("Error: Invalid argument for dimension of square matrix. Please try again.\n");
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

    if (DIMENSION % NUMBER_OF_PROCESSES != 0) {
       printf("Dimension of square matrix = %d\tNumber of processes = %d\n", DIMENSION, NUMBER_OF_PROCESSES);
       printf("Number of processes does NOT divide dimension of square matrix. Please try again.\n");
       printf("[For example: Dimension of square matrix = 24. Number of processes = 8.]\n");
       MPI_Finalize();
       exit(1);
    }

    srand(time(NULL));

    /***************************************************************************************************/

    NUMBER_OF_ROWS_PER_PROCESS = DIMENSION / NUMBER_OF_PROCESSES;

    int matrix[DIMENSION][DIMENSION];

    numbers = (int*) calloc(DIMENSION, sizeof(int));

    if (numbers == NULL) {
       printf("Memory allocation failed for numbers array! ");
       printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
       MPI_Finalize();
       exit(1);
    }

    /***** Create a derived datatype that contains all of the elements in a column *****/
    error_code = MPI_Type_vector(DIMENSION, 1, DIMENSION, MPI_INT, &column_type);
    error_code = MPI_Type_commit(&column_type);

    if (error_code != 0) {
       printf("Error creating derived datatype.\n");
       MPI_Finalize();
       exit(1);
    }

    /****************************************************************************************************
    ** MASTER                                                                                          **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       int current_column,
           current_row,
           destination,     /* process that receives data from master */
           previous_column,
           previous_row,
           source;          /* process that sent data to master       */

       initialize(&matrix[0][0], DIMENSION, DIMENSION);

       #ifdef DEBUG
           /*************************************************************************************
           ** Note: Be sure that DIMENSION is not too large so that the matrix will be small   **
           **       enough to be viewable. --BPD                                               **
           *************************************************************************************/
           printf("\n");
           printf("======================================================================\n");
           printf("== Initial matrix                                                   ==\n");
           printf("======================================================================\n\n");
           print_matrix(&matrix[0][0], DIMENSION, DIMENSION);
           printf("\n");
       #endif

       program_start = time(NULL);

       do {
           #ifdef DEBUG
               printf("////////// Begin pass %d //////////\n", ++counter);
           #endif
           current_row = 0;
           current_column = 0;

           /****************************************************************************************************
           ** Send unsorted rows to workers and then get partially sorted rows from them                      **
           ****************************************************************************************************/
           #ifdef DEBUG
               printf("   Sending rows to workers...\n");
           #endif
           for (i = 0; i < NUMBER_OF_ROWS_PER_PROCESS; i++) {
               previous_row = current_row;

               for (destination = 1; destination < NUMBER_OF_PROCESSES; destination++) {
                   MPI_Send(&matrix[current_row++][0], DIMENSION, MPI_INT, destination, ROW_TAG, MPI_COMM_WORLD);
               }

               #ifdef DEBUG
                   printf(">> Master now sorting row %d...\n", i * NUMBER_OF_ROWS_PER_PROCESS);
               #endif
               osort(&matrix[current_row][0], DIMENSION);
               esort(&matrix[current_row++][0], DIMENSION);

               for (source = 1; source < NUMBER_OF_PROCESSES; source++) {
                   MPI_Recv(&matrix[previous_row++][0], DIMENSION, MPI_INT, source, ROW_TAG, MPI_COMM_WORLD, &status);
               }
           }
           #ifdef DEBUG
               printf("   Received rows from workers. ");
           #endif

           /****************************************************************************************************
           ** Send unsorted columns to workers and then get partially sorted columns from them                **
           ****************************************************************************************************/
           #ifdef DEBUG
               printf("Sending columns to workers...\n");
           #endif
           for (j = 0; j < NUMBER_OF_ROWS_PER_PROCESS; j++) {
               previous_column = current_column;

               for (destination = 1; destination < NUMBER_OF_PROCESSES; destination++) {
                   MPI_Send(&matrix[0][current_column++], 1, column_type, destination, COLUMN_TAG, MPI_COMM_WORLD);
               }

               #ifdef DEBUG
                   printf(">> Master now sorting column %d...\n", j * NUMBER_OF_ROWS_PER_PROCESS);
               #endif
               for (current_row = 0; current_row < DIMENSION; current_row++) {
                   numbers[current_row] = matrix[current_row][current_column];
               }
               osort(numbers, DIMENSION);
               esort(numbers, DIMENSION);
               for (current_row = 0; current_row < DIMENSION; current_row++) {
                   matrix[current_row][current_column] = numbers[current_row];
               }
               current_column++;

               for (source = 1; source < NUMBER_OF_PROCESSES; source++) {
                   MPI_Recv(&matrix[0][previous_column++], 1, column_type, source, COLUMN_TAG, MPI_COMM_WORLD, &status);
               }
           }
           #ifdef DEBUG
               printf("   Received columns from workers.\n");
           #endif

           /****************************************************************************************************
           ** Check if diagonals below and above the main diagonal are sorted in ascending order              **
           ****************************************************************************************************/
           #ifdef DEBUG
               printf(">> Master now checking if matrix is sorted...\n");
           #endif
           is_sorted = TRUE;
           for (i = 0; i < DIMENSION - 1 && is_sorted == TRUE; i++) {
               for (current_row = i, current_column = 0; current_row < DIMENSION - 1 && is_sorted == TRUE; current_row++, current_column++) {
                   if (matrix[current_row][current_column] > matrix[current_row + 1][current_column + 1]) {
                      is_sorted = FALSE;
                   }
               }
           }
           for (j = 0; j < DIMENSION - 1 && is_sorted == TRUE; j++) {
               for (current_row = 0, current_column = j; current_column < DIMENSION - 1 && is_sorted == TRUE; current_row++, current_column++) {
                   if (matrix[current_row][current_column] > matrix[current_row + 1][current_column + 1]) {
                      is_sorted = FALSE;
                   }
               }
           }

           /****************************************************************************************************
           ** Let workers know that the matrix is sorted or not yet sorted                                    **
           ****************************************************************************************************/
           for (destination = 1; destination < NUMBER_OF_PROCESSES; destination++) {
               MPI_Send(&is_sorted, 1, MPI_UNSIGNED_SHORT, destination, IS_SORTED_TAG, MPI_COMM_WORLD);
           }
           #ifdef DEBUG
               if (is_sorted) {
                  printf("\n   Matrix is sorted. Outputting results...\n\n");
               }
               else {
                  printf("\n   Matrix is NOT sorted.\n\n");
               }
           #endif

       } while (is_sorted == FALSE);

       program_end = time(NULL);
    }
    /****************************************************************************************************
    ** WORKERS                                                                                         **
    ****************************************************************************************************/
    else {
       do {

           for (i = 0; i < NUMBER_OF_ROWS_PER_PROCESS; i++) {

               /****************************************************************************************************
               ** If process ID is even, sort rows in ascending order. Otherwise, sort rows in descending order.  **
               ****************************************************************************************************/
               MPI_Recv(&numbers[0], DIMENSION, MPI_INT, MASTER, ROW_TAG, MPI_COMM_WORLD, &status);
               if (PROCESS_ID % 2 == 0) {
                  osort(numbers, DIMENSION);
                  esort(numbers, DIMENSION);
               }
               else {
                  orsort(numbers, DIMENSION);
                  ersort(numbers, DIMENSION);
               }
               MPI_Send(&numbers[0], DIMENSION, MPI_INT, MASTER, ROW_TAG, MPI_COMM_WORLD);

           }

           /****************************************************************************************************
           ** Sort columns in ascending order                                                                 **
           ****************************************************************************************************/
           for (j = 0; j < NUMBER_OF_ROWS_PER_PROCESS; j++) {
               MPI_Recv(&numbers[0], DIMENSION, MPI_INT, MASTER, COLUMN_TAG, MPI_COMM_WORLD, &status);
               osort(numbers, DIMENSION);
               esort(numbers, DIMENSION);
               MPI_Send(&numbers[0], DIMENSION, MPI_INT, MASTER, COLUMN_TAG, MPI_COMM_WORLD);
           }

           MPI_Recv(&is_sorted, 1, MPI_UNSIGNED_SHORT, MASTER, IS_SORTED_TAG, MPI_COMM_WORLD, &status);

       } while (is_sorted == FALSE);
    }

    /****************************************************************************************************
    ** Print results                                                                                   **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       #ifdef DEBUG
           int current_column,
               current_row;

           printf("======================================================================\n");
           printf("== Diagonals                                                        ==\n");
           printf("======================================================================\n\n");
           printf("Below main diagonal in sorted matrix:\n\n");
           for (i = 0; i < DIMENSION; i++) {
               for (current_row = i, current_column = 0; current_row < DIMENSION; current_row++, current_column++) {
                   printf("%10d\t", matrix[current_row][current_column]);
               }
               printf("\n");
           }
           printf("\nAbove main diagonal in sorted matrix:\n\n");
           for (j = 0; j < DIMENSION; j++) {
               for (current_row = 0, current_column = j; current_column < DIMENSION; current_row++, current_column++) {
                   printf("%10d\t", matrix[current_row][current_column]);
               }
               printf("\n");
           }
           printf("\n");
           printf("======================================================================\n");
           printf("== Sorted matrix                                                    ==\n");
           printf("======================================================================\n\n");
           print_matrix(&matrix[0][0], DIMENSION, DIMENSION);
           printf("\n");
           printf("======================================================================\n");
           printf("== Variables                                                        ==\n");
           printf("======================================================================\n\n");
           printf("is_sorted:\t%d\t  counter: %10d\n\n", is_sorted, counter);
       #endif

       printf("======================================================================\n");
       printf("== Summary                                                          ==\n");
       printf("======================================================================\n\n");
       printf("Total number of processes:         %10d\n\n", NUMBER_OF_PROCESSES);
       printf("Length and width of square matrix: %10d\n",  DIMENSION);
       printf("Number of elements in matrix:      %10d\n\n", DIMENSION * DIMENSION);
       printf("Total runtime:                        %10.2f seconds\n\n", difftime(program_end, program_start));
    }

    /***************************************************************************************************/

    free(numbers);

    MPI_Finalize();

    return 0;

}


void initialize(int* matrix, int height, int width) {
     int i, j;
     for (i = 0; i < height; i++) {
         for (j = 0; j < width; j++) {
             #ifdef DEBUG
                 *matrix++ = rand() % 10 + 1;
             #else
                 *matrix++ = rand();
             #endif
         }
     }
}

void esort(int *row, int length) {
     int i, temp;
     for (i = 0; i < length - 1; i += 2) {
         if (row[i+1] < row[i]) {
            temp = row[i+1];
            row[i+1] = row[i];
            row[i] = temp;
         }
     }
}

void ersort(int *row, int length) {
     int i, temp;
     for (i = 0; i < length - 1; i += 2) {
         if (row[i+1] > row[i]) {
            temp = row[i+1];
            row[i+1] = row[i];
            row[i] = temp;
         }
     }
}

void osort(int *row, int length) {
     int i, temp;
     for (i = 1; i < length - 1; i += 2) {
         if (row[i+1] < row[i]) {
            temp = row[i+1];
            row[i+1] = row[i];
            row[i] = temp;
         }
     }
}

void orsort(int *row, int length) {
     int i, temp;
     for (i = 1; i < length - 1; i += 2) {
         if (row[i+1] > row[i]) {
            temp = row[i+1];
            row[i+1] = row[i];
            row[i] = temp;
         }
     }
}

void print_matrix(int* matrix, int height, int width) {
     int i, j;
     for (i = 0; i < height; i++) {
         for (j = 0; j < width; j++) {
             printf("%10d\t", *matrix++);
         }
         printf("\n");
     }
}

double** create_matrix(int height, int width) {

     double** matrix = (double**) calloc(width, sizeof(double*));

     if (matrix == NULL) {
        return NULL;
     }
     else {
        int i;
        for (i = 0; i < height; i++) {
            matrix[i] = (double*) calloc(width, sizeof(double));

            if (matrix[i] == NULL) {
               return NULL;
            }
        }
     }

     return matrix;

}

void destroy_matrix(double** matrix, int height) {

     int i;

     for (i = 0; i < height; i++) {
         free(matrix[i]);
     }

     free(matrix);

}