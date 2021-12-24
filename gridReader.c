
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// Program to read a grid from a file
int main(int argc, char **argv)
{
    int dimension = 10;
    double **read = (double **)malloc(sizeof(double *) * dimension);

    for (int l = 0; l < dimension; l++)
    {
        read[l] = (double *)malloc(sizeof(double) * dimension);
    }

    char *file_name = malloc(sizeof(char) * 50);

    sprintf(file_name, "grids\\grid_%d.bin", dimension);
    printf("File name: %s\n", file_name);
    FILE *in_file;

    in_file = fopen(file_name, "rb"); // r for read, b for binary

    for (int x = 0; x < dimension; x++)
    {
        fread(read[x], sizeof(double)*dimension, 1, in_file);
        for (int y = 0; y < dimension; y++)
        {
            printf("%f ", read[x][y]);
        }
        printf("\n");
    }
    fclose(in_file);
    return 0;
}