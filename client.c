#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    char buf[500];
    // char write_buf[] = "testing writing";
    int offset = 500; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    struct timespec t1, t2;

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC, &t1);

        // long long sz = write(fd, buf, 2);
        // long long sz = write(fd, buf, 1);
        long long sz = read(fd, buf, 500);

        clock_gettime(CLOCK_MONOTONIC, &t2);

        // verify fib
        printf("fib(%d): %s\n", i, buf);
        printf("time %lld nanoseconds (10^-9 second)\n", sz);

        long long ut = (long long) (t2.tv_sec * 1e9 + t2.tv_nsec) -
                       (t1.tv_sec * 1e9 + t1.tv_nsec);

        // user space, kernel space, sys call overhead
        // printf("%d %lld %lld %lld\n", i, sz, ut, ut - sz);

        // long long start_time = (long long) (t1.tv_sec * 1e9 + t1.tv_nsec);
        // long long end_time = (long long) (t2.tv_sec * 1e9 + t2.tv_nsec);

        // user to kernel, kernel to user
        // printf("%d %lld %lld\n", i, sz - start_time, end_time - sz);
    }
    close(fd);
    return 0;
}
