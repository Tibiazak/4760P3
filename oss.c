#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>


int main(int argc, char * argv[]) {
    int i, pid, c;
    int maxprocs = 5;
    int endtime;
    char* argarray[] = {"./user", NULL};
    char* filename;

    if(argc == 1)
    {
        printf("ERROR: command line options required, please run ./oss -h for usage instructions.\n");
        return 1;
    }

    while ((c = getopt(argc, argv, "hs:l:t:")) != -1)
    {
        switch(c)
        {
            case 'h':
                printf("Help options go here!\n");
                return 0;
            case 's':
                if(isdigit(*optarg))
                {
                    maxprocs = atoi(optarg);
                    printf("Max %d processes\n", maxprocs);
                }
                else
                {
                    printf("Error, -s must be followed by an integer!\n");
                    return 1;
                }
                break;
            case 'l':
                printf("Filename option\n");
                filename = optarg;
                printf("Log file name is: %s\n", filename);
                break;
            case 't':
                printf("Time option\n");
                if(isdigit(*optarg))
                {
                    printf("isdigit True\n");
                    endtime = atoi(optarg);
                    printf("Max time to run: %d\n", endtime);
                }
                else
                {
                    printf("Error, -t must be followed by an integer!\n");
                    return 1;
                }
                break;
            default:
                printf("Expected format: [-s x] -l filename -t z\n");
                printf("-s for max number of processes, -l for log file name, and -t for number of seconds to run.\n");
                return 1;
        }
    }

    printf("Finished processing command line arguments.\n");
    for (i = 0; i < maxprocs; i++)
    {
        printf("Forking a new process\n");

        pid = fork();
        if(pid == 0)
        {
            printf("Child executing new program\n");
            if(execvp(argarray[0], argarray) < 0)
            {
                printf("Execution failed!\n");
                return 1;
            }
        } else if(pid < 0) {
            printf("Fork failed!\n");
            return 1;
        }
    }
    sleep(1);
    return 0;
}