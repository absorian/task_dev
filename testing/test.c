#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>


int main() {
    int fd = open("/dev/task_dev", O_RDONLY);
    if (fd == -1) {
        printf("Error while opening\n");
        return -1;
    }
    printf("Opening was successful\n");
    close(fd);
    return 0;
}