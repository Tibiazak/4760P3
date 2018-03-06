#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>


#define SHAREKEY 92195
#define SHAREKEYSTR "92195"
#define TIMER_MSG "Received timer interrupt!\n"

int ClockID;
int *Clock;

// A function from the setperiodic code, catches the interrupt and prints to screen
static void interrupt(int signo, siginfo_t *info, void *context)
{
    int errsave;

    errsave = errno;
    write(STDOUT_FILENO, TIMER_MSG, sizeof(TIMER_MSG) - 1);
    errno = errsave;
    signal(SIGUSR1, SIG_IGN);
    kill(-1*getpid(), SIGUSR1);
    shmdt(Clock);
    shmctl(ClockID, IPC_RMID, NULL);
    exit(1);
}

// A function from the setperiodic code, it sets up the interrupt handler
static int setinterrupt()
{
    struct sigaction act;

    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = interrupt;
    if (((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGALRM, &act, NULL) == -1)) || sigaction(SIGINT, &act, NULL) == -1)
    {
        return 1;
    }
    return 0;
}


// A function that sets up a timer to go off after a specified number of seconds
// The timer only goes off once
static int setperiodic(double sec)
{
    timer_t timerid;
    struct itimerspec value;

    if (timer_create(CLOCK_REALTIME, NULL, &timerid) == -1)
    {
        return -1;
    }
    value.it_value.tv_sec = (long)sec;
    value.it_value.tv_nsec = 0;
    value.it_interval.tv_sec = 0;
    value.it_interval.tv_nsec = 0;
    return timer_settime(timerid, 0, &value, NULL);
}


int main(int argc, char * argv[]) {
//    signal(SIGINT, interrupt);
    int i, pid, c;
    int maxprocs = 5;
    int endtime = 20;
    char* argarray[] = {"./user", SHAREKEYSTR, NULL};
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
                filename = optarg;
                printf("Log file name is: %s\n", filename);
                break;
            case 't':
                if(isdigit(*optarg))
                {
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

    if (setinterrupt() == -1)
    {
        perror("Failed to set up handler");
        return 1;
    }
    if (setperiodic((long) endtime) == -1)
    {
        perror("Failed to set up timer");
        return 1;
    }

    ClockID = shmget(SHAREKEY, sizeof(int), 0777 | IPC_CREAT);
    if(ClockID == -1)
    {
        perror("Master shmget");
        exit(1);
    }

    Clock = (int *)(shmat(ClockID, 0, 0));
    if(Clock == -1)
    {
        perror("Master shmat");
        exit(1);
    }

    *Clock = 25;

    printf("Clock is set to %d\n", *Clock);

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

    sleep(2);

    shmdt(Clock);
    shmctl(ClockID, IPC_RMID, NULL);
    printf("Exiting normally\n");
    return 0;
}
