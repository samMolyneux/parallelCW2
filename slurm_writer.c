#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

int dimension_start = 16000;
int processes_start = 176;
double precision_start = 0.000000001;

int simple_start = 50000;

int di_pro_start_di = 10000;
int di_pro_start_pro = 176;

char *openGrid(int dimension, int simple)
{
    char *grid_file_name = malloc(sizeof(char) * 50);
    if (simple)
    {
        sprintf(grid_file_name, "grids\\simple_%d.bin", dimension);
    }
    else
    {
        sprintf(grid_file_name, "grids\\grid_%d.bin", dimension);
    }
    if (access(grid_file_name, F_OK) == 0)
    {
        //file exists
    }
    else
    {
        // file doesn't exist
        // Allocate memory for the grid
        double **grid = (double **)malloc(sizeof(double *) * dimension);
        int l;
        for (l = 0; l < dimension; l++)
        {
            grid[l] = (double *)malloc(sizeof(double) * dimension);
        }
        if (simple)
        {
            for (int i = 0; i < dimension; i++)
            {
                grid[i][0] = 1;
                grid[0][i] = 1;
            }
        }
        else
        {
            srand(time(NULL));

            for (int x = 0; x < dimension; x++)
            {
                for (int y = 0; y < dimension; y++)
                {
                    grid[x][y] = (double)(rand() % 2);
                }
            }
        }

        FILE *write_ptr = fopen(grid_file_name, "wb");
        for (int x = 0; x < dimension; x++)
        {
            fwrite(grid[x], sizeof(double) * dimension, 1, write_ptr);
        }
        fclose(write_ptr);

        for (int x = 0; x < dimension; x++)
        {
            free(grid[x]);
        }
        free(grid);
    }
    return grid_file_name;
}

char *writeSlurm(int dimension, double precision, int processes, char *next_slurm, char *type)
{

    int nodes = (processes + 43) / 44;
    int tasks_per_node = processes / nodes;
    if (processes > 132)
    {
        nodes = 4;
        tasks_per_node = processes / nodes;
    }

    char *grid_file;

    if (!strcmp("simple", type))
    {
        grid_file = openGrid(dimension, 1);
    }
    else
    {
        grid_file = openGrid(dimension, 0);
    }

    char *slurm_file_name = malloc(sizeof(char) * 50);
    sprintf(slurm_file_name, "scalability_testing\\%s_%d_%g_%d.batch", type, dimension, precision, processes);

    char *slurm_name = malloc(sizeof(char) * 50);
    sprintf(slurm_name, "%s_%d_%g_%d", type, dimension, precision, processes);

    char *slurm_err_name = malloc(sizeof(char) * 50);
    sprintf(slurm_err_name, "%s_%d_%g_%d.err", type, dimension, precision, processes);

    char *slurm_out_name = malloc(sizeof(char) * 50);
    sprintf(slurm_out_name, "%s_%d_%g_%d.out", type, dimension, precision, processes);

    FILE *write_ptr = fopen(slurm_file_name, "w+");

    fprintf(write_ptr, "#!/bin/bash\n\n");

    fprintf(write_ptr, "#SBATCH --account=cm30225\n\n");

    fprintf(write_ptr, "#SBATCH --job-name=%s\n", slurm_name);
    fprintf(write_ptr, "#SBATCH --output=out/%s\n", slurm_out_name);
    fprintf(write_ptr, "#SBATCH --error=err/%s\n\n", slurm_err_name);

    fprintf(write_ptr, "#SBATCH --nodes=%d\n", nodes);
    fprintf(write_ptr, "#SBATCH --ntasks-per-node=%d\n\n", tasks_per_node);

    fprintf(write_ptr, "#SBATCH --time=00:19:30\n");
    fprintf(write_ptr, "#SBATCH --mail-type=FAIL\n");
    fprintf(write_ptr, "#SBATCH --mail-user=sm2744@bath.ac.uk\n\n");

    fprintf(write_ptr, "mpirun main.exe -d %d -p %.15f -t %s_data.txt -f %s\n",
            dimension,
            precision,
            type,
            grid_file);
    if (next_slurm)
    {
        fprintf(write_ptr, "sbatch %s", next_slurm);
    }
    fclose(write_ptr);

    free(slurm_name);
    free(slurm_err_name);
    free(slurm_out_name);

    return slurm_file_name;
}

int main(int argc, char **argv)
{
    int opt;
    // size defaults to 10 if there are not arguments
    int dimension = 10;

    double precision = 0.1;

    int processes = 4;
    // Handle the arguement(s)
    while ((opt = getopt(argc, argv, "d:p:n:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            dimension = atoi(optarg); // the dimensions of the grid
            break;
        case 'p':
            precision = atof(optarg);
            break;
        case 'n':
            processes = atoi(optarg);
            break;
        default:
            break;
        }
    }

    double scale_precision = precision_start;
    char *next_slurm_precision = NULL;
    while (scale_precision <= 0.1)
    {

        char *slurm = writeSlurm(dimension, scale_precision, processes, next_slurm_precision, "precision");
        next_slurm_precision = slurm;
        scale_precision = scale_precision * 10;
    }

    char *next_slurm_dimension = NULL;
    int scale_dimension = dimension_start;
    while (scale_dimension >= 1000)
    {
        next_slurm_dimension = writeSlurm(scale_dimension, precision, processes, next_slurm_dimension, "dimension");
        scale_dimension -= 500;
    }

    char *next_slurm_simple = NULL;
    int scale_simple = simple_start;

    while (scale_simple >= 1000)
    {
        next_slurm_simple = writeSlurm(scale_simple,
                                       precision,
                                       processes,
                                       next_slurm_simple,
                                       "simple");
        scale_simple -= 2000;
    }

    char *next_slurm_processes = NULL;
    int scale_processes = processes_start;
    while (scale_processes >= 4)
    {

        next_slurm_processes = writeSlurm(dimension, precision, scale_processes, next_slurm_processes, "processes");
        scale_processes -= 4;
    }

    char *next_slurm_di_pro = NULL;
    scale_dimension = di_pro_start_di;
    scale_processes = di_pro_start_pro;
    while (scale_processes >= 44)
    {
        while (scale_dimension >= 1000)
        {
            next_slurm_di_pro = writeSlurm(scale_dimension, precision, scale_processes, next_slurm_di_pro, "di_pro");
            scale_dimension -= 500;
        }
        scale_dimension = di_pro_start_di;
        scale_processes -= 44;
    }

    printf("Batches to run:\n\n %s\n %s\n %s\n %s\n %s\n",
           next_slurm_precision,
           next_slurm_dimension,
           next_slurm_simple,
           next_slurm_processes,
           next_slurm_di_pro);
}