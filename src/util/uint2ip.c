#define _GNU_SOURCE 1
#include <arpa/inet.h>
#include <ctype.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

void usage(const char *name)
{
    fprintf(stderr, "usage: %s [-S] <uint32>\n", name);
    fprintf(stderr, "    -S Interpret as big-endian (e.g. in_addr.s_addr)\n");
}

int main(int argc, char *argv[])
{
    char *value = NULL;      // for getopt
    int c, ret, reqargs = 0; // for getopt
    int swapped = 0;

    while ((c = getopt(argc, argv, "S")) != -1)
    {
        switch (c)
        {
            case 'S':
                swapped = 1;
                break;
            case '?':
                if (isprint(optopt))
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
                exit(2);
        }
    }

    if ((argc - optind) < 1)
    {
        usage(argv[0]);
        exit(1);
    }

    uint8_t octet[4];
    uint32_t ip = atoi(argv[optind]);

    ip = swapped ? htonl(ip) : ip;

    for (int i = 0; i < 4; i++)
    {
        octet[i] = ip >> (i * 8);
    }
    fprintf(stdout, "%d.%d.%d.%d\n", octet[3], octet[2], octet[1], octet[0]);
}
