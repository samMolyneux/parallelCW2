#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int size, rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /* the body of the code goes here */

    // Handle Arguements
    int opt;
    int dimension = -1; // dimension is required so initialise to -1 (invalid value) to check later
    int precision = 0.001; // set default for precision

    while ((opt = getopt(argc, argv, "d:p:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            dimension = atoi(optarg);
            break;

        default:
            break;
        }
    }

    // Check mandatory arguements:
    if (dimension == -1) {
       printf("-d is mandatory!\n");
       exit(1); //some mpi function to improve this
    }

    // Setup Grid


    // double **read = (double **)malloc(sizeof(double *) * dimension);
   
    // for (int l = 0; l < dimension; l++)
    // {
    //     read[l] = (double *)malloc(sizeof(double) * dimension);
    // }


    // char *file_name = malloc(sizeof(char) * 50);

    // sprintf(file_name, "grids\\grid_%d.bin", dimension);

    // FILE *in_file;

    // in_file = fopen(file_name, "rb"); // r for read, b for binary

    // fread(read, sizeof(read), dimension*dimension, in_file);

    // for (int x = 0; x < dimension; x++)
    // {
    //     for (int y = 0; y < dimension; y++)
    //     {
    //         printf("%f ", read[x][y]);
    //     }
    //     printf("\n");
    // }

    printf("finished!\n");
    MPI_Finalize();
    return 0;
}