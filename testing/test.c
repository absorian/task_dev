#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "../task_dev.h"

static void print_date_ns(uint64_t ns) {
    time_t sec = ns / 1e9L;
    char* tm = ctime(&sec);
    tm[strlen(tm) - 1] = '\0'; // rm '\n'
    printf("%s", tm);
}


int main(int argc, char **argv) {
    if (argv[1] == NULL) return -1;
    int cmd = atoi(argv[1]);
    if (!cmd) return -1;
    
    int fd = open("/dev/task_dev", O_RDWR);
    if (fd == -1) {
        printf("Error while opening\n");
        return -1;
    }
 
    switch (cmd) {
    case 1: {
        char buf[256];
        scanf("%[^\n]", buf);
        int res = write(fd, buf, strlen(buf) + 1);
        printf("Write res: %d\n", res);
        break;
    }
    case 2: {
        char buf[256];
        char t;
        scanf("%c", &t); // stop point
        int res = read(fd, buf, 256);
        printf("Read res: %d\n", res);
        printf("Message: %s\n", buf);
        break;
    }
    case 3: {
        int scmd = atoi(argv[2]);
        if (!scmd) return -1;
        switch (scmd)
        {
        case 1:
            struct tdev_ioc_info info;
            if (ioctl(fd, TDEV_IOC_GETINFO, &info) < 0) {
                printf("Error while calling ioctl TDEV_IOC_GETINFO\n");
            } else {
                printf("Last write: ");
                print_date_ns(info.last_write.timestamp);
                printf(" from pid: %d, uid: %u\n", info.last_write.pid, info.last_write.uid);
                printf("Last read: ");
                print_date_ns(info.last_read.timestamp);
                printf(" from pid: %d, uid: %u\n", info.last_read.pid, info.last_read.uid);
            }
            break;
        case 2: {
            int on = 0;
            if (ioctl(fd, TDEV_IOC_NONBLOCK, &on) < 0) {
                printf("Error while calling ioctl TDEV_IOC_NONBLOCK\n");
            }
            break;
        }     
        case 3: {
            int on = 1;
            if (ioctl(fd, TDEV_IOC_NONBLOCK, &on) < 0) {
                printf("Error while calling ioctl TDEV_IOC_NONBLOCK\n");
            }
            break;   
        }   
        default:
            break;
        }
    }
    default:
        break;
    }
    close(fd);
    return 0;
}