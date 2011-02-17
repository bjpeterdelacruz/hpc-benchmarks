/*!
 *
 *  \file    shearsort.c
 *  \brief   Implementation of the shear sort algorithm
 *
 *  \author  BJ Peter DeLaCruz
 *
 *  \date    June 4, 2010
 *
 *  \version 1.0
 *
 *  \details \par How this program works:
 *           Given an N by N matrix, where N % 8 = 0, the program will first divide the rows evenly
 *           between N processors. The processors with even IDs will sort their rows in ascending
 *           order; the ones with odd IDs will sort theirs in descending order. After sorting the
 *           rows, the columns are divided evenly between the same N processors and are sorted in
 *           ascending order. This process is repeated log(N) times. When the program finishes, the
 *           matrix is sorted in "snake-like" order (diagonally, in ascending order). The time
 *           complexity of this program is O(n lg n).
 *
 *           \par Reference:
 *           <A HREF="http://www.inf.fh-flensburg.de/lang/algorithmen/sortieren/twodim/shear/shearsorten.htm">Shearsort Algorithm</A>
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
 *  Uses bubble sort to sort array in ascending order.
 *
 *  \param row 1-dimensional subarray in \b matrix
 *  \param length Size of subarray
 *
 */
void sort(int* row, int length);

/*!
 *
 *  \par Description:
 *  Uses bubble sort to sort array in descending order.
 *
 *  \param row 1-dimensional subarray in \b matrix
 *  \param length Size of subarray
 *
 */
void rsort(int* row, int length);

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
 *  Not called in program as matrix is currently being allocated on the stack.
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
 *  Not called in program as matrix is currently being allocated on the stack.
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
    /* Dimension of square matrix (i.e. number of rows = number of columns) */
    int DIMENSION;
    /* Used for error handling */
    int error_code;
    /* Total number of processes used in this program */
    int NUMBER_OF_PROCESSES;
    /* Current process */
    int PROCESS_ID;
    /* Main loop counter. Counts from 0 to \f$\log_2 N\f$, where N = DIMENSION. */
    int program_counter;
    /* Message identifier for sending/receiving rows in a matrix */
    int ROW_TAG = 1;

    /* Used to start timing shearsort algorithm */
    time_t start;
    /* Used to end timing shearsort algorithm */
    time_t end;

    /* Derived datatype for sending a column in a matrix to a process */
    MPI_Datatype column_type;
    /* Used in MPI_Recv */
    MPI_Status status;

    /***************************************************************************************************/

    if (argc != 2) {
       printf("Usage: ./shearsort [dimension of square matrix]\nPlease try again.\n");
       exit(1);
    }

    if ((DIMENSION = atoi(argv[1])) <= 0) {
       printf("Error: Invalid argument for dimension of square matrix. Please try again.\n");
       exit(1);
    }

    /***************************************************************************************************/

    error_code = MPI_Init(&argc, &argv);
    error_code = MPI_Comm_size(MPI_COMM_WORLD, &NUMBER_OF_PROCESSES);
    error_code = MPI_Comm_rank(MPI_COMM_WORLD, &PROCESS_ID);

    if (error_code != 0) {
       printf("Error initializing MPI and obtaining task information.\n");
       MPI_Finalize();
       exit(1);
    }

    if (DIMENSION != NUMBER_OF_PROCESSES) {
       printf("Dimension of square matrix = %d\tNumber of processes = %d\n", DIMENSION, NUMBER_OF_PROCESSES);
       printf("Number of processes does NOT equal dimension of square matrix. Please try again.\n");
       MPI_Finalize();
       exit(1);
    }

    srand(time(NULL));

    /***************************************************************************************************/

    int matrix[DIMENSION][DIMENSION];
    int numbers[DIMENSION];

    /***** Create a derived datatype that contains all of the elements in a column *****/
    error_code = MPI_Type_vector(DIMENSION, 1, DIMENSION, MPI_INT, &column_type);
    error_code = MPI_Type_commit(&column_type);

    if (error_code != 0) {
       printf("Error creating derived vector datatype.\n");
       MPI_Finalize();
       exit(1);
    }

    /****************************************************************************************************
    ** MASTER                                                                                          **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       int current_column,
           current_row,
           i,
           destination,    /* recipient of data */
           source;         /* sender of data    */

       unsigned char is_sorted; /* TRUE if matrix is sorted diagonally in ascending order */

       printf("\n");
       printf("Initializing matrix...\n");
       printf("\n");

       initialize(&matrix[0][0], DIMENSION, DIMENSION);

       #ifdef DEBUG
           /*************************************************************************************
           ** Note: Be sure that DIMENSION is not too large so that the matrix will be small   **
           **       enough to be viewable. --BPD                                               **
           *************************************************************************************/
           printf("======================================================================\n");
           printf("== Initial matrix                                                   ==\n");
           printf("======================================================================\n\n");
           print_matrix(&matrix[0][0], DIMENSION, DIMENSION);
           printf("\n");
       #endif

       printf("Sorting matrix...\n");
       printf("\n");

       start = time(NULL);

       for (program_counter = 0; program_counter < (int) ceil(log((double) DIMENSION) / log(2.0)); program_counter++) {
           printf("   Pass %d of %d...\n\n", program_counter + 1, (int) ceil((log((double) DIMENSION) / log(2.0))));

           /****************************************************************************************************
           ** Send unsorted rows to workers, then receive sorted rows from them                               **
           ****************************************************************************************************/
           printf("      Sending rows to workers... ");
           for (destination = 1; destination < NUMBER_OF_PROCESSES; destination++) {
               MPI_Send(&matrix[destination][0], DIMENSION, MPI_INT, destination, ROW_TAG, MPI_COMM_WORLD);
           }
           for (source = 1; source < NUMBER_OF_PROCESSES; source++) {
               MPI_Recv(&matrix[source][0], DIMENSION, MPI_INT, source, ROW_TAG, MPI_COMM_WORLD, &status);
           }

           /***** Master sorts its row *****/
           sort(&matrix[0][0], DIMENSION);

           printf("Received sorted rows from workers.\n\n");

           /****************************************************************************************************
           ** Send unsorted columns to workers, then receive sorted columns from them                         **
           ****************************************************************************************************/
           printf("      Sending columns to workers... ");
           for (destination = 1; destination < NUMBER_OF_PROCESSES; destination++) {
               MPI_Send(&matrix[0][destination], 1, column_type, destination, COLUMN_TAG, MPI_COMM_WORLD);
           }
           for (source = 1; source < NUMBER_OF_PROCESSES; source++) {
               MPI_Recv(&numbers, DIMENSION, MPI_INT, source, COLUMN_TAG, MPI_COMM_WORLD, &status);
               for (i = 0; i < DIMENSION; i++) {
                   matrix[i][source] = numbers[i];
               }
           }
           printf("Received sorted columns from workers.\n\n");

           /***** Master sorts its column *****/
           for (i = 0; i < DIMENSION; i++) {
               numbers[i] = matrix[i][0];
           }
           sort(numbers, DIMENSION);
           for (i = 0; i < DIMENSION; i++) {
               matrix[i][0] = numbers[i];
           }
       }

       /****************************************************************************************************
       ** For the last time, send unsorted rows to workers and then receive sorted rows from them         **
       ****************************************************************************************************/
       printf("   Sending rows to workers... ");
       for (destination = 1; destination < NUMBER_OF_PROCESSES; destination++) {
           MPI_Send(&matrix[destination][0], DIMENSION, MPI_INT, destination, ROW_TAG, MPI_COMM_WORLD);
       }
       for (source = 1; source < NUMBER_OF_PROCESSES; source++) {
           MPI_Recv(&matrix[source][0], DIMENSION, MPI_INT, source, ROW_TAG, MPI_COMM_WORLD, &status);
       }
       printf("Received sorted rows from workers.\n\n");

       /****************************************************************************************************
       ** Check if diagonals below and above the main diagonal are sorted in ascending order              **
       ****************************************************************************************************/
       printf("Checking if matrix is sorted... ");
       for (i = 0, is_sorted = TRUE; i < DIMENSION - 1 && is_sorted == TRUE; i++) {
           for (current_row = i, current_column = 0; current_row < DIMENSION - 1 && is_sorted == TRUE; current_row++, current_column++) {
               if (matrix[current_row][current_column] > matrix[current_row + 1][current_column + 1]) {
                  is_sorted = FALSE;
               }
           }
       }
       for (i = 0; i < DIMENSION - 1 && is_sorted == TRUE; i++) {
           for (current_row = 0, current_column = i; current_column < DIMENSION - 1 && is_sorted == TRUE; current_row++, current_column++) {
               if (matrix[current_row][current_column] > matrix[current_row + 1][current_column + 1]) {
                  is_sorted = FALSE;
               }
           }
       }

       if (is_sorted == TRUE) {
          printf("Matrix is sorted.\n\n");
          printf("Printing results...\n\n");
       }
       else {
          printf("Matrix is NOT sorted. Aborting...\n\n");
          MPI_Finalize();
          exit(1);
       }

       end = time(NULL);
    }
    /****************************************************************************************************
    ** WORKERS                                                                                         **
    ****************************************************************************************************/
    else {

       /****************************************************************************************************
       ** Get rows from Master, sort them, and then return them back to Master                            **
       ****************************************************************************************************/
       for (program_counter = 0; program_counter < (int) ceil((log((double) DIMENSION) / log(2.0))); program_counter++) {
           MPI_Recv(&numbers, DIMENSION, MPI_INT, MASTER, ROW_TAG, MPI_COMM_WORLD, &status);
           /***** Sort even rows in ascending order and odd rows in descending order *****/
           if (PROCESS_ID % 2 == 0) {
              sort(numbers, DIMENSION);
           }
           else {
              rsort(numbers, DIMENSION);
           }
           MPI_Send(&numbers, DIMENSION, MPI_INT, MASTER, ROW_TAG, MPI_COMM_WORLD);

           /****************************************************************************************************
           ** Get columns from Master, sort them, and then return them back to Master                         **
           ****************************************************************************************************/
           MPI_Recv(&numbers, DIMENSION, MPI_INT, MASTER, COLUMN_TAG, MPI_COMM_WORLD, &status);
           sort(numbers, DIMENSION);
           MPI_Send(&numbers, DIMENSION, MPI_INT, MASTER, COLUMN_TAG, MPI_COMM_WORLD);
       }

       /****************************************************************************************************
       ** For the last time, get rows from Master, sort them, and then return them back to Master         **
       ****************************************************************************************************/
       MPI_Recv(&numbers, DIMENSION, MPI_INT, MASTER, ROW_TAG, MPI_COMM_WORLD, &status);
       sort(numbers, DIMENSION);
       MPI_Send(&numbers, DIMENSION, MPI_INT, MASTER, ROW_TAG, MPI_COMM_WORLD);

    }

    /****************************************************************************************************
    ** Print results                                                                                   **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       #ifdef DEBUG
           int current_column,
               current_row,
               i;

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
           for (i = 0; i < DIMENSION; i++) {
               for (current_row = 0, current_column = i; current_column < DIMENSION; current_row++, current_column++) {
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
       #endif

       printf("======================================================================\n");
       printf("== Summary                                                          ==\n");
       printf("======================================================================\n\n");
       printf("Total number of processes:    %10d\n\n", NUMBER_OF_PROCESSES);
       printf("Dimension of square matrix:   %10d\n",  DIMENSION);
       printf("Number of elements in matrix: %10d\n\n", DIMENSION * DIMENSION);
       printf("Total runtime:                   %10.2f seconds\n\n", difftime(end, start));
    }

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

void sort(int *row, int length) {
     int i, j, temp;
     for (i = 0; i < length; i++) {
         for (j = length - 1; j >= 0; j--) {
             if (row[j-1] > row[j]) {
                temp = row[j-1];
                row[j-1] = row[j];
                row[j] = temp;
             }
         }
     }
}

void rsort(int *row, int length) {
     int i, j, temp;
     for (i = 0; i < length; i++) {
         for (j = length - 1; j >= 0; j--) {
             if (row[j-1] < row[j]) {
                temp = row[j-1];
                row[j-1] = row[j];
                row[j] = temp;
             }
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