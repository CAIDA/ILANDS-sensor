#define _GNU_SOURCE 1

#include <GraphBLAS.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <malloc.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <pcap.h>
#include <pcap/pcap.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#include "cryptopANT.h"

// Default
#define SUBWINSIZE (1 << 17) // 131072

#define BSWAP(a)   (pstate->swapped ? ntohl(a) : (a))

struct px3_state
{
    struct tm *f_tm;
    unsigned int save_trailing_packets;
    unsigned int anonymize;
    unsigned int swapped;
    unsigned int rec;
    int link_type;
    uint32_t files_per_window;
    uint32_t subwinsize;
    uint64_t total_packets, total_invalid;
    char *out_prefix;
    char f_name[1024];
    uint32_t findex;
    GrB_Index *R, *C;
    uint32_t *V;
};

/// @brief 512-byte POSIX tar file header.
struct posix_tar_header
{                       /* byte offset */
    char name[100];     /*   0 - filename */
    char mode[8];       /* 100 - octal mode */
    char uid[8];        /* 108 - user ID */
    char gid[8];        /* 116 - group ID */
    char size[12];      /* 124 - file size */
    char mtime[12];     /* 136 - modification time */
    char chksum[8];     /* 148 - checksum */
    char typeflag;      /* 156 - type */
    char linkname[100]; /* 157 - link name */
    char magic[6];      /* 257 - tar magic, USTAR */
    char version[2];    /* 263 - tar version */
    char uname[32];     /* 265 - user name */
    char gname[32];     /* 297 - group name */
    char devmajor[8];   /* 329 - device major */
    char devminor[8];   /* 337 - device minor */
    char prefix[155];   /* 345 - prefix */
                        /* 500 */
    char padding1[12];  /* 512 - unused padding to fill 512 bytes */
};

#define TMAGIC   "ustar" /* ustar and a null */
#define TMAGLEN  6
#define TVERSION "00" /* 00 and no null */
#define TVERSLEN 2

struct px3_state *pstate; // global state

// If at first you don't succeed, just abort.
#define LAGRAPH_TRY_EXIT(method)                                                                                       \
    {                                                                                                                  \
        GrB_Info info = (method);                                                                                      \
        if (!(info == GrB_SUCCESS))                                                                                    \
        {                                                                                                              \
            fprintf(stderr, "LAGraph error: [%d]\nFile: %s Line: %d\n", info, __FILE__, __LINE__);                     \
            exit(5);                                                                                                   \
        }                                                                                                              \
    }

// Convenience macros to time code sections.
#ifndef NDEBUG
#define TIC(clocktype, msg)                                                                                            \
    {                                                                                                                  \
        double t_ts;                                                                                                   \
        pid_t _mytid = syscall(SYS_gettid);                                                                            \
        clock_gettime(clocktype, &ts_start);                                                                           \
        t_ts = ((ts_start.tv_sec * 1e9) + (ts_start.tv_nsec)) * 1e-9;                                                  \
        fprintf(stderr, "[%d] [%.2f] [%s] Section begin.\n", _mytid, t_ts, msg);                                       \
    }

#define TOC(clocktype, msg)                                                                                            \
    {                                                                                                                  \
        struct timespec ts_end, tsdiff;                                                                                \
        double t_ts;                                                                                                   \
        pid_t _mytid = syscall(SYS_gettid);                                                                            \
        clock_gettime(clocktype, &ts_end);                                                                             \
        timespec_diff(&ts_end, &ts_start, &tsdiff);                                                                    \
        t_ts      = ((ts_start.tv_sec * 1e9) + (ts_start.tv_nsec)) * 1e-9;                                             \
        t_elapsed = ((tsdiff.tv_sec * 1e9) + (tsdiff.tv_nsec)) * 1e-9;                                                 \
        fprintf(stderr, "[%d] [%.2f] [%s] elapsed %.2fs\n", _mytid, t_ts, msg, t_elapsed);                             \
    }
#else
#define TIC(clocktype, msg)
#define TOC(clocktype, msg)
#endif

/// @brief Calculate the delta time (as a struct timespec) between two struct timespec.
/// @param a Time #1
/// @param b Time #2
/// @param result struct timespec containing the elapsed time between them.
static inline void timespec_diff(struct timespec *a, struct timespec *b, struct timespec *result)
{
    result->tv_sec  = a->tv_sec - b->tv_sec;
    result->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (result->tv_nsec < 0)
    {
        --result->tv_sec;
        result->tv_nsec += 1000000000L;
    }
}

void usage(const char *name)
{
    fprintf(stderr,
            "usage: %s [-a anonymize.key] [-W FILES_PER_WINDOW] [-w SUBWINSIZE] -i INPUT_FILE -o OUTPUT_DIRECTORY\n",
            name);
    fprintf(stderr, "    -a Anonymize using CryptopANT (https://ant.isi.edu/software/cryptopANT/index.html)\n");
    fprintf(stderr,
            "       If CryptoPAN anonymization keyfile does not exist, a random key will be generated and saved.\n");
    fprintf(stderr, "    -W Number of GraphBLAS matrices to save in the output tar file.\n");
    fprintf(stderr, "    -w Window size (number of entries) in the saved GraphBLAS matrices.\n");
    fprintf(stderr, "    -O Single file mode - one tar file containing one GraphBLAS matrix..\n");
    fprintf(stderr, "    -i Input file (pcap format).\n");
    fprintf(stderr, "    -o Output directory.\n");
}

/// @brief Find the IPv4 header in a packet (look for ETHERTYPE_IP).
/// @param base Beginning of packet Ethernet header. (uint8_t)
/// @return Pointer to the beginning of the IPv4 header, or NULL if not found.
const uint8_t *find_iphdr(const uint8_t *base)
{
    uint16_t ether_type = ntohs(*(uint16_t *)(base + 12));

    if (ether_type == ETHERTYPE_IP) // IP
    {
        return base + 14;
    }
    else if (ether_type == 0x8100) // VLAN
    {
        ether_type = ntohs(*(uint16_t *)(base + 16));

        if (ether_type == ETHERTYPE_IP)
        {
            return base + 18;
        }
    }

    return NULL;
}

void set_output_filename(void)
{
    char timestr[128]   = { 0 };
    uint32_t windowsize = pstate->subwinsize * pstate->files_per_window;

    strftime(timestr, sizeof(timestr), "%Y%m%d-%H%M%S", pstate->f_tm);
    snprintf(pstate->f_name, sizeof(pstate->f_name), "%s/%s.%u.tar", pstate->out_prefix, timestr, windowsize);

    fprintf(stderr, "Setting output filename to %s.\n", pstate->f_name);

    if (access(pstate->f_name, F_OK) == 0)
    {
        fprintf(stderr, "ERR: Output file already exists in set_output_filename: %s\n", pstate->f_name);
        fprintf(stderr, "Not appending new data.\n");
        exit(EXIT_FAILURE);
    }

    pstate->findex = 0;
}

void add_blob_to_tar(void *blob_data, unsigned int blob_size)
{
    int tarfd;
    struct posix_tar_header th  = { 0 }; // let's make a tar file, or close enough
    const unsigned char *th_ptr = (const unsigned char *)&th;
    size_t tmp_chksum           = 0;
    size_t aligned;

    if ((tarfd = open(pstate->f_name, O_CREAT | O_WRONLY | O_APPEND, 0660)) == -1)
    {
        perror("open tar");
        exit(1);
    }

    sprintf(th.name, "%d.grb", pstate->findex);
    sprintf(th.uid, "%06o ", 0);
    sprintf(th.gid, "%06o ", 0);
    sprintf(th.size, "%011o", blob_size);
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

    if (write(tarfd, blob_data, blob_size) != blob_size)
    {
        perror("write tar error");
        exit(4);
    }

    aligned = (sizeof(th) + blob_size) % 512; // pad output file to 512 byte alignment
    if (aligned != 0)
    {
        const unsigned char padblock[512] = { 0 };

        if (write(tarfd, padblock, 512 - aligned) != 512 - aligned)
        {
            perror("write tar error");
            exit(4);
        }
    }

    close(tarfd);

    pstate->findex++;

    if (pstate->findex >= pstate->files_per_window && !(pstate->findex > 0 && pstate->files_per_window == 1))
    {
        set_output_filename();
    }
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

    pstate                   = calloc(1, sizeof(struct px3_state));
    pstate->f_tm             = NULL;
    pstate->subwinsize       = SUBWINSIZE; // 131072
    pstate->files_per_window = 64;         // 64 x 131072 = 8388608 (default)

    while ((c = getopt(argc, argv, "SOva:i:o:w:W:")) != -1)
    {
        switch (c)
        {
            case 'a':
                value             = optarg;
                pstate->anonymize = 1;
                snprintf(anonkey, sizeof(anonkey), "%s", value);
                break;
            case 'i':
                // input
                value = optarg;
                reqargs++;
                snprintf(in_f, sizeof(in_f), "%s", value);
                break;
            case 'S':
                pstate->swapped = 1;
                break;
            case 'o':
                // output dir
                value = optarg;
                reqargs++;
                pstate->out_prefix = strdup(value);
                break;
            case 'O':
                pstate->files_per_window      = 1;
                pstate->subwinsize            = UINT_MAX;
                pstate->save_trailing_packets = 1;
                break;
            case 'W':
                // files per window
                if (sscanf(optarg, "%u", &pstate->files_per_window) != 1)
                {
                    fprintf(stderr, "Invalid argument to option -%c.\n", optopt);
                    exit(EXIT_FAILURE);
                }
                if (pstate->files_per_window <= 0)
                {
                    fprintf(stderr, "Invalid value for window size: %u\n", pstate->files_per_window);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'w':
                // subwinsize
                if (sscanf(optarg, "%u", &pstate->subwinsize) != 1)
                {
                    fprintf(stderr, "Invalid argument to option -%c.\n", optopt);
                    exit(EXIT_FAILURE);
                }
                if (pstate->subwinsize <= 0)
                {
                    fprintf(stderr, "Invalid value for subwindow size: %u\n", pstate->subwinsize);
                    exit(EXIT_FAILURE);
                }
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
        fprintf(stderr, "input pcap file is %ld bytes\n", filesize);
        fflush(stderr);
    }

    if ((pcap = pcap_fopen_offline(in, errbuf)) == NULL)
    {
        fprintf(stderr, "Error in opening pipefd for reading: %s\n", errbuf);
        fflush(stderr);
        return 1;
    }

    pstate->link_type = pcap_datalink(pcap);
    fprintf(stderr, "input pcap file is of type %s\n", pcap_datalink_val_to_name(pstate->link_type));
    fprintf(stderr, "Generating tar files with %u matrices of size: %u\n", pstate->files_per_window,
            pstate->subwinsize);

    GrB_init(GrB_NONBLOCKING);

    GrB_Matrix Gmat;
    GrB_Descriptor desc = NULL;

    pstate->R = malloc(sizeof(GrB_Index) * pstate->subwinsize);
    pstate->C = malloc(sizeof(GrB_Index) * pstate->subwinsize);
    pstate->V = malloc(sizeof(uint32_t) * pstate->subwinsize);

    GrB_Descriptor_new(&desc);
    GxB_Desc_set(desc, GxB_COMPRESSION, GxB_COMPRESSION_ZSTD + 1);

    TIC(CLOCK_REALTIME, "pcap begin");

    while ((ret = pcap_next_ex(pcap, &hdr_p, &buf_p)) >= 0)
    {
        pstate->total_packets++;
        struct ip *ip_hdr = NULL;

        if (pstate->link_type == DLT_EN10MB)
        {
            const struct ether_header *eth_hdr = (struct ether_header *)buf_p;
            ip_hdr                             = (struct ip *)find_iphdr(buf_p);
        }
        else if (pstate->link_type == DLT_RAW)
        {
            ip_hdr = (struct ip *)buf_p;
        }

        if (ip_hdr == NULL) // Not ETHERTYPE_IP.
        {
            continue;
        }

        if (ip_hdr->ip_v == IPVERSION)
        {
            uint32_t srcip, dstip;

            if (pstate->findex == 0 && pstate->rec == 0)
            {
                if (pstate->f_tm == NULL)
                {
                    pstate->f_tm = localtime(&hdr_p->ts.tv_sec);
                    set_output_filename();
                }
                else
                {
                    pstate->f_tm = localtime(&hdr_p->ts.tv_sec);
                }
            }

            if (pstate->anonymize != 0)
            {
                srcip = scramble_ip4(BSWAP(ip_hdr->ip_src.s_addr), 16);
                dstip = scramble_ip4(BSWAP(ip_hdr->ip_dst.s_addr), 16);
            }
            else
            {
                srcip = BSWAP(ip_hdr->ip_src.s_addr);
                dstip = BSWAP(ip_hdr->ip_dst.s_addr);
            }

            if (srcip > UINT_MAX - 1 || dstip > UINT_MAX - 1) // Global broadcasts.
            {
                continue;
            }

            pstate->R[pstate->rec] = srcip;
            pstate->C[pstate->rec] = dstip;
            pstate->V[pstate->rec] = 1;

            pstate->rec++;
        }
        else
            pstate->total_invalid++;

        if (pstate->rec == pstate->subwinsize)
        {
            void *blob          = NULL;
            GrB_Index blob_size = 0;

            LAGRAPH_TRY_EXIT(GrB_Matrix_new(&Gmat, GrB_UINT32, 4294967296, 4294967296));
            LAGRAPH_TRY_EXIT(
                GrB_Matrix_build(Gmat, pstate->R, pstate->C, pstate->V, pstate->subwinsize, GrB_PLUS_UINT32));
            LAGRAPH_TRY_EXIT(GxB_Matrix_serialize(&blob, &blob_size, Gmat, desc));

            add_blob_to_tar(blob, blob_size);
            free(blob);

            GrB_free(&Gmat);
            pstate->rec = 0;
        }
    }

    TOC(CLOCK_REALTIME, "pcap process");

    if (ret == -1) // 0 = packet retrieved, -1 = read error, -2 = successful EOF
    {
        fprintf(stderr, "pcap_next_ex error: %s\n", pcap_geterr(pcap));
    }

    if (pstate->rec > 0)
    {
        if (pstate->save_trailing_packets != 0)
        {
            void *blob          = NULL;
            GrB_Index blob_size = 0;

            LAGRAPH_TRY_EXIT(GrB_Matrix_new(&Gmat, GrB_UINT32, 4294967296, 4294967296));
            LAGRAPH_TRY_EXIT(GrB_Matrix_build(Gmat, pstate->R, pstate->C, pstate->V, pstate->rec, GrB_PLUS_UINT32));
            LAGRAPH_TRY_EXIT(GxB_Matrix_serialize(&blob, &blob_size, Gmat, desc));

            fprintf(stderr, "Adding trailing %u packets to tar file.\n", pstate->rec);

            add_blob_to_tar(blob, blob_size);
            free(blob);

            GrB_free(&Gmat);
        }
        else
        {
            fprintf(stderr, "INFO: Not processing %u remaining packets (less than matrix size of %u).\n", pstate->rec,
                    pstate->subwinsize);
        }
    }

    filepos = ftell(in);
    fprintf(stderr, "Done: %ld packets.  (%.2f pps)\n", pstate->total_packets, pstate->total_packets / t_elapsed);
    fclose(in);
    free(pstate->R);
    free(pstate->C);
    free(pstate->V);
    exit(0);
}
