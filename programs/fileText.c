#include <stdio.h>
#include <sys.h>
#include <file.h>

void main1()
{
    char data1[13] = "Hello World!";
    char data2[13];
    int fd = 0;
    int count = 0;
    int i, j;

    fd = creat("Jessy", 0666); // 刚创建好的文件，访问方式是可写
    if (fd > 0)
    {
        printf("The file %d is created.\n", fd);
    }
    else
    {
        printf("The file can not be created.\n");
    }
    close(fd);

    if (fork())
    {
        i = wait(&j);
        fd = open("Jessy", 1); // 以可读的方式打开文件
        count = read(fd, data2, j);
        printf("%d characters are read from file %d: %s.", count, fd, data2);
        printf("\n");
        close(fd);
    }
    else
    {
        fd = open("Jessy", 2); // 以可写的方式打开文件
        count = write(fd, data1, 12);
        if (count == 12)
        {
            printf("%d characters have been written into the file %d.\n", count, fd);
        }
        else
        {
            printf("The file can not be written successfully.\n");
        }
        close(fd);
        exit(count);
    }
}