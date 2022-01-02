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

double **readGrid(char *file_name, int dimension, int allocStart, int allocRows);
double relaxCell(double **grid, int row, int column);

static void handle_error(int errcode, char *str)
{
    char msg[MPI_MAX_ERROR_STRING];
    int resultlen;
    MPI_Error_string(errcode, msg, &resultlen);
    fprintf(stderr, "%s: %s\n", str, msg);
    MPI_Abort(MPI_COMM_WORLD, 1);
}

double **allocateGridMem(int rows, int columns){
    double **read = (double **)malloc(sizeof(double *) * rows);

    for (int l = 0; l < rows; l++)
    {
        read[l] = (double *)malloc(sizeof(double) * columns);
    }
}

/**
 * Checks whether two grids have any corresponding cells with a difference greater than the specified precision 
 **/
int checkComplete(double **grid, double **newGrid, int rows, int columns, double precision){
    int i,j;
    for(i = 0; i < rows; i++){
        for(j = 0; j < columns; j++){
            if (fabs(newGrid[i][j] - grid[i][j]) > precision)
            {
                return 1;
            }
        }
    }
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
        case 'p':
            precision = atof(optarg);
            break;
        default:
            break;
        }
    }

    // Check arguements:
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

    // Calculate each proccess' allocation
    int remainder = dimension % size;
    int allocStart;
    int allocRows;

    // the total number of rows that cannot be distributed evenly is calculated as remainder
    // all process with rank less than the remainder are then handed on extra row such that all rows are assigned

    // for the group of processes that are assigned an extra rows we can calculate alloc start simply,
    // the later processes have a different(smaller) value for allocRows so we must add the remainder to account for the extra rows  
    if (rank < remainder)
    {
        allocRows = dimension / size + 1;
        allocStart = rank * allocRows;
    }
    else
    {
        allocRows = dimension / size;
        //remainder is added to account for the total number of extra rows that have been allocated
        allocStart = rank * allocRows + remainder;
    }


    double **in_grid = readGrid(file_name, dimension, allocStart, allocRows);
    double **out_grid = allocateGridMem(allocRows, dimension);

    double *top_edge_neighbours = allocateGridMem(1, dimension);
    double *bottom_edge_neighbours = allocateGridMem(1, dimension);
    int cont = 0;
    
    while(cont){
    //Send edges (Asynchronously)
        //send in_grid[0]
        //send in_grid[allocRows]
    
    //Check completeness
        //Check if local complete, if so set flag 
        //reduce and across local complete 
        //if global complete ouput

    //Receive edge neighbours (Asycnchronously)
        //if not rank 0
            //receive top_edge_neighbours
        //if not rank size -1
            //recieve bottom_edge_neighbours

    //Calculate internal
        //calculate for ingrid[1] to ingrid[allocRows-1]

    //Await edges neighbours
        //if they are expected

    //Calculate edges
        //if not rank 0
            //calculate for ingrid[0] using top_edge_neighbours
        //if not rank size - 1
            //calculate for ingrid[allocRows] using bottom_edge_neighbours

    //Await edges send
        //if they were sent
        //could use wait all?
    

    }
    printf("finished!\n");
    MPI_Finalize();

    return 0;
}



double **readGrid(char *file_name, int dimension, int allocStart, int allocRows)
{
    
    double **read = allocateGridMem(allocRows, dimension);

    //printf("Process number: %d \n Allocated Rows: %d\n Allocation Start: %d \n\n", rank, allocRows, allocStart);
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

/**
 * Function that calculates the average of a cell's four neighbours, or when on an edge, the available neighbours
 **/
double relaxCell(double **grid, int row, int column)
{
    double sum = 0;

    sum = sum + grid[row - 1][column];
    sum = sum + grid[row + 1][column];
    sum = sum + grid[row][column - 1];
    sum = sum + grid[row][column + 1];
    return (sum / 4);
}
