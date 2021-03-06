/*
 * Joshua Bearden
 * CS 4760 Project 3
 * Message queues & a simulated clock
 *
 * This is a program to simulate the basic functions of an OS.
 * We fork child processes, they enter a mutually exclusive critical section (via message queues)
 * and increment a "clock". Then when they terminate they send a message to the parent.
 * The parent will, on receipt of this message, increment the "clock" and fork another process, all while
 * logging its simulated times and activities.
 *
 * Functions used:
 * Interrupt: A signal handler to catch SIGALRM and SIGINT and terminate all the processes cleanly.
 *
 * SetInterrupt: A function that registers Interrupt as the signal handler for SIGALRM and SIGINT.
 *
 * SetPeriodic: A function that sets up a timer to go off in a user-specified number of real-time seconds.
 *
 * In this program I did get 4 lines of code (in user.c) from StackOverflow (cited directly above said lines of code).
 * The code is a simple solution for allowing random numbers to be generated > RAND_MAX without introducing bias.
 */
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

// Declare some global variables so that shared memory can be cleaned from the interrupt handler
int ClockID;
struct clock *Clock;
int MsgID;
FILE *fp;

struct mesg_buf {
    long mtype;
    char mtext[100];
} message;

// A function that catches SIGINT and SIGALRM
// It prints an alert to the screen then sends a signal to all the child processes to terminate,
// frees all the shared memory and closes the file.
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
    if(argc == 1) //if no arguments passed
    {
        printf("ERROR: command line options required, please run ./oss -h for usage instructions.\n");
        return 1;
    }

    while ((c = getopt(argc, argv, "hs:l:t:")) != -1)
    {
        switch(c)
        {
            case 'h': // -h for help
                printf("Usage: ./oss [-s x] [-t z] -l filename\n");
                printf("-s x: x is the maximum number of concurrent processes (default 5)\n");
                printf("-t z: z is the number of real time seconds you would like the program to run\n");
                printf("-l filename: filename is the name you would like the log file to have. This is a required argument\n");
                return 0;
            case 's': // -s for max number of processes
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
            case 'l': // -l for filename (required!)
                filename = optarg;
                printf("Log file name is: %s\n", filename);
                break;
            case 't': // -t for total running time (wall clock time)
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
                if(endtime <= 0)
                {
                    printf("Error: Number of processes must be positive, please run ./oss -h for more information\n");
                    return 1;
                }
                break;
            default: // anything else, fail
                printf("Expected format: [-s x] -l filename -t z\n");
                printf("-s for max number of processes, -l for log file name, and -t for number of seconds to run.\n");
                return 1;
        }
    }

    if(!filename) //ensure filename was passed
    {
        printf("Error! Must specify a filename with the -l flag, please run ./oss -h for more info.\n");
        return(1);
    }


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

    // initialize the clock
    Clock->sec = 0;
    Clock->nsec = 0;

    // Create the message queue
    MsgID = msgget(MSGKEY, 0666 | IPC_CREAT);

    message.mtype = 3; // Allows a process to enter the critical section

    // open file
    fp = fopen(filename, "w");

    // Fork processes
    for (i = 0; i < maxprocs; i++)
    {
        pid = fork();
        pr_count++;
        totalprocs++;
        if(pid == 0)
        {
            if(execvp(argarray[0], argarray) < 0) //execute user
            {
                printf("Execution failed!\n");
                return 1;
            }
        } else if(pid < 0) {
            printf("Fork failed!\n");
            return 1;
        }
        // output process creation to file
        fprintf(fp, "Master: Creating child process %d at my time %d.%d\n", pid, Clock->sec, Clock->nsec);
    }

    msgsnd(MsgID, &message, sizeof(message), 0); // send the mutex signal


    // loop while we haven't used more than 100 processes or gone over 2 simulated seconds
    while(totalprocs < 101 && !timeElapsed)
    {
        // wait for a child to terminate and receive its message
        msgrcv(MsgID, &message, sizeof(message), 2, 0);

        // process the message
        strcpy(messageString, message.mtext);
        temp = strtok(messageString, " ");
        pid = atoi(temp);
        temp = strtok(NULL, " ");
        procendsec = atoi(temp);
        temp = strtok(NULL, " ");
        procendnsec = atoi(temp);
        temp = strtok(NULL, " ");
        proctime = atoi(temp);

        // print processed message to file
        fprintf(fp, "Master: Child %d terminating at my time %d.%d, because it reached %d.%d, which lived for %d nanoseconds\n",
                pid, Clock->sec, Clock->nsec, procendsec, procendnsec, proctime);

        // request entry to critical section
        msgrcv(MsgID, &message, sizeof(message), 3, 0);

        // increment clock and ensure it is accurate and not over 2 seconds
        Clock->nsec += 100;
        if (Clock->nsec > BILLION)
        {
            Clock->sec++;
            Clock->nsec -= BILLION;
        }
        if (Clock->sec >= 2)
        {
            timeElapsed = true;
        }

        // Prepare another mutex message and fork a new process
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
        //print process creation
        fprintf(fp, "Master: Creating child process %d at my time %d.%d\n", pid, Clock->sec, Clock->nsec);

        // send mutex message
        msgsnd(MsgID, &message, sizeof(message), 0);
    }

    // we're done, detach and free shared memory and close the file
    // then send a kill signal to the children and wait for them to exit
    shmdt(Clock);
    shmctl(ClockID, IPC_RMID, NULL);
    msgctl(MsgID, IPC_RMID, NULL);
    fclose(fp);
    signal(SIGUSR1, SIG_IGN);
    kill(-1*getpid(), SIGUSR1);
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
