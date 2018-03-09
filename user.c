//
// Created by Josh on 3/5/2018.
//

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/msg.h>
#include <string.h>
#include <time.h>
#include "clock.c"

#define BILLION 1000000000

int ClockID;
struct clock *Clock;
int MsgID;

struct mesg_buf {
    long mtype;
    char mtext[100];
} message;

static void interrupt()
{
    printf("Received interrupt!\n");
    shmdt(Clock);
    exit(1);
}

int main(int argc, char *argv[]) {
    signal(SIGUSR1, interrupt);
    int sharekey = atoi(argv[1]);
    int msgkey = atoi(argv[2]);
    unsigned long x;
    int totalwork;
    int workdone = 0;
    int work;
    int donesec;
    int donensec;

    srand(time(0));

    ClockID = shmget(sharekey, sizeof(int), 0777);
    Clock = (struct clock *)shmat(ClockID, NULL, 0);


    MsgID = msgget(msgkey, 0666);
//    message.mtype = 2;
//    strcpy(message.mtext, "This is a test of two different message types.");
//    msgsnd(MsgID, &message, sizeof(message), 0);
//    sleep(2);

//    message.mtype = 1;
//    strcpy(message.mtext, "This is a test of the message queue!");
//    msgsnd(MsgID, &message, sizeof(message), 0);

    // The following 4 lines are taken from stackoverflow
    // https://stackoverflow.com/questions/19870276/generate-a-random-number-from-0-to-10000000
    // Solution to have a random number larger than MAX_RAND
    x = rand();
    x <<= 15;
    x ^= rand();
    x %= 1000001;

    totalwork = (int) x;

    while(workdone < totalwork)
    {
        msgrcv(MsgID, &message, sizeof(message), 3, 0);
        printf("Entering CS\n");
        work = rand();
        if((work + workdone) > totalwork)
        {
            work = totalwork - workdone;
        }
        Clock->nsec += work;
        if(Clock->nsec >= BILLION)
        {
            Clock->sec++;
            Clock->nsec -= BILLION;
        }
        workdone += work;
        donensec = Clock->nsec;
        donesec = Clock->sec;
        message.mtype = 3;
        msgsnd(MsgID, &message, sizeof(message), 0);
    }

    message.mtype = 2;
    sprintf(message.mtext, "%d %d", donensec, totalwork);
    msgsnd(MsgID, &message, sizeof(message), 0);

    shmdt(Clock);
    return 0;
}