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
typedef int64_t ktime_t;  // TODO: find the way to include ktime.h


struct read_sys_rt {
    long long fabo_res;
    ktime_t K_delta;  // real calculation time in kernel, K stands for kernel
};

static inline diff_ns time_diff_ns(struct timespec *start, struct timespec *end)
{
    long delta = end->tv_nsec - start->tv_nsec;
    if ((end->tv_sec - start->tv_sec) > 1)
        return -1;  // calculation of fabonacci spends too much time (at least 2
                    // second), won't happen at usual

    return delta;
}

int main()
{
    FILE *fp_perf;
    struct timespec start, end;
    long avg_res_userspace[FABONACII_INPUT] = {0},
         avg_res_kernelspace[FABONACII_INPUT] = {0}, tmp_nsec;
    ktime_t avg_trans_time[FABONACII_INPUT] = {0};
    struct read_sys_rt res_from_kernel;
    size_t rd_size = sizeof(ktime_t);

    int fd;
    long long sz;

    ktime_t buf;
    char write_buf[] = "testing writing";
    int offset = FABONACII_INPUT;  // TODO: test something bigger than the limit
    int i = 0, j, res_buf_idx;  // All buffer in single test shares this index

    fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        goto bad_fopen;
    }

    /* every test should start after a dummy test, which can improve the
     * accuracy of the measurement. But if you have large TEST_COUNT, this can
     * be ignored */

    for (i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    fp_perf = fopen(PERF_RESULT_DOWNWARD_PATH, "w");
    if (!fp_perf) {
        perror("Failed to create result file");
        exit(1);
    }

    // reset avg. buffer for next measurement
    for (j = 0; j < FABONACII_INPUT; j++)
        avg_res_userspace[j] = 0;

    // TODO: check if result of upward and downward are similar, we may
    // deprecating downward or upward if it's similar
    for (j = 0; j < TEST_COUNT; j++) {
        res_buf_idx = 99;

        for (i = offset; i >= 0; i--) {
            lseek(fd, i, SEEK_SET);

            clock_gettime(CLOCK_REALTIME, &start);
            sz = read(fd, &buf, rd_size);
            clock_gettime(CLOCK_REALTIME, &end);


            tmp_nsec = time_diff_ns(&start, &end);

            avg_res_userspace[res_buf_idx] += tmp_nsec;
            res_buf_idx--;

            if (unlikely(tmp_nsec < 0))
                goto timeout;

            printf("Reading from " FIB_DEV
                   " at offset %d, returned the sequence "
                   "%lld.\n",
                   i, sz);
        }
    }

    for (j = 0; j < FABONACII_INPUT; j++) {
        avg_res_userspace[j] /= TEST_COUNT;
        fprintf(fp_perf, "%d %ld\n", j, avg_res_userspace[j]);
    }

    fp_perf = fopen(PERF_RESULT_UPWARD_PATH, "w");
    if (!fp_perf) {
        perror("Failed to create result file");
        goto bad_fopen;
    }

    for (j = 0; j < TEST_COUNT; j++) {
        res_buf_idx = 0;  // initilize index for each test
        for (i = 0; i <= offset; i++) {
            lseek(fd, i, SEEK_SET);

            clock_gettime(CLOCK_REALTIME, &start);
            sz = read(fd, &buf, rd_size);
            clock_gettime(CLOCK_REALTIME, &end);

            tmp_nsec = time_diff_ns(&start, &end);

            // Since not sure how it's being translated, hence split it up
            // (TODO)
            avg_res_userspace[res_buf_idx] += tmp_nsec;

            if (unlikely(tmp_nsec < 0))  // the macro has no help for perf
                                         // improvement essentially
                goto timeout;

            avg_res_kernelspace[res_buf_idx] += buf;

            avg_trans_time[res_buf_idx] +=
                tmp_nsec - buf;  // Transmission time = total time elapsed
                                 // (whole syscall) - real calculation time

            res_buf_idx++;

            printf("Reading from " FIB_DEV
                   " at offset %d, returned the sequence "
                   "%lld.\n",
                   i, sz);
        }
    }

    // avg. time spend calculation for each fabonacci, then write to result file
    for (j = 0; j < FABONACII_INPUT; j++) {
        avg_res_userspace[j] /= TEST_COUNT;
        avg_res_kernelspace[j] /= TEST_COUNT;
        avg_trans_time[j] /= TEST_COUNT;
        fprintf(fp_perf, "%d %ld %ld %ld\n", j, avg_res_userspace[j],
                avg_res_kernelspace[j], avg_trans_time[j]);
    }

    fclose(fp_perf); /* FIX: forgot to close previous fd */
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
