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

    while ((c = getopt(argc, argv, "hs:l:t:")) != -1)
    {
        switch(c)
        {
            case 'h':
                printf("Help options go here!\n");
                break;
            case 's':
                if(isdigit(optarg))
                {
                    maxprocs = atoi(optarg);
                }
                else
                {
                    printf("Error, -s must be followed by an integer!\n");
                    return 1;
                }
                break;
            case 'l':
                filename = optarg;
                break;
            case 't':
                if(isdigit(optarg))
                {
                    endtime = atoi(optarg);
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
    sleep(3);
    return 0;
}