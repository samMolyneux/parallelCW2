#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <signal.h>
#include <time.h>

#define BILLION 1000000000L;

const int workerThreads = 4;
const int dimension = 10;
const double precision = 0.1;

pthread_barrier_t barrier1;
pthread_barrier_t barrier2;

struct threadArgs
{
    double **inGrid;
    double **outGrid;
    int startRow, endRow; //start row is inclusive, end row is exclusive
} threadArgs;

int populateGrid(double *grid[dimension]);
double relaxCell(double *grid[dimension], int row, int column);
int checkComplete(double *grid[dimension], double *newGrid[dimension]);
int printGrid(double *grid[dimension]);
int relaxGrid(double *inGrid[dimension], double *outGrid[dimension]);
int swapGrids(double *fromGrid[dimension], double *toGrid[dimension]);

int main()
{
    struct timespec start, stop;
    long long int accum;
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
    {
        perror("clock gettime");
        exit(EXIT_FAILURE);
    }
    //barriers are used to synchronise all worker threads + the main thread
    if (pthread_barrier_init(&barrier1, NULL, (unsigned int)workerThreads + 1) != 0)
    {
        perror("barrier 1 init");
        exit(EXIT_FAILURE);
    }
    if (pthread_barrier_init(&barrier2, NULL, (unsigned int)workerThreads + 1) != 0)
    {
        perror("barrier 2 init");
        exit(EXIT_FAILURE);
    }

    //Allocate memory to store the grids and then populate them with the desiered values
    double *grid[dimension];
    double *newGrid[dimension];

    int l;
    for (l = 0; l < dimension; l++)
    {
        grid[l] = (double *)malloc((unsigned)dimension * sizeof(double));
        newGrid[l] = (double *)malloc((unsigned)dimension * sizeof(double));
    }
    populateGrid(grid);
    populateGrid(newGrid);

   
    //Carry out the relaxation
    relaxGrid(grid, newGrid);

    printGrid(grid);
    //Cleanup memory
    int k;
    for (k = 0; k < dimension; k++)
    {
        free(grid[k]);
        free(newGrid[k]);
    }

    //Output timing information
    if (clock_gettime(CLOCK_MONOTONIC, &stop) == -1)
    {
        perror("clock gettime");
        exit(EXIT_FAILURE);
    }

    accum = (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) / BILLION;
    printf("%lld\n", accum);
    return 0;
}

/**
 * Function that calculates the average of a cell's four neighbours, or when on an edge, the available neighbours
 **/
double relaxCell(double *grid[dimension], int row, int column)
{
    double sum = 0;

    sum = sum + grid[row - 1][column];
    sum = sum + grid[row + 1][column];
    sum = sum + grid[row][column - 1];
    sum = sum + grid[row][column + 1];
    return (sum / 4);
}

/**
 * Fills the first row and left most columns of a 2d array with 1s
 **/
int populateGrid(double *grid[dimension])
{
    for (int i = 0; i < dimension; i++)
    {
        grid[i][0] = 1;
        grid[0][i] = 1;
    }
    return 0;
}

/**
 * Checks whether two grids have any corresponding cells with a difference greater than the specified precision 
 **/
int checkComplete(double *grid[dimension], double *newGrid[dimension])
{
    int i, j;
    for (i = 0; i < dimension; i++)
    {
        for (j = 0; j < dimension; j++)
        {
            if (fabs(newGrid[i][j] - grid[i][j]) > precision)
            {
                return 1;
            }
        }
    }
    return 0;
}

/**
 * Prints a grid out in a readable format (for testing)
 **/
int printGrid(double *grid[dimension])
{
    int k, l;
    for (k = 0; k < dimension; k++)
    {
        for (l = 0; l < dimension; l++)
        {
            printf("%f ", grid[k][l]);
        }
        printf("\n");
    }
    printf("\n");
    return 0;
}

//Loops over its allocation until cancelled by the main thread
void *workerThread(void *dummyArgs)
{
    struct threadArgs *args = (struct threadArgs *)dummyArgs;
    //loops until cancelled by the main thread once it detects a completed relaxation
    //using this method avoids the need for a shared variable to detect completion
    while (1)
    {
        for (int s = args->startRow; s < args->endRow; s++)
        {
            for (int t = 1; t < dimension - 1; t++)
            {
                args->outGrid[s][t] = relaxCell(args->inGrid, s, t);
            }
        }
        //first barrier ensures that all threads have computed their allocation before the main thread checks for completion
        pthread_barrier_wait(&barrier1);

        //Here the main thread checks if the grid is complete:
        //If not, inGrid is updated with the result of the previous iteration then all threads continue
        //If it is complete, all worker threads are cancelled by the main thread whilst waiting here
        pthread_barrier_wait(&barrier2);
    }
}

/**
 * Reforms a single relaxation iteration on all but the outermost rows and columns of a grid
 **/

int relaxGrid(double *inGrid[dimension], double *outGrid[dimension])
{
    pthread_t thread_id[workerThreads];

    int allocation = (dimension - 2) / workerThreads;
    int remainder = (dimension - 2) % workerThreads;
    int offset = 1;

    //Build a struct to contain the arguements for each worker thread
    //Then create the worker threads
    struct threadArgs *argsArr[workerThreads];
    int count = 0;
    for (int i = 0; i < workerThreads; i++)
    {
        argsArr[i] = (struct threadArgs *)malloc(sizeof(struct threadArgs));

        //Splits rows across threads as evenly as possible
        //Divided equally, with the remaining rows being assigned one by one until none remain
        argsArr[i]->startRow = i * allocation + offset;

        if (remainder > 0)
        {
            argsArr[i]->endRow = argsArr[i]->startRow + allocation + 1;
            offset++;
            remainder--;
        }
        else
        {
            argsArr[i]->endRow = argsArr[i]->startRow + allocation;
        }

        argsArr[i]->inGrid = inGrid;
        argsArr[i]->outGrid = outGrid;

        if (pthread_create(&thread_id[i], NULL, workerThread, argsArr[i]))
        {
            perror("thread create");
            exit(EXIT_FAILURE);
        }
    }
    //Once worker threads are created, loop constantly until the relaxation is complete
    while (1)
    {
        //once all threads have calculated their allocation check completeness
        pthread_barrier_wait(&barrier1);
        //if complete, destroy threads and clear arguement memory then return
        if (!checkComplete(inGrid, outGrid))
        {
            printf("count: %d\n",count);
            for (int j = 0; j < workerThreads; j++)
            {
                if (pthread_cancel(thread_id[j]) != 0)
                {
                    perror("thread cancel");
                    exit(EXIT_FAILURE);
                }
            }
            for (int k = 0; k < workerThreads; k++)
            {
                free(argsArr[k]);
            }
            return 0;
        }
        count++;
        printGrid(outGrid);
        //otherwise, update the inGrid with the result of the previous iteration
        swapGrids(outGrid, inGrid);
        //now hit barrier so worker threads start on next iteration
        pthread_barrier_wait(&barrier2);
    }
}

/**
 * Copies the values contained in one grid to another
 **/
int swapGrids(double *fromGrid[dimension], double *toGrid[dimension])
{
    int i;
    double *temp;
    for (i = 0; i < dimension; i++){
        temp = fromGrid[i];
        fromGrid[i] = toGrid[i];
        toGrid[i] = temp;
    }
    return 0;
}
