#include <stdio.h>
#include <sys/types.h>


int main() {
    int i;
    char* argarray[] = {"./user", NULL};
    for (i = 0; i < 5; i++)
    {
        printf("Forking a new process\n");

        if(fork() == 0) {
            printf("Child executing new program\n");
            execvp(argarray[0], argarray);
        }
    }
}