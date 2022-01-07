#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define SEND_TOP_EDGE_TAG 0
#define SEND_BOTTOM_EDGE_TAG 1

// used to check that MPI functions have executed correctly
#define MPI_CHECK(fn)                   \
    {                                   \
        int errcode;                    \
        errcode = (fn);                 \
        if (errcode != MPI_SUCCESS)     \
            handle_error(errcode, #fn); \
    }

double **readGrid(char *file_name, int dimension, int allocStart, int allocRows, int rank);
double relaxCell(double **grid, int row, int column);
double **relaxGrid(double **in_grid, double **out_grid, int start_rows, int rows, int columns);
double *relaxEdge(double *in_edge, double *neighbours_above, double *neighbours_below, double *out_edge, int columns);
int checkComplete(double **grid, double **newGrid, int rows, int columns, double precision);
double **allocateGridMem(int rows, int columns);
double **copyEdges(double **from_grid, double **to_grid, int allocRows, int dimension, int rank, int size);
int swapGrids(double **fromGrid, double **toGrid, int rows);
void writeGrid(double **grid, char *file_name, int dimension, int allocStart, int allocRows);

/**
 * Function that outputs to handle errors in MPI functions
 */
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
    int dimension = -1;   // dimension is required so initialise to -1 (invalid value) to check later
    int precision = 0.01; // set default for precision

    char *file_name = NULL;
    char *out_file_name;

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

    // setup the file name to write the final grid to
    out_file_name = malloc(sizeof(char) * 50);
    sprintf(out_file_name, "grids/grid_%d_out.bin", dimension);

    // Setup Grid
    // Calculate each proccess' allocation
    int remainder = dimension % size;
    int allocStart;
    int allocRows;

    // the total number of rows that cannot be distributed evenly is calculated as remainder
    // all process with rank less than the remainder are then handed on extra row such that all rows are assigned

    // for the group of processes that are assigned extra rows we can calculate the start location of the allocation simply,
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

    // set up the two grids, reading the process's allocated portion of the overall grid from file
    double **in_grid = readGrid(file_name, dimension, allocStart, allocRows, rank);
    double **out_grid = allocateGridMem(allocRows, dimension);

    // assign overall edges in the outgrid (these never change)
    copyEdges(in_grid, out_grid, allocRows, dimension, rank, size);

    // used to receive the row directly above or below the processes allocation from an adjacent process
    double *top_edge_neighbours = (double *)malloc(sizeof(double) * dimension);
    double *bottom_edge_neighbours = (double *)malloc(sizeof(double) * dimension);

    MPI_Request send_top_edge_req;
    MPI_Request send_bottom_edge_req;

    MPI_Request rec_top_neighbours_req;
    MPI_Request rec_bottom_neighbours_req;

    // the global finished, boolean that signifies whether all processes have finished
    int finished = 0;

    while (!finished)
    {
        //Send edges (Asynchronously)

        // if not the first process (first process has no neighbour above)
        // send the top edge of the allocation
        if (rank != 0)
        {
            MPI_CHECK(MPI_Isend(in_grid[0], dimension, MPI_DOUBLE, rank - 1, SEND_TOP_EDGE_TAG, MPI_COMM_WORLD, &send_top_edge_req));
        }
        // if not the final process (final process has no neighbour below)
        // send bottom edge of the allocation
        if (rank != size - 1)
        {
            MPI_CHECK(MPI_Isend(in_grid[allocRows - 1], dimension, MPI_DOUBLE, rank + 1, SEND_BOTTOM_EDGE_TAG, MPI_COMM_WORLD, &send_bottom_edge_req));
        }

        // Receive edge neighbours (Asycnchronously)

        // if not first process
        // receive top edge neighbours
        if (rank != 0)
        {
            MPI_CHECK(MPI_Irecv(top_edge_neighbours, dimension, MPI_DOUBLE, rank - 1, SEND_BOTTOM_EDGE_TAG, MPI_COMM_WORLD, &rec_top_neighbours_req));
        }
        // if not final process
        // recieve bottom edge neighbours
        if (rank != size - 1)
        {
            MPI_CHECK(MPI_Irecv(bottom_edge_neighbours, dimension, MPI_DOUBLE, rank + 1, SEND_TOP_EDGE_TAG, MPI_COMM_WORLD, &rec_bottom_neighbours_req));
        }
        // Calculate internal
        // i.e. ingrid[1] to ingrid[allocRows-1]
        relaxGrid(in_grid, out_grid, 1, allocRows - 2, dimension);

        // Await neighbours and Calculate edges

        // if not the first process (first process contains the overall top row which is constant)
        // await neighbours and use them to calculate the top edge of the allocation
        if (rank != 0)
        {
            MPI_Wait(&rec_top_neighbours_req, MPI_STATUS_IGNORE);
            relaxEdge(in_grid[0], top_edge_neighbours, in_grid[1], out_grid[0], dimension);
        }
        // if not the final process (final process contains the overall bottom row which is constant)
        // await neighbours and use them to calculate the bottom edge of the allocation
        if (rank != size - 1)
        {
            MPI_Wait(&rec_bottom_neighbours_req, MPI_STATUS_IGNORE);
            relaxEdge(in_grid[allocRows - 1], bottom_edge_neighbours, in_grid[allocRows - 2], out_grid[allocRows - 1], dimension);
        }

        // Await edge sends
        // if they were sent
        if (rank != 0)
        {
            MPI_Wait(&send_top_edge_req, MPI_STATUS_IGNORE);
        }
        if (rank != size - 1)
        {
            MPI_Wait(&send_bottom_edge_req, MPI_STATUS_IGNORE);
        }

        // check if this proccess's allocation is complete to the precision
        int local_finished = checkComplete(in_grid, out_grid, allocRows, dimension, precision);

        // perform a reduce to check whether or not all processes have finished with their allocation
        MPI_CHECK(MPI_Allreduce(&local_finished, &finished, 1, MPI_INT, MPI_LAND, MPI_COMM_WORLD));

        // if not all processes are finished, swap grids so that the old out_grid can be used as the input for the next iteration
        // Allreduce is blocking so acts as a barrier to synchronise processes,
        // this means all the proccesses will have finished their computation and we can swap the grids without race conditions
        if (!finished)
        {
            swapGrids(in_grid, out_grid, allocRows);
        }
    }
    printf("Process %d finishing\n", rank);
    writeGrid(out_grid, out_file_name, dimension, allocStart, allocRows);

    // clean up memory
    free(top_edge_neighbours);
    free(bottom_edge_neighbours);
    free(file_name);
    free(out_file_name);

    for(int i = 0; i < allocRows; i++){
        free(in_grid[i]);
        free(out_grid[i]);
    }
    free(in_grid);
    free(out_grid);

    MPI_Finalize();

    return 0;
}

/**
 * Function that is called once to copy the values at the 'edge' of one grid to another
 * this function considers the grid as a whole (across processes)
 */
double **copyEdges(double **from_grid, double **to_grid, int allocRows, int dimension, int rank, int size)
{
    // Copy the first and last value of each row
    for (int y = 0; y < allocRows; y++)
    {
        to_grid[y][0] = from_grid[y][0];
        to_grid[y][dimension - 1] = from_grid[y][dimension - 1];
    }
    // Copy the top row
    if (rank == 0)
    {
        for (int x = 0; x < dimension; x++)
        {
            to_grid[0][x] = from_grid[0][x];
        }
    }
    //Copy the bottom row
    if (rank == size - 1)
    {
        for (int x = 0; x < dimension; x++)
        {
            to_grid[allocRows - 1][x] = from_grid[allocRows - 1][x];
        }
    }
    return to_grid;
}

/**
 * Function used to read the allocated rows from a binary file containing a grid of doubles in parallel
 */
double **readGrid(char *file_name, int dimension, int allocStart, int allocRows, int rank)
{

    double **read = allocateGridMem(allocRows, dimension);

    srand(time(NULL));

    for (int x = 0; x < allocRows; x++)
    {
        for (int y = 0; y < dimension; y++)
        {
            read[x][y] = (double)(rand() % 2);
        }
    }

    // MPI_File handle;
    // MPI_Status status;
    // MPI_CHECK(MPI_File_open(MPI_COMM_WORLD, file_name, MPI_MODE_RDONLY, MPI_INFO_NULL, &handle));

    // MPI_CHECK(MPI_File_seek(handle, allocStart * dimension * sizeof(double), MPI_SEEK_SET));

    // for (int x = 0; x < allocRows; x++)
    // {
    //     MPI_CHECK(MPI_File_read(handle, read[x], dimension, MPI_DOUBLE, &status));
    //     for (int y = 0; y < dimension; y++)
    //     {
    //         //printf("%f ", read[x][y]);
    //     }
    //     //printf("\n");
    // }
    // MPI_CHECK(MPI_File_close(&handle));

    return read;
}

/**
 * Function used to write the allocated rows to a binary file at the correct position
 */
void writeGrid(double **grid, char *file_name, int dimension, int allocStart, int allocRows)
{

    // MPI_File handle;
    // MPI_Status status;
    // MPI_CHECK(MPI_File_open(MPI_COMM_WORLD, file_name, MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &handle));

    // MPI_CHECK(MPI_File_seek(handle, allocStart * dimension * sizeof(double), MPI_SEEK_SET));

    for (int x = 0; x < allocRows; x++)
    {
        //MPI_CHECK(MPI_File_write(handle, grid[x], dimension, MPI_DOUBLE, &status));
        for (int y = 0; y < dimension; y++)
        {
            printf("%f ", grid[x][y]);
        }
        printf("\n");
    }
    //MPI_CHECK(MPI_File_close(&handle));
}

/**
 * Function that performs relaxation on all the values in a row, when provided with the rows above and below
 * this is used for the edges of a process's allocation
 */
double *relaxEdge(double *in_edge, double *neighbours_above, double *neighbours_below, double *out_edge, int columns)
{
    for (int x = 1; x < columns - 1; x++)
    {
        double sum = 0;
        sum = sum + neighbours_above[x];
        sum = sum + neighbours_below[x];

        sum = sum + in_edge[x - 1];
        sum = sum + in_edge[x + 1];
        out_edge[x] = sum / 4;
    }
    return out_edge;
}

/**
 * Function that calculates the average of a cell's four neighbours
 */
double relaxCell(double **grid, int row, int column)
{
    double sum = 0;

    sum = sum + grid[row - 1][column];
    sum = sum + grid[row + 1][column];
    sum = sum + grid[row][column - 1];
    sum = sum + grid[row][column + 1];
    return (sum / 4);
}

/**
 * Function that performs relaxation on all values in a grid that are not on the edge for the given number of rows
 */
double **relaxGrid(double **in_grid, double **out_grid, int start_row, int rows, int columns)
{
    for (int s = start_row; s < start_row + rows; s++)
    {
        for (int t = 1; t < columns - 1; t++)
        {
            out_grid[s][t] = relaxCell(in_grid, s, t);
        }
    }
    return out_grid;
}

/**
 * Copies the values contained in one grid to another
 **/
int swapGrids(double **fromGrid, double **toGrid, int rows)
{
    int i;
    double *temp;
    for (i = 0; i < rows; i++)
    {
        temp = fromGrid[i];
        fromGrid[i] = toGrid[i];
        toGrid[i] = temp;
    }
    return 0;
}

/**
 * Checks whether two grids have any corresponding cells with a difference greater than the specified precision 
 * returns true if the grid is complete
 **/
int checkComplete(double **grid, double **newGrid, int rows, int columns, double precision)
{
    int i, j;
    for (i = 0; i < rows; i++)
    {
        for (j = 0; j < columns; j++)
        {
            if (fabs(newGrid[i][j] - grid[i][j]) > precision)
            {
                return 0;
            }
        }
    }
    return 1;
}

/**
 * Function that allocates memory for a grid of the specified dimensions
 */
double **allocateGridMem(int rows, int columns)
{
    double **read = (double **)malloc(sizeof(double *) * rows);

    for (int l = 0; l < rows; l++)
    {
        read[l] = (double *)malloc(sizeof(double) * columns);
    }
    return read;
}
