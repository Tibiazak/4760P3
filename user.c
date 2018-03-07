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

int ClockID;
int *Clock;
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

    printf("Process %d executed.\n", getpid());

    ClockID = shmget(sharekey, sizeof(int), 0777);
    Clock = (int *)shmat(ClockID, NULL, 0);

    printf("Process %d reads the clock at %d\n", getpid(), *Clock);

    MsgID = msgget(MSGKEY, 0666);
    message.mtype = 1;
    message.mtext = "This is a test of the message queue!";
    msgsnd(MsgID, &message, sizeof(message), 0);

    sleep(5);

    shmdt(Clock);
    printf("Shared memory detached.\n");

    return 0;
}