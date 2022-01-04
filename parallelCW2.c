#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

#define TOP_EDGE_TAG 0
#define BOTTOM_EDGE_TAG 1
#define MPI_CHECK(fn)                   \
    {                                   \
        int errcode;                    \
        errcode = (fn);                 \
        if (errcode != MPI_SUCCESS)     \
            handle_error(errcode, #fn); \
    }

double **readGrid(char *file_name, int dimension, int allocStart, int allocRows);
double relaxCell(double **grid, int row, int column);
double **relaxGrid(double **in_grid, double **out_grid, int start_rows, int rows, int columns);
double *relaxEdge(double *in_edge, double *neighbours_above, double *neighbours_below, double *out_edge, int columns);
int checkComplete(double **grid, double **newGrid, int rows, int columns, double precision);
double **allocateGridMem(int rows, int columns);
double **copyEdges(double **from_grid, double **to_grid, int allocRows, int dimension);
int swapGrids(double **fromGrid, double **toGrid, int rows);

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

    // assign edges in the outgrid (these never change)
    copyEdges(in_grid, out_grid, allocRows, dimension);

    double *top_edge_neighbours = (double *)malloc(sizeof(double) * dimension);
    double *bottom_edge_neighbours = (double *)malloc(sizeof(double) * dimension);
    int global_cont = 1;
    MPI_Request send_top_edge_req;
    MPI_Request send_bottom_edge_req;

    MPI_Request rec_top_neighbours_req;
    MPI_Request rec_bottom_neighbours_req;
    while (global_cont)
    {
        //Send edges (Asynchronously)

        // if not the first process
        // send the top edge
        if (rank != 0)
        {
            MPI_CHECK(MPI_Isend(in_grid[0], dimension, MPI_DOUBLE, rank - 1, TOP_EDGE_TAG, MPI_COMM_WORLD, &send_top_edge_req));
        }
        // if not the final process
        // send bottom edge
        if (rank != size - 1)
        {
            MPI_CHECK(MPI_Isend(in_grid[allocRows - 1], dimension, MPI_DOUBLE, rank + 1, BOTTOM_EDGE_TAG, MPI_COMM_WORLD, &send_bottom_edge_req));
        }


        // Receive edge neighbours (Asycnchronously)

        // if not first process
        // receive top edge neighbours
        if (rank != 0)
        {
            MPI_CHECK(MPI_Irecv(top_edge_neighbours, dimension, MPI_DOUBLE, rank - 1, TOP_EDGE_TAG, MPI_COMM_WORLD, &rec_top_neighbours_req));
        }
        // if not final process
        // recieve bottom edge neighbours
        if (rank != size - 1)
        {
            MPI_CHECK(MPI_Irecv(bottom_edge_neighbours, dimension, MPI_DOUBLE, rank + 1, BOTTOM_EDGE_TAG, MPI_COMM_WORLD, &rec_bottom_neighbours_req));
        }
        //Calculate internal
        //calculate for ingrid[1] to ingrid[allocRows-1]
        relaxGrid(in_grid, out_grid, 1, allocRows - 2, dimension);

        //Await neighbours and Calculate edges

        // if not the first process
        // await neighbours and use them to calculate the top edge
        if (rank != 0)
        {
            MPI_Wait(&rec_top_neighbours_req, MPI_STATUS_IGNORE);
            relaxEdge(in_grid[0], top_edge_neighbours, in_grid[1], out_grid[0], dimension);
        }
        // if not the final process
        // await neighbours and use them to calculate the bottom edge
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
        int local_cont = checkComplete(in_grid, out_grid, allocRows, dimension, precision);

        MPI_CHECK(MPI_Allreduce(&local_cont, &global_cont, 1, MPI_INT, MPI_LAND, MPI_COMM_WORLD));

        // swap grids so that the old out_grid can be used as the input for the next iteration
        // **some explanation for why we don't need to worry about swapping and then using as buffer for send **
        if (global_cont)
        {
            swapGrids(in_grid, out_grid, allocRows);
        }
        
    }
    printf("finished!\n");
    MPI_Finalize();

    return 0;
}

/**
 * Function that is called once to copy the values at the 'edge' of one grid to another
 */
double **copyEdges(double **from_grid, double **to_grid, int allocRows, int dimension)
{
    for (int x = 0; x < dimension; x++)
    {
        to_grid[0][x] = from_grid[0][x];
        to_grid[allocRows - 1][x] = from_grid[allocRows - 1][x];
    }
    for (int y = 0; y < allocRows; y++)
    {
        to_grid[y][0] = from_grid[y][0];
        to_grid[y][dimension - 1] = from_grid[y][dimension - 1];
    }
    return to_grid;
}

/**
 * Function used to read the allocated rows from a binary file containing a grid of doubles in parallel
 */
double **readGrid(char *file_name, int dimension, int allocStart, int allocRows)
{

    double **read = allocateGridMem(allocRows, dimension);

    printf("Allocated Rows: %d\n Allocation Start: %d \n\n", allocRows, allocStart);
    MPI_File handle;
    MPI_Status status;
    MPI_CHECK(MPI_File_open(MPI_COMM_WORLD, file_name, MPI_MODE_RDONLY, MPI_INFO_NULL, &handle));

    MPI_CHECK(MPI_File_seek(handle, allocStart * dimension * sizeof(double), MPI_SEEK_SET));

    for (int x = 0; x < allocRows; x++)
    {
        MPI_CHECK(MPI_File_read(handle, read[x], dimension, MPI_DOUBLE, &status));
        for (int y = 0; y < dimension; y++)
        {
            printf("%f ", read[x][y]);
        }
        printf("\n");
    }
    MPI_CHECK(MPI_File_close(&handle));

    return read;
}

/**
 * Function that performs relaxation on all the values in a row, when provided with the row's above and below
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
