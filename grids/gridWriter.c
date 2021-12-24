#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    // size defaults to 10,000 if there are not arguments
    int size = 10;
    int opt;

    // Handle the arguement(s)
    while ((opt = getopt(argc, argv, "s:")) != -1)
    {
        switch (opt)
        {
        case 's':
            size = atoi(optarg);
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

    FILE *write_ptr;

    char *file_name = malloc(sizeof(char) * 50);

    sprintf(file_name, "grid_%d.bin", size);
    printf("File name: %s\n", file_name);
    write_ptr = fopen(file_name, "wb");

    for (int x = 0; x < size; x++)
    {
        fwrite(grid[x], sizeof(double) * size, 1, write_ptr);
    }

    fclose(write_ptr);

    double **read = (double **)malloc(sizeof(double *) * size);

    for (l = 0; l < size; l++)
    {
        read[l] = (double *)malloc(sizeof(double) * size);
    }

    FILE *ptr;
    ptr = fopen(file_name, "rb"); // r for read, b for binary

    for (int x = 0; x < size; x++)
    {
        fread(read[x], sizeof(double)*size, 1, ptr);
        for (int y = 0; y < size; y++)
        {
            printf("%f ", read[x][y]);
        }
        printf("\n");
    }
    fclose(ptr);
    return 0;
}