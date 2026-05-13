#include <stdio.h>
#include <sys.h>

int main1()
{
    int i;
    printf("%d %d \n", getpid(), getppid(getpid()));
    for ( i = 0; i < 3; ++i){
        if(fork()==0){
            printf("%d %d \n", getpid(), getppid(getpid()));
        }
    }
    sleep(2);
    return 0;
}