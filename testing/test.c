#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>


int main() {
    int fd = open("/dev/task_dev", O_RDWR);
    if (fd == -1) {
        printf("Error while opening\n");
        return -1;
    }
    char buf[256];
    char rbuf[256];

    scanf("%[^\n]", buf);
    getc(stdin);
    
    int res = write(fd, buf, strlen(buf) + 1);
    printf("Write res: %d\n", res);

    printf("Continue?\n");
    char e = getc(stdin);
    if (e != 'y') return 0;

    res = read(fd, rbuf, 256);
    printf("Read res: %d\n", res);
    printf("Message: %s\n", rbuf);
    close(fd);
    return 0;
}