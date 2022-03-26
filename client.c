#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    FILE *fp = fopen("./plot", "w");
    char buf[1];
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }


    for (int i = 0; i <= offset; i++) {
        struct timespec start_ts, end_ts;
        long long kernel_ts;
        lseek(fd, i, SEEK_SET);

        clock_gettime(CLOCK_MONOTONIC, &start_ts);
        kernel_ts = write(fd, buf, 1);
        clock_gettime(CLOCK_MONOTONIC, &end_ts);
        long long user_ts = (long long) (end_ts.tv_nsec - start_ts.tv_nsec);
        printf("%d %lld %lld\n", i, kernel_ts, user_ts);
        fprintf(fp, "%d %lld %lld\n", i, kernel_ts, user_ts);
    }

    close(fd);
    fclose(fp);
    return 0;
}
