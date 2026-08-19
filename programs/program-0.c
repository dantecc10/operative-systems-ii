#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main() {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    } 
    else if (pid == 0) {
        // Child process runs this loop
        while (1) {
            printf("Child process loop running. PID: %d\n", getpid());
            sleep(2); // Wait 2 seconds
        }
    } 
    else {
        // Parent process runs this
        printf("Parent process created child with PID: %d\n", pid);
        printf("Parent exiting now.\n");
    }

    return 0;
}
