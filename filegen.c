/*!
 *
 *  \file    filegen.c
 *  \brief   Creates a file with a lot of numbers in it.
 *
 *  \author  BJ Peter DeLaCruz
 *
 *  \date    July 15, 2010
 *
 *  \version 1.0
 *
 *  \details \note
 *           The \b fileio and \b fileio_block programs use the file that is created by this program.
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*!
 *  \param argv[1] Number of characters to write to file
 */
int main(int argc, char** argv) {

    FILE* fp;
    char filename[128];
    char buffer[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
    int i;
    unsigned long SIZE;

    if (argc != 2) {
       printf("Usage: ./filegen [number of characters]\nPlease try again.\n");
       exit(1);
    }

    if ((SIZE = atol(argv[1])) <= 0) {
       printf("Error: Invalid argument for number of characters. Please try again.\n");
       exit(1);
    }

    srand(time(0));

    sprintf(filename, "unsorted.txt");
    fp = fopen(filename, "w");
    for (i = 0; i < SIZE; i++) {
        fputc(buffer[rand() % 9], fp);
    }
    fclose(fp);

    return 0;

}