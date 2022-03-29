#include <getopt.h>
#include <pcap/pcap.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

/* see README */
#include "cryptopANT.h"

/* hardcoded paths for testing */
#define ANONKEYFILE "anon.key"
#define CACHE_PATH  "/data/ipcache_file2"

/* helper function borrowed from LAGraph */
#define LAGRAPH_TRY_EXIT(method)                                                                   \
    {                                                                                              \
        GrB_Info info = (method);                                                                  \
        if (!(info == GrB_SUCCESS))                                                                \
        {                                                                                          \
            fprintf(stderr, "LAGraph error: [%d]\nFile: %s Line: %d\n", info, __FILE__, __LINE__); \
            pthread_exit(NULL);                                                                    \
        }                                                                                          \
    }

#define TIC(clocktype)                       \
    {                                        \
        clock_gettime(clocktype, &ts_start); \
    }

#define TOC(clocktype, msg)                                            \
    {                                                                  \
        double t_elapsed;                                              \
        struct timespec ts_end, tsdiff;                                \
        clock_gettime(clocktype, &ts_end);                             \
        timespec_diff(&ts_end, &ts_start, &tsdiff);                    \
        t_elapsed = ((tsdiff.tv_sec * 1e9) + (tsdiff.tv_nsec)) * 1e-9; \
        fprintf(stderr, "[%s] elapsed %.2fs\n", msg, t_elapsed);       \
    }

static inline void timespec_diff(struct timespec *a, struct timespec *b, struct timespec *result)
{
    result->tv_sec = a->tv_sec - b->tv_sec;
    result->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (result->tv_nsec < 0)
    {
        --result->tv_sec;
        result->tv_nsec += 1000000000L;
    }
}

int main(int argc, char **argv)
{
    struct timespec ts_start;
    double elapsed_wait, elapsed_write, elapsed_clear;
    uint32_t tmp, i;
    uint32_t *ipc;
    int fd;
    size_t ret;
    FILE *fp;

    scramble_crypt_t key_crypto = SCRAMBLE_BLOWFISH;
    const char *keyfile = ANONKEYFILE;

    fprintf(stderr, "scramble keyfile: %s\n", keyfile);
    if (scramble_init_from_file(keyfile, key_crypto, key_crypto, NULL) < 0)
    {
        fprintf(stderr, "scramble_init_from_file(): nope\n");
        return 1;
    }

    const size_t cachesize = sizeof(uint32_t) * (UINT_MAX - 1);

    fprintf(stderr, "allocating ip4cache of size %ld\n", cachesize);
    ipc = malloc(cachesize);

    TIC(CLOCK_REALTIME);
    for (i = 0; i < UINT_MAX - 1; i++)
    {
        if (i % 10000000 == 0)
        { // this takes a while, provide some periodic status output
            fprintf(stderr, "Generating, i=%u\n", i);
        }
        ipc[i] = scramble_ip4(i, 16); // preserve 16 upper bits
    }
    TOC(CLOCK_REALTIME, "random IP cache creation");
    
    if ((fp = fopen(CACHE_PATH, "w")) == NULL)
    {
        perror("fopen");
        exit(0);
    }

    if ((ret = fwrite(ipc, cachesize, 1, fp)) < 1)
    {
        perror("write");
    }
    fclose(fp);
    free(ipc);
}
