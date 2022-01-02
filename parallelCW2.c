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

double **readGrid(char *file_name, int dimension, int rank, int size);

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

    double **grid = readGrid(file_name, dimension, rank, size);

    printf("finished!\n");
    MPI_Finalize();

    return 0;
}

double **readGrid(char *file_name, int dimension, int rank, int size)
{
    int remainder = dimension % size;
    int allocStart;
    int allocRows;

    if (rank < remainder)
    {

        allocRows = dimension / size + 1;
        //rank is added here to account for the processes of lower rank that each have one extra row
        allocStart = rank * allocRows;
    }
    else
    {
        allocRows = dimension / size;
        //remainder is added to account for the total number of extra rows that have been allocated
        allocStart = rank * allocRows + remainder;
    }

    double **read = (double **)malloc(sizeof(double *) * allocRows);

    for (int l = 0; l < dimension; l++)
    {
        read[l] = (double *)malloc(sizeof(double) * dimension);
    }

    printf("Process number: %d \n Allocated Rows: %d\n Allocation Start: %d \n\n", rank, allocRows, allocStart);
    // MPI_File handle;
    // MPI_Status status;
    // MPI_CHECK(MPI_File_open(MPI_COMM_WORLD, file_name, MPI_MODE_RDONLY, MPI_INFO_NULL, &handle));

    // MPI_CHECK(MPI_File_seek(handle, allocStart * dimension * sizeof(double), MPI_SEEK_SET));

    // for (int x = 0; x < allocRows; x++)
    // {
    //     MPI_CHECK(MPI_File_read(handle, read[x], dimension, MPI_DOUBLE, &status));
    //     for (int y = 0; y < dimension; y++)
    //     {
    //         printf("%f ", read[x][y]);
    //     }
    //     printf("\n");
    // }
    // MPI_CHECK(MPI_File_close(&handle));

    return read;
}
