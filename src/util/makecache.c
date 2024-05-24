#define _GNU_SOURCE

#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <pcap/pcap.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* see README */
#include "common.h"
#include "cryptopANT.h"

void usage(const char *name)
{
    fprintf(stdout, "usage: %s -a <path to cryptoPAN anonymization key> -o <path to output ip4 cache file>\n", name);
    fprintf(stdout, "       If CryptoPAN anonymization keyfile does not exist, a random key will be generated and saved.\n");
    fprintf(stdout, "       Output is a 16GB anonymization table.\n");
}

int main(int argc, char **argv)
{
    struct timespec ts_start;
    uint32_t i;
    uint32_t *ipc;
    size_t ret;
    char *output_file = NULL;
    FILE *fp;
    int c, reqargs = 0;
    char anonkey[PATH_MAX] = { 0 }, prefix[128] = { 0 };

    scramble_crypt_t key_crypto = SCRAMBLE_BLOWFISH;

    while ((c = getopt(argc, argv, "a:o:")) != -1)
    {
        switch (c)
        {
            case 'a':
                reqargs++;
                snprintf(anonkey, sizeof(anonkey), "%s", optarg);
                break;
            case 'o':
                // output dir
                reqargs++;
                output_file = strdup(optarg);
                break;
            case '?':
                if (optopt == 'a' || optopt == 'o')
                {
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                }
                else if (isprint(optopt))
                {
                    fprintf(stderr, "Unknown option: -%c.\n", optopt);
                }
                else
                {
                    fprintf(stderr, "Unrecognized option: -%c.\n", optopt);
                }
                usage(argv[0]);
                exit(1);
            default:
                usage(argv[0]);
                exit(2);
        }
    }

    if (reqargs < 2)
    {
        fprintf(stderr, "Invalid or insufficient arguments.\n");
        usage(argv[0]);
        exit(1);
    }

    fprintf(stderr, "using cryptoPAN anonymization key: %s\n", anonkey);
    if (scramble_init_from_file(anonkey, key_crypto, key_crypto, NULL) < 0)
    {
        fprintf(stderr, "error in scramble_init_from_file()\n");
        return 1;
    }

    const size_t cachesize = sizeof(uint32_t) * (UINT_MAX - 1);

    fprintf(stderr, "Allocating ip4cache of size %ld\n", cachesize);
    ipc = malloc(cachesize);

    if ((fp = fopen(output_file, "w")) == NULL)
    {
        perror("fopen");
        exit(0);
    }

    TIC(CLOCK_REALTIME, "random IP cache creation");
    for (i = 0; i < UINT_MAX - 1; i++)
    {
        if (i % 10000000 == 0)
        { // this takes a while, provide some periodic status output
            fprintf(stderr, "Generating, i=%u\n", i);
        }
        ipc[i] = scramble_ip4(i, 16); // preserve 16 upper bits
    }
    TOC(CLOCK_REALTIME, "random IP cache creation");

    if ((ret = fwrite(ipc, cachesize, 1, fp)) < 1)
    {
        perror("write");
    }
    fclose(fp);
    free(ipc);
}
