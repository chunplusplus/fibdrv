#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

#define LOGPHI (20898764025)
#define LOGSQRT5 (34948500216)
#define SCALE (100000000000)

size_t cal_buf_size(int n)
{
    size_t digits = (n * LOGPHI - LOGSQRT5) / SCALE;
    return digits + 2;
}

int main()
{
    long long sz;

    char *buf;
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        size_t size = cal_buf_size(i);
        buf = malloc(size);
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, size);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
        free(buf);
    }

    for (int i = offset; i >= 0; i--) {
        size_t size = cal_buf_size(i);
        buf = malloc(size);
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, size);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
        free(buf);
    }

    close(fd);
    return 0;
}
