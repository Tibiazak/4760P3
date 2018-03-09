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
#include <sys/msg.h>
#include <string.h>
#include "clock.c"
#include <stdbool.h>


#define SHAREKEY 92195
#define SHAREKEYSTR "92195"
#define TIMER_MSG "Received timer interrupt!\n"
#define MSGKEY 110992
#define MSGKEYSTR "110992"
#define BILLION 1000000000
#define PR_LIMIT 17

int ClockID;
struct clock *Clock;
int MsgID;
FILE *fp;

struct mesg_buf {
    long mtype;
    char mtext[100];
} message;

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
    msgctl(MsgID, IPC_RMID, NULL);
    fclose(fp);
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
    int i, pid, c, status;
    int maxprocs = 5;
    int endtime = 20;
    int pr_count = 0;
    int totalprocs = 0;
    char* argarray[] = {"./user", SHAREKEYSTR, MSGKEYSTR, NULL};
    char* filename;
    pid_t wait = 0;
    bool timeElapsed = false;
    char messageString[100];
    int proctime;
    int procendsec;
    int procendnsec;
    char* temp;
    // Process command line arguments
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
                    if(maxprocs > PR_LIMIT)
                    {
                        printf("Error: Too many processes. No more than 17 allowed.\n");
                        return(1);
                    }
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

    if(!filename)
    {
        printf("Error! Must specify a filename with the -l flag, please run ./oss -h for more info.\n");
        return(1);
    }

    printf("Finished processing command line arguments.\n");


    // Set the timer-kill
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


    // Allocate & attach shared memory for the clock
    ClockID = shmget(SHAREKEY, sizeof(int), 0777 | IPC_CREAT);
    if(ClockID == -1)
    {
        perror("Master shmget");
        exit(1);
    }

    Clock = (struct clock *)(shmat(ClockID, 0, 0));
    if(Clock == -1)
    {
        perror("Master shmat");
        exit(1);
    }

    Clock->sec = 0;
    Clock->nsec = 0;

    printf("Clock is set to %d:%d\n", Clock->sec, Clock->nsec);


    // Create the message queue
    MsgID = msgget(MSGKEY, 0666 | IPC_CREAT);

    message.mtype = 3; // Allows a process to enter the critical section
    fp = fopen(filename, "w");

    // Fork processes
    for (i = 0; i < maxprocs; i++)
    {
        pid = fork();
        pr_count++;
        totalprocs++;
        if(pid == 0)
        {
            if(execvp(argarray[0], argarray) < 0)
            {
                printf("Execution failed!\n");
                return 1;
            }
        } else if(pid < 0) {
            printf("Fork failed!\n");
            return 1;
        }
        fprintf(fp, "Master: Creating child process %d at my time %d.%d\n", pid, Clock->sec, Clock->nsec);
    }

    msgsnd(MsgID, &message, sizeof(message), 0);

    while(totalprocs < 101 && !timeElapsed)
    {
        msgrcv(MsgID, &message, sizeof(message), 2, 0);
        strcpy(messageString, message.mtext);
        temp = strtok(messageString, " ");
        pid = atoi(temp);
        temp = strtok(NULL, " ");
        procendsec = atoi(temp);
        temp = strtok(NULL, " ");
        procendnsec = atoi(temp);
        temp = strtok(NULL, " ");
        proctime = atoi(temp);
        fprintf(fp, "Master: Child %d terminating at my time %d.%d, because it reached %d.%d, which lived for %d nanoseconds\n",
                pid, Clock->sec, Clock->nsec, procendsec, procendnsec, proctime);
        msgrcv(MsgID, &message, sizeof(message), 3, 0);
        Clock->nsec += 100;
        if (Clock->nsec > BILLION)
        {
            Clock->sec++;
            Clock->nsec -= BILLION;
        }
        if (Clock->sec == 2)
        {
            timeElapsed = true;
        }
        message.mtype = 3;
        pid = fork();
        totalprocs++;
        if(pid == 0)
        {
            if(execvp(argarray[0], argarray) < 0)
            {
                printf("Execution failed!\n");
                return 1;
            }
        } else if(pid < 0){
            printf("Fork failed!\n");
            return 1;
        }
        fprintf(fp, "Master: Creating child process %d at my time %d.%d\n", pid, Clock->sec, Clock->nsec);
        msgsnd(MsgID, &message, sizeof(message), 0);
    }




    shmdt(Clock);
    shmctl(ClockID, IPC_RMID, NULL);
    msgctl(MsgID, IPC_RMID, NULL);
    fclose(fp);
    while(pr_count > 0)
    {
        wait = waitpid(-1, &status, WNOHANG);
        if(wait != 0)
        {
            pr_count--;
        }
    }
    printf("Exiting normally\n");
    return 0;
}
