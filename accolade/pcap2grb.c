#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <malloc.h>
#include <zlib.h>
#include <pcap.h>
#include <GraphBLAS.h>
#include "cryptopANT.h"

#define WINDOWSIZE (1 << 23)
#define SUBWINSIZE (1 << 17)

struct px3_state
{
    struct tm *f_tm;
    unsigned int anonymize;
    unsigned int records_per_file;
    unsigned int rec;
    uint64_t total_packets, total_skipped, total_tagged, total_v6;
    char *out_prefix;
    char f_name[1024];
};

struct _serialized_blob
{
    void *blob_data;
    GrB_Index blob_size;
};

struct posix_tar_header
{                       /* byte offset */
    char name[100];     /*   0 */
    char mode[8];       /* 100 */
    char uid[8];        /* 108 */
    char gid[8];        /* 116 */
    char size[12];      /* 124 */
    char mtime[12];     /* 136 */
    char chksum[8];     /* 148 */
    char typeflag;      /* 156 */
    char linkname[100]; /* 157 */
    char magic[6];      /* 257 */
    char version[2];    /* 263 */
    char uname[32];     /* 265 */
    char gname[32];     /* 297 */
    char devmajor[8];   /* 329 */
    char devminor[8];   /* 337 */
    char prefix[155];   /* 345 */
                        /* 500 */
    char padding1[12];  /* 512 */
};

#define TMAGIC "ustar" /* ustar and a null */
#define TMAGLEN 6
#define TVERSION "00" /* 00 and no null */
#define TVERSLEN 2

struct px3_state *pstate; // global state

// If at first you don't succeed, just abort.
#define LAGRAPH_TRY_EXIT(method)                                                                   \
    {                                                                                              \
        GrB_Info info = (method);                                                                  \
        if (!(info == GrB_SUCCESS))                                                                \
        {                                                                                          \
            fprintf(stderr, "LAGraph error: [%d]\nFile: %s Line: %d\n", info, __FILE__, __LINE__); \
            exit(5);                                                                               \
        }                                                                                          \
    }

// Convenience macros to time code sections.
#ifndef NDEBUG
#define TIC(clocktype, msg)                                                      \
    {                                                                            \
        double t_ts;                                                             \
        pid_t _mytid = syscall(SYS_gettid);                                      \
        clock_gettime(clocktype, &ts_start);                                     \
        t_ts = ((ts_start.tv_sec * 1e9) + (ts_start.tv_nsec)) * 1e-9;            \
        fprintf(stderr, "[%d] [%.2f] [%s] Section begin.\n", _mytid, t_ts, msg); \
    }

#define TOC(clocktype, msg)                                                                \
    {                                                                                      \
        struct timespec ts_end, tsdiff;                                                    \
        double t_ts;                                                                       \
        pid_t _mytid = syscall(SYS_gettid);                                                \
        clock_gettime(clocktype, &ts_end);                                                 \
        timespec_diff(&ts_end, &ts_start, &tsdiff);                                        \
        t_ts = ((ts_start.tv_sec * 1e9) + (ts_start.tv_nsec)) * 1e-9;                      \
        t_elapsed = ((tsdiff.tv_sec * 1e9) + (tsdiff.tv_nsec)) * 1e-9;                     \
        fprintf(stderr, "[%d] [%.2f] [%s] elapsed %.2fs\n", _mytid, t_ts, msg, t_elapsed); \
    }
#else
#define TIC(clocktype, msg)
#define TOC(clocktype, msg)
#endif

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

void usage(const char *name)
{
    printf("usage: %s [-a anonymize.key] -i INPUT_FILE -o OUTPUT_DIRECTORY\n", name);
}

int main(int argc, char *argv[])
{
    FILE *in;
    char in_f[PATH_MAX], anonkey[PATH_MAX];
    pcap_t *pcap;
    char errbuf[PCAP_ERRBUF_SIZE];
    char *value = NULL;      // for getopt
    int c, ret, reqargs = 0; // for getopt
    size_t filesize, filepos;
    struct timespec ts_start;
    double t_elapsed = 0;

    const uint8_t *buf_p;
    struct pcap_pkthdr *hdr_p;

    pstate = calloc(1, sizeof(struct px3_state));
    pstate->f_tm = NULL;
    pstate->records_per_file = WINDOWSIZE;

    while ((c = getopt(argc, argv, "a:i:o:")) != -1)
    {
        switch (c)
        {
        case 'a':
            value = optarg;
            pstate->anonymize = 1;
            snprintf(anonkey, sizeof(anonkey), "%s", value);
            break;
        case 'i':
            // input
            value = optarg;
            reqargs++;
            snprintf(in_f, sizeof(in_f), "%s", value);
            break;
        case 'o':
            // output dir
            value = optarg;
            reqargs++;
            pstate->out_prefix = strdup(value);
            break;
        case '?':
            if (optopt == 'i' || optopt == 'o' || optopt == 'a')
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
            exit(2);
        }
    }

    if (reqargs < 2)
    {
        fprintf(stderr, "Invalid or insufficient arguments.\n");
        usage(argv[0]);
        exit(1);
    }

    if (pstate->anonymize != 0)
    {
        fprintf(stderr, "anonymizing using scramble keyfile: %s\n", anonkey);
        if (scramble_init_from_file(anonkey, SCRAMBLE_BLOWFISH, SCRAMBLE_BLOWFISH, NULL) < 0)
        {
            fprintf(stderr, "scramble_init_from_file(): nope\n");
            return 1;
        }
    }

    if (!strcmp(in_f, "-"))
    {
        in = stdin;
    }
    else if ((in = fopen(in_f, "r")) == NULL)
    {
        perror("fopen in_f");
        return 1;
    }

    if (in != stdin)
    {
        fseek(in, 0L, SEEK_END);
        filesize = ftell(in);
        fseek(in, 0L, SEEK_SET);
        fprintf(stderr, "pcap file is %ld bytes\n", filesize);
        fflush(stderr);
    }

    if ((pcap = pcap_fopen_offline(in, errbuf)) == NULL)
    {
        fprintf(stderr, "Error in opening pipefd for reading: %s\n", errbuf);
        fflush(stderr);
        return 1;
    }

    GrB_init(GrB_NONBLOCKING);

    GrB_Matrix Gmat;
    GrB_Descriptor desc = NULL;

    GrB_Index *R = malloc(sizeof(GrB_Index) * SUBWINSIZE);
    GrB_Index *C = malloc(sizeof(GrB_Index) * SUBWINSIZE);
    uint32_t *V = malloc(sizeof(uint32_t) * SUBWINSIZE);

    const int blocks_per_file = (WINDOWSIZE / SUBWINSIZE);
    struct _serialized_blob blob_list[blocks_per_file];
    int subblock = 0;

    GrB_Descriptor_new(&desc);
    GxB_Desc_set(desc, GxB_COMPRESSION, GxB_COMPRESSION_LZ4);

    TIC(CLOCK_REALTIME, "pcap begin");

    while ((ret = pcap_next_ex(pcap, &hdr_p, &buf_p)) >= 0)
    {
        pstate->total_packets++;
        const struct ether_header *eth_hdr = (struct ether_header *)buf_p;

        if (ntohs(eth_hdr->ether_type) == ETHERTYPE_IP)
        {
            const struct ip *ip_hdr = (struct ip *)(buf_p + ETH_HLEN);

            if (ip_hdr->ip_v == IPVERSION)
            {
                uint32_t srcip, dstip;

                if (subblock == 0 && pstate->rec == 0)
                {
                    pstate->f_tm = localtime(&hdr_p->ts.tv_sec);
                }

                if (pstate->anonymize != 0)
                {
                    srcip = scramble_ip4(ip_hdr->ip_src.s_addr, 16);
                    dstip = scramble_ip4(ip_hdr->ip_dst.s_addr, 16);
                }
                else
                {
                    srcip = ip_hdr->ip_src.s_addr;
                    dstip = ip_hdr->ip_dst.s_addr;
                }

                if (srcip > UINT_MAX - 1 || dstip > UINT_MAX - 1)
                {
                    continue;
                }

                R[pstate->rec] = srcip;
                C[pstate->rec] = dstip;
                V[pstate->rec] = 1;

                pstate->rec++;
            }
            else
                pstate->total_skipped++;
        }
        else if (ntohs(eth_hdr->ether_type) == ETH_P_8021Q) // quick and dirty, can refactor to avoid duplication
        {
            pstate->total_tagged++;

            if (ntohs(*(uint16_t *)(buf_p + 16)) == ETHERTYPE_IP)
            {
                const struct ip *ip_hdr = (struct ip *)(buf_p + 18);

                if (ip_hdr->ip_v == IPVERSION)
                {
                    uint32_t srcip, dstip;

                    if (subblock == 0 && pstate->rec == 0)
                    {
                        pstate->f_tm = localtime(&hdr_p->ts.tv_sec);
                    }

                    if (pstate->anonymize != 0)
                    {
                        srcip = scramble_ip4(ip_hdr->ip_src.s_addr, 16);
                        dstip = scramble_ip4(ip_hdr->ip_dst.s_addr, 16);
                    }
                    else
                    {
                        srcip = ip_hdr->ip_src.s_addr;
                        dstip = ip_hdr->ip_dst.s_addr;
                    }

                    if (srcip > UINT_MAX - 1 || dstip > UINT_MAX - 1)
                    {
                        continue;
                    }

                    R[pstate->rec] = srcip;
                    C[pstate->rec] = dstip;
                    V[pstate->rec] = 1;

                    pstate->rec++;
                }
                else
                    pstate->total_skipped++;
            }
            else
                pstate->total_skipped++;
        }
        else if (ntohs(eth_hdr->ether_type) == ETH_P_IPV6)
        {
            pstate->total_v6++;
        }

        if (pstate->rec == SUBWINSIZE)
        {
            void *blob = NULL;
            GrB_Index blob_size = 0;

            LAGRAPH_TRY_EXIT(GrB_Matrix_new(&Gmat, GrB_UINT32, UINT_MAX - 1, UINT_MAX - 1));
            LAGRAPH_TRY_EXIT(GrB_Matrix_build(Gmat, R, C, V, SUBWINSIZE, GrB_PLUS_UINT32));

            LAGRAPH_TRY_EXIT(GxB_Matrix_serialize(&blob, &blob_size, Gmat, desc));

            blob_list[subblock].blob_size = blob_size;
            blob_list[subblock].blob_data = blob;

            GrB_free(&Gmat);
            pstate->rec = 0;
            subblock++;
        }

        if (subblock == blocks_per_file)
        {
            size_t offset = 0;
            const unsigned char padblock[512] = {0};
            char f_name[PATH_MAX], tmp_t[64];
            int tarfd;

            strftime(tmp_t, sizeof(tmp_t), "%Y%m%d-%H%M%S", pstate->f_tm);
            snprintf(f_name, sizeof(f_name), "%s/%s.%d.tar", pstate->out_prefix, tmp_t, WINDOWSIZE);

            if ((tarfd = open(f_name, O_CREAT | O_WRONLY | O_TRUNC, 0660)) != -1)
            {
                for (int i = 0; i < blocks_per_file; i++)
                {
                    struct posix_tar_header th = {0}; // let's make a tar file, or close enough

                    const unsigned char *th_ptr = &th;
                    size_t tmp_chksum = 0;
                    size_t aligned;

                    sprintf(th.name, "%d.grb", i);
                    sprintf(th.uid, "%06o ", 0);
                    sprintf(th.gid, "%06o ", 0);
                    sprintf(th.size, "%011o", (unsigned int)blob_list[i].blob_size);
                    sprintf(th.mode, "%06o", 0644);
                    sprintf(th.magic, "%s", TMAGIC);
                    sprintf(th.mtime, "%011o", (unsigned int)time(NULL));
                    th.typeflag = '0';
                    memset(th.chksum, ' ', 8); // or checksum computed below will be wrong!

                    for (int b = 0; b < sizeof(struct posix_tar_header); b++)
                        tmp_chksum += th_ptr[b];

                    sprintf(th.chksum, "%06o ", (unsigned int)tmp_chksum);

                    if (write(tarfd, &th, sizeof(th)) != sizeof(th))
                    {
                        perror("write tar error");
                        exit(4);
                    }

                    if (write(tarfd, blob_list[i].blob_data, blob_list[i].blob_size) != blob_list[i].blob_size)
                    {
                        perror("write tar error");
                        exit(4);
                    }

                    offset += sizeof(th) + blob_list[i].blob_size;
                    aligned = offset % 512; // pad output file to 512 byte alignment
                    if (aligned != 0)
                    {
                        if (write(tarfd, padblock, 512 - aligned) != 512 - aligned)
                        {
                            perror("write tar error");
                            exit(4);
                        }
                        offset += 512 - aligned;
                    }
                    free(blob_list[i].blob_data);
                }
                close(tarfd);
            }
            else
            {
                perror("open");
                exit(1);
            }

            subblock = 0;
        }
    }
    free(R);
    free(C);
    free(V);
    TOC(CLOCK_REALTIME, "pcap process");

    if (ret == -1) // 0 = packet retrieved, -1 = read error, -2 = successful EOF
    {
        fprintf(stderr, "pcap_next_ex error: %s\n", pcap_geterr(pcap));
    }

    filepos = ftell(in);
    fprintf(stderr, "Done: %ld packets.  (tagged: %ld, v6: %ld, skipped: %ld) Filepos: %ld/%ld (%.2f pps)\n", pstate->total_packets, pstate->total_tagged, pstate->total_v6, pstate->total_skipped, filepos, filesize, pstate->total_packets / t_elapsed);
    fclose(in);
    exit(0);
}
