#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define PERF_RESULT_DOWNWARD_PATH "orig_downward_perf_res.txt"
#define PERF_RESULT_UPWARD_PATH "orig_upward_perf_res.txt"
#define TEST_COUNT 100
#define FABONACII_INPUT 100

#define unlikely(x) __builtin_expect(!!(x), 0)

typedef long diff_ns;

static inline diff_ns time_diff_ns(struct timespec *start, struct timespec *end)
{
    long delta = end->tv_nsec - start->tv_nsec;
    if ((end->tv_sec - start->tv_sec) > 1)
        return -1;  // calculation of fabonacci spends too much time (at least 2
                    // second), won't happen at usual

    return (delta >= 0 ? delta : (delta + 10e9));
}

int main()
{
    FILE *fp_perf;
    struct timespec start, end;
    long avg_res[FABONACII_INPUT] = {0}, tmp_nsec;

    int fd;
    long long sz;

    char buf[1];
    char write_buf[] = "testing writing";
    int offset = FABONACII_INPUT;  // TODO: test something bigger than the limit
    int i = 0, j, avg_buf_idx;

    fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        goto bad_fopen;
    }

    for (i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    fp_perf = fopen(PERF_RESULT_UPWARD_PATH, "w");
    if (!fp_perf) {
        perror("Failed to create result file");
        goto bad_fopen;
    }

    for (j = 0; j < TEST_COUNT; j++) {
        avg_buf_idx = 0;  // initilize index for each test
        for (i = 0; i <= offset; i++) {
            lseek(fd, i, SEEK_SET);

            clock_gettime(CLOCK_REALTIME, &start);
            sz = read(fd, buf, 1);
            clock_gettime(CLOCK_REALTIME, &end);

            tmp_nsec = time_diff_ns(&start, &end);

            // Since not sure how it's being translated, hence split it up
            // (TODO)
            avg_res[avg_buf_idx] += tmp_nsec;
            avg_buf_idx++;

            if (unlikely(tmp_nsec < 0))  // the macro has no help for perf
                                         // improvement essentially
                goto timeout;

            printf("Reading from " FIB_DEV
                   " at offset %d, returned the sequence "
                   "%lld.\n",
                   i, sz);
        }
    }

    // avg. time spend calculation for each fabonacci, then write to result file
    for (j = 0; j < FABONACII_INPUT; j++) {
        avg_res[j] /= TEST_COUNT;
        fprintf(fp_perf, "%d %ld\n", j, avg_res[j]);
    }

    fp_perf = fopen(PERF_RESULT_DOWNWARD_PATH, "w");
    if (!fp_perf) {
        perror("Failed to create result file");
        exit(1);
    }

    // reset avg. buffer for next measurement
    for (j = 0; j < FABONACII_INPUT; j++)
        avg_res[j] = 0;

    for (j = 0; j < TEST_COUNT; j++) {
        avg_buf_idx = 99;

        for (i = offset; i >= 0; i--) {
            lseek(fd, i, SEEK_SET);

            clock_gettime(CLOCK_REALTIME, &start);
            sz = read(fd, buf, 1);
            clock_gettime(CLOCK_REALTIME, &end);


            tmp_nsec = time_diff_ns(&start, &end);

            avg_res[avg_buf_idx] += tmp_nsec;
            avg_buf_idx--;

            if (unlikely(tmp_nsec < 0))
                goto timeout;

            printf("Reading from " FIB_DEV
                   " at offset %d, returned the sequence "
                   "%lld.\n",
                   i, sz);
        }
    }

    for (j = 0; j < FABONACII_INPUT; j++) {
        avg_res[j] /= TEST_COUNT;
        fprintf(fp_perf, "%d %ld\n", j, avg_res[j]);
    }



    fclose(fp_perf);
    close(fd);
    return 0;

timeout:
    puts(
        "Time spend of single calculation of fabonacci is too long (at least 1 "
        "second)");

    close(fd);
    fclose(fp_perf);

    return -1;

bad_fopen:
    close(fd);
    fclose(fp_perf);

    return -1;
}
