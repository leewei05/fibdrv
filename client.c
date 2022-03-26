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
    int offset = 500; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }


    for (int i = 0; i <= offset; i++) {
        long long kt;
        lseek(fd, i, SEEK_SET);
        kt = write(fd, buf, 1);
        printf("%d %lld.\n", i, kt);
        fprintf(fp, "%d %lld\n", i, kt);
    }

    close(fd);
    fclose(fp);
    return 0;
}
