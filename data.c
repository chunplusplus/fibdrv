#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

#define LOGPHI (20898764025)
#define LOGSQRT5 (34948500216)
#define SCALE (100000000000)

#define OFFSET 100

static inline long long get_nanotime()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

size_t cal_buf_size(int n)
{
    size_t digits = (n * LOGPHI - LOGSQRT5) / SCALE;
    return digits + 2;
}

int main(int argc, char const *argv[])
{
    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    FILE *data = fopen("data.txt", "w");
    if (!data) {
        perror("Failed to open data text");
        exit(1);
    }

    for (int i = 0; i <= OFFSET; i++) {
        lseek(fd, i, SEEK_SET);
        long long start = get_nanotime();
        size_t size = cal_buf_size(i);
        char *buf = malloc(size);
        long long ktime = read(fd, buf, size);
        long long utime = get_nanotime() - start;
        fprintf(data, "%d %lld %lld %lld\n", i, ktime, utime, utime - ktime);

        free(buf);
    }

    close(fd);
    fclose(data);

    return 0;
}
