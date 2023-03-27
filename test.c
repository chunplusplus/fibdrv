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

int main(int argc, char const *argv[])
{
    /* long long sz; */

    char *buf;
    int offset = atoi(argv[1]);

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    size_t size = cal_buf_size(offset);
    buf = malloc(size);
    lseek(fd, offset, SEEK_SET);
    read(fd, buf, size);
    printf("%s\n", buf);
    free(buf);

    return 0;
}
