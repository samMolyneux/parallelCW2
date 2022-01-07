#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/**
 * Fills the first row and left most columns of a 2d array with 1s
 **/
int populateGrid(double **grid, int dimension)
{
    for (int i = 0; i < dimension; i++)
    {
        grid[i][0] = 1;
        grid[0][i] = 1;
    }
    return 0;
}

// Program to write a grid of random 1s and 0s to a file
int main(int argc, char **argv)
{
    // size defaults to 10 if there are not arguments
    int size = 10;
    int opt;
    int basic = 0;
    // Handle the arguement(s)
    while ((opt = getopt(argc, argv, "bs:")) != -1)
    {
        switch (opt)
        {
        case 's':
            size = atoi(optarg); // the dimensions of the grid
            break;
        case 'b':
            basic = 1;
            break;
        default:
            break;
        }
    }
    printf("Size: %d\n", size);

    // Allocate memory for the grid
    double **grid = (double **)malloc(sizeof(double *) * size);
    int l;
    for (l = 0; l < size; l++)
    {
        grid[l] = (double *)malloc(sizeof(double) * size);
    }

    if (basic)
    {
        printf("basic\n");
        populateGrid(grid, size);
    }
    else
    {
        // Generate the grid
        srand(time(NULL));

        for (int x = 0; x < size; x++)
        {
            for (int y = 0; y < size; y++)
            {
                grid[x][y] = (double)(rand() % 2);
                printf("%f ", grid[x][y]);
            }
            printf("\n");
        }
    }
    // Write to file

    FILE *write_ptr;

    char *file_name = malloc(sizeof(char) * 50);

    sprintf(file_name, "grids\\grid_%d.bin", size);
    printf("File name: %s\n", file_name);
    write_ptr = fopen(file_name, "wb");

    for (int x = 0; x < size; x++)
    {
        fwrite(grid[x], sizeof(double) * size, 1, write_ptr);
    }

    fclose(write_ptr);

    return 0;
}
