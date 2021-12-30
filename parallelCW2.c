#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define MPI_CHECK(fn)                   \
    {                                   \
        int errcode;                    \
        errcode = (fn);                 \
        if (errcode != MPI_SUCCESS)     \
            handle_error(errcode, #fn); \
    }

double **readGrid(char *file_name, int dimension, int rank);

static void handle_error(int errcode, char *str)
{
    char msg[MPI_MAX_ERROR_STRING];
    int resultlen;
    MPI_Error_string(errcode, msg, &resultlen);
    fprintf(stderr, "%s: %s\n", str, msg);
    MPI_Abort(MPI_COMM_WORLD, 1);
}

int main(int argc, char **argv)
{
    int size, rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /* the body of the code goes here */

    // Handle Arguements
    int opt;
    int dimension = -1;    // dimension is required so initialise to -1 (invalid value) to check later
    int precision = 0.001; // set default for precision

    char *file_name = NULL;

    while ((opt = getopt(argc, argv, "d:p:f:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            dimension = atoi(optarg);
            break;
        case 'f':
            file_name = optarg;
            break;
        default:
            break;
        }
    }

    // Check mandatory arguements:
    if (dimension == -1)
    {
        printf("-d is mandatory!\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if (file_name == NULL)
    {
        file_name = malloc(sizeof(char) * 50);

        sprintf(file_name, "grids/grid_%d.bin", dimension);
    }
    // Setup Grid
    if (rank == 0)
    {
        double **grid = readGrid(file_name, dimension, rank);
    }
    printf("finished!\n");
    MPI_Finalize();

    return 0;
}

double **readGrid(char *file_name, int dimension, int rank)
{
    double **read = (double **)malloc(sizeof(double *) * dimension);

    for (int l = 0; l < dimension; l++)
    {
        read[l] = (double *)malloc(sizeof(double) * dimension);
    }

    MPI_File handle;
    MPI_Status status;
    MPI_CHECK(MPI_File_open(MPI_COMM_SELF, file_name, MPI_MODE_RDONLY, MPI_INFO_NULL, &handle));

    for (int x = 0; x < dimension; x++)
    {
        MPI_File_read_at(handle, x * dimension * sizeof(double), read[x], dimension, MPI_DOUBLE, &status);
        for (int y = 0; y < dimension; y++)
        {
            printf("%f ", read[x][y]);
        }
        printf("\n");
    }
    if (MPI_File_close(&handle) != MPI_SUCCESS)
    {
        printf("[MPI process %d] Failure in closing the file.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    return read;
}
