//
// Created by Josh on 3/5/2018.
//

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
    printf("Process %d executed and terminating.\n", getpid());
    return 0;
}