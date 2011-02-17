/*!
 *
 *  \file    mm.c
 *  \brief   Implementation of a parallel matrix multiplication algorithm
 *
 *  \author  BJ Peter DeLaCruz
 *
 *  \date    June 7, 2010
 *
 *  \version 1.0
 *
 *  \details \par How this program works:
 *           Given an M x N matrix and an N x P matrix, the program will perform matrix
 *           multiplication. Each process will be given M / Q rows from the first matrix and a copy
 *           of the entire N x P matrix. Each process will then perform matrix multiplication
 *           for each element in its rows. Finally, the results will be sent to the master, which
 *           will combine all of them together.
 *
 *           \par Example:
 *           2 processes and two 2x2 matrices.\n
 *           Process 0's rowA is first row in matrixA.\n
 *           Process 1's rowA is second row in matrixA.
 *           \arg Process 0: results[0] = (rowA[0] * matrixB[0][0]) + (rowA[1] * matrixB[1][0])
 *           \arg Process 0: results[1] = (rowA[0] * matrixB[0][1]) + (rowA[1] * matrixB[1][1])
 *           \arg Process 1: results[0] = (rowA[0] * matrixB[0][0]) + (rowA[1] * matrixB[1][0])
 *           \arg Process 1: results[1] = (rowA[0] * matrixB[0][1]) + (rowA[1] * matrixB[1][1])
 *
 *           \note
 *           \arg M mod Q must equal 0, where Q is the number of processes.
 *           \arg This version of matrix multiplication does not use a ring topology.
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
 *  Assigns random values to elements in matrix.
 *
 *  \param matrix Empty 2-dimensional array
 *  \param width Number of columns in matrix
 *  \param height Number of rows in matrix
 *
 */
void initialize(double* matrix, int width, int height);

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
void print_matrix(double* matrix, int width, int height);

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
 *  \param argv[1] Number of rows in matrix A
 *  \param argv[2] Number of columns in matrix A
 *  \param argv[3] Number of rows in matrix B
 *  \param argv[4] Number of columns in matrix B
 */
int main(int argc, char** argv) {

    /* Holds a row that will be used in matrix C */
    double* results = NULL;
    /* Holds a row from matrix A */
    double* rowA = NULL;

    /* Number of rows in matrix A */
    int A_HEIGHT;
    /* Number of columns in matrix A */
    int A_WIDTH;
    /* Number of rows in matrix B */
    int B_HEIGHT;
    /* Number of columns in matrix B */
    int B_WIDTH;
    /* Used for error handling */
    int error_code;
    /* Total number of processes used in this program */
    int NUMBER_OF_PROCESSES;
    /* Current process */
    int PROCESS_ID;

    #ifndef SERIAL
        /* Message identifier for sending/receiving columns to/from processes */
        int COLUMN_TAG = 0;
        /* Main loop counter. Counts from 0 to N, where N = SIZE. */
        int program_counter;
        /* Message identifier for sending/receiving rows to/from processes */
        int ROW_TAG = 1;
        /* Number of rows per process */
        int SIZE;
    #endif

    /* Used to start timing matrix multiplication algorithm */
    time_t start;
    /* Used to end timing matrix multiplication algorithm */
    time_t end;

    /* Derived datatype for sending a column in a matrix to a process */
    MPI_Datatype column_type;
    /* Used in MPI_Recv */
    MPI_Status status;

    /***************************************************************************************************/

    if (argc != 5) {
       printf("Usage: ./mm ");
       printf("[number of rows in matrix A] [number of columns in matrix A] ");
       printf("[number of rows in matrix B] [number of columns in matrix B]\nPlease try again.\n");
       exit(1);
    }

    if ((A_HEIGHT = atoi(argv[1])) <= 0) {
       printf("Error: Invalid argument for number of rows in matrix A. Please try again.\n");
       exit(1);
    }

    if ((A_WIDTH = atoi(argv[2])) <= 0) {
       printf("Error: Invalid argument for number of columns in matrix A. Please try again.\n");
       exit(1);
    }

    if ((B_HEIGHT = atoi(argv[3])) <= 0) {
       printf("Error: Invalid argument for number of rows in matrix B. Please try again.\n");
       exit(1);
    }

    if ((B_WIDTH = atoi(argv[4])) <= 0) {
       printf("Error: Invalid argument for number of columns in matrix B. Please try again.\n");
       exit(1);
    }

    if (A_WIDTH != B_HEIGHT) {
       printf("Error: Column length of Matrix A does not equal row length of Matrix B.\n");
       exit(1);
    }

    /***************************************************************************************************/

    double matrixA[A_HEIGHT][A_WIDTH];
    double matrixB[B_HEIGHT][B_WIDTH];
    double matrixC[A_HEIGHT][B_WIDTH];

    error_code = MPI_Init(&argc, &argv);
    error_code = MPI_Comm_size(MPI_COMM_WORLD, &NUMBER_OF_PROCESSES);
    error_code = MPI_Comm_rank(MPI_COMM_WORLD, &PROCESS_ID);

    if (error_code != 0) {
       printf("Error encountered while initializing MPI and obtaining task information.\n");
       MPI_Finalize();
       exit(1);
    }

    if (A_HEIGHT % NUMBER_OF_PROCESSES != 0) {
       printf("Number of rows in matrix A = %d\tNumber of processes = %d\n", A_HEIGHT, NUMBER_OF_PROCESSES);
       printf("Number of processes does NOT divide number of rows in matrix A. Please try again.\n");
       printf("[For example: Number of rows in matrix A = 24. Number of processes = 8.]\n");
       MPI_Finalize();
       exit(1);
    }

    srand(time(NULL));

    /***************************************************************************************************/

    SIZE = A_HEIGHT / NUMBER_OF_PROCESSES;

    /****************************************************************************************************
    ** MASTER                                                                                          **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       int j, k;

       #ifndef SERIAL
           int current_row,
               destination,  /* process that receives data from master */
               source,       /* process that sent data to master       */
               previous_row;
       #else
           int i;
       #endif

       initialize(&matrixA[0][0], A_HEIGHT, A_WIDTH);
       initialize(&matrixB[0][0], B_HEIGHT, B_WIDTH);

       #ifdef DEBUG
           /*************************************************************************************
           ** Note: Be sure that the heights and widths are not too large so that the matrices **
           **       will be small enough to be viewable. --BPD                                 **
           *************************************************************************************/
           printf("\n");
           printf("======================================================================\n");
           printf("== Matrix A                                                         ==\n");
           printf("======================================================================\n\n");
           print_matrix(&matrixA[0][0], A_HEIGHT, A_WIDTH);
           printf("\n");
           printf("======================================================================\n");
           printf("== Matrix B                                                         ==\n");
           printf("======================================================================\n\n");
           print_matrix(&matrixB[0][0], B_HEIGHT, B_WIDTH);
           printf("\n");
       #endif

       start = time(NULL);

       #ifndef SERIAL
          /****************************************************************************************************
          ** Send rows in matrix A to workers and then get results from them                                 **
          ****************************************************************************************************/
          for (program_counter = 0, current_row = 0; program_counter < SIZE; program_counter++) {
              /***** Master calculates its row *****/
              for (j = 0; j < B_WIDTH; j++) {
                  matrixC[current_row][j] = 0.0;
                  for (k = 0; k < A_WIDTH; k++) {
                      matrixC[current_row][j] += (matrixA[current_row][k] * matrixB[k][j]);
                  }
              }

              current_row++;
              previous_row = current_row;

              for (destination = 1; destination < NUMBER_OF_PROCESSES; destination++) {
                  MPI_Send(&matrixA[current_row++][0], A_WIDTH, MPI_DOUBLE, destination, ROW_TAG, MPI_COMM_WORLD);
              }

              /****************************************************************************************************
              ** Send matrix B to workers only once                                                              **
              ****************************************************************************************************/
              if (program_counter == 0) {
                 for (destination = 1; destination < NUMBER_OF_PROCESSES; destination++) {
                     MPI_Send(&matrixB[0][0], B_HEIGHT * B_WIDTH, MPI_DOUBLE, destination, COLUMN_TAG, MPI_COMM_WORLD);
                 }
              }

              for (source = 1; source < NUMBER_OF_PROCESSES; source++) {
                  MPI_Recv(&matrixC[previous_row++][0], A_WIDTH, MPI_DOUBLE, source, ROW_TAG, MPI_COMM_WORLD, &status);
              }
          }
       #else
          printf("======================================================================\n");
          printf("== Serial version                                                   ==\n");
          printf("======================================================================\n\n");
          for (i = 0; i < A_HEIGHT; i++) {
              for (j = 0; j < B_WIDTH; j++) {
                  matrixC[i][j] = 0.0;
                  for (k = 0; k < A_WIDTH; k++) {
                      matrixC[i][j] += (matrixA[i][k] * matrixB[k][j]);
                  }
              }
          }
          #ifdef DEBUG
             print_matrix(&matrixC[0][0], A_HEIGHT, B_WIDTH);
             printf("\n");
          #endif
       #endif

       end = time(NULL);
    }
    /****************************************************************************************************
    ** WORKERS                                                                                         **
    ****************************************************************************************************/
    else {
       #ifndef SERIAL
          int i, j;

          results = (double*) calloc(B_WIDTH, sizeof(double));

          if (results == NULL) {
             printf("Memory allocation failed for results array! ");
             printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
             MPI_Finalize();
             exit(1);
          }

          rowA = (double*) calloc(A_WIDTH, sizeof(double));

          if (rowA == NULL) {
             printf("Memory allocation failed for rowA array! ");
             printf("Unable to allocate memory on process %d.\nAborting program...\n", PROCESS_ID);
             MPI_Finalize();
             exit(1);
          }

          /****************************************************************************************************
          ** Get rows in matrix A and everything in matrix B from Master                                     **
          ****************************************************************************************************/
          for (program_counter = 0; program_counter < SIZE; program_counter++) {

              MPI_Recv(&rowA[0], A_WIDTH, MPI_DOUBLE, MASTER, ROW_TAG, MPI_COMM_WORLD, &status);

              if (program_counter == 0) {
                 MPI_Recv(&matrixB[0][0], B_HEIGHT * B_WIDTH, MPI_DOUBLE, MASTER, COLUMN_TAG, MPI_COMM_WORLD, &status);
              }

              /***** Perform matrix multiplication, store in results, and then send results to Master *****/
              for (i = 0; i < B_WIDTH; i++) {
                  results[i] = 0.0;
                  for (j = 0; j < A_WIDTH; j++) {
                      results[i] += (rowA[j] * matrixB[j][i]);
                  }
              }

              MPI_Send(&results[0], A_WIDTH, MPI_DOUBLE, MASTER, ROW_TAG, MPI_COMM_WORLD);
          }
       #endif
    }

    MPI_Barrier(MPI_COMM_WORLD);

    /****************************************************************************************************
    ** Print results                                                                                   **
    ****************************************************************************************************/
    if (PROCESS_ID == MASTER) {
       #ifdef DEBUG
           printf("======================================================================\n");
           printf("== Results                                                          ==\n");
           printf("======================================================================\n\n");
           print_matrix(&matrixC[0][0], A_HEIGHT, B_WIDTH);
           printf("\n");
       #endif

       printf("======================================================================\n");
       printf("== Summary                                                          ==\n");
       printf("======================================================================\n\n");
       printf("Total number of processes:                  %10d\n\n", NUMBER_OF_PROCESSES);
       printf("Matrix A\n");
       printf("   Number of rows:                          %10d\n", A_HEIGHT);
       printf("   Number of columns:                       %10d\n", A_WIDTH);
       printf("   Number of elements in matrix A\n");
       printf("      (number of rows * number of columns): %10d\n\n", A_HEIGHT * A_WIDTH);
       printf("Matrix B\n");
       printf("   Number of rows:                          %10d\n", B_HEIGHT);
       printf("   Number of columns:                       %10d\n", B_WIDTH);
       printf("   Number of elements in matrix B:          %10d\n\n", B_HEIGHT * B_WIDTH);
       printf("Matrix C (results)\n");
       printf("   Number of rows:                          %10d\n", A_HEIGHT);
       printf("   Number of columns:                       %10d\n", B_WIDTH);
       printf("   Number of elements in matrix C:          %10d\n\n", A_HEIGHT * B_WIDTH);
       printf("Total runtime:                              %13.2f seconds\n\n", difftime(end, start));
    }

    /***************************************************************************************************/

    if (PROCESS_ID != MASTER) {
       free(rowA);
       free(results);
    }

    MPI_Finalize();

    return 0;

}

void initialize(double* matrix, int height, int width) {
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

void print_matrix(double* matrix, int height, int width) {
     int i, j;
     for (i = 0; i < height; i++) {
         for (j = 0; j < width; j++) {
             printf("%10.0f\t", *matrix++);
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