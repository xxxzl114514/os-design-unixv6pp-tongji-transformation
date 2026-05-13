#include <stdio.h>
int version = 1;

int sum(int var1,int var2)
{
    int count;
    version = 2;
    count = var1 + var2;
    return (count);
}

void main1()
{
    /*int c, d, result;
    c = 1;
    d = 2;
    result = sum(c, d);
    printf("result=%d\n", result);*/

    char a[] = "Hello";
    char b[] = "World!";
    printf("%s %s\n", a, b);
}