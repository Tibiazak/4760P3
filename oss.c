#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>


int main() {
    int i, pid;
    char* argarray[] = {"./user", NULL};
    for (i = 0; i < 5; i++)
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
    return 0;
}