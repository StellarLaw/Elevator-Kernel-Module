#include <stdio.h>
#include <unistd.h>


int main() {
    pid_t pid = getpid(); //1 get the process ID

    pid_t ppid = getppid(); // 2 get parent process ID

    char message[] = "Process ID and Parent Process ID obtained.\n";
    write(STDOUT_FILENO, message, sizeof(message) - 1); // 3 write to std output 

    sleep(1); // 4 sleep for a second

    return 0;
}
