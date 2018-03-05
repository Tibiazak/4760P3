//
// Created by Josh on 3/5/2018.
//

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
    int clockid;
    int *clock;

    printf("Process %d executed.\n", getpid());

    clockid = shmget(sharekey, sizeof(int), 0777);
    clock = (int *)shmat(clockid, NULL, 0);

    printf("Process %d reads the clock at %d\n", getpid(), *clock);

    shmdt(clock);
    printf("Shared memory detached.\n");

    return 0;
}