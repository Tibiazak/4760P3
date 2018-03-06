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

static void interrupt()
{
    printf("Received interrupt!\n");
    shmdt(Clock);
    exit(1);
}

int main(int argc, char *argv[]) {
    signal(SIGUSR1, interrupt);
    int sharekey = atoi(argv[1]);

    printf("Process %d executed.\n", getpid());

    ClockID = shmget(sharekey, sizeof(int), 0777);
    Clock = (int *)shmat(ClockID, NULL, 0);

    printf("Process %d reads the clock at %d\n", getpid(), *Clock);

    sleep(5);

    shmdt(Clock);
    printf("Shared memory detached.\n");

    return 0;
}