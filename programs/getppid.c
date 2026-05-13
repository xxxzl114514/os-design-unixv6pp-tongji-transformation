#include <stdio.h>
#include <sys.h>

int main1(){
    int pid, ppid;

    pid = getpid(pid);
    ppid = getppid(pid);
    printf("This is Process %d speaking...\n", pid);
    printf("My Parent Process ID is %d\n", ppid);

    return 0;
}