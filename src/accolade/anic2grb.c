#define _GNU_SOURCE

#include <anic_api.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <getopt.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <pcap/pcap.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "cryptopANT.h"
#include <GraphBLAS.h>

#define WINDOWSIZE  (1 << 23)
#define SUBWINSIZE  (1 << 17)
#define OUTPUT_PATH "/out"

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

#define TMAGIC   "ustar" /* ustar and a null */
#define TMAGLEN  6
#define TVERSION "00" /* 00 and no null */
#define TVERSLEN 2

struct accolade_worker_args
{
    int ring_id;
    int anic_id;
    int ring_count;
    int block_count;
    int block_quota;
    int cache;
};

struct graphblas_worker_args
{
    uint32_t *pktbuf;
    struct tm *t;
    int ring_id;
};

struct _serialized_blob
{
    void *blob_data;
    GrB_Index blob_size;
};

/* globals */
static int numrings = 1;
static char keyfile[PATH_MAX];
static char cachefile[PATH_MAX];
const size_t cachesize = sizeof(uint32_t) * (UINT_MAX - 1);
uint32_t *ip4cache     = NULL;

// If at first you don't succeed, just abort.
#define LAGRAPH_TRY_EXIT(method)                                                                                       \
    {                                                                                                                  \
        GrB_Info info = (method);                                                                                      \
        if (!(info == GrB_SUCCESS))                                                                                    \
        {                                                                                                              \
            fprintf(stderr, "LAGraph error: [%d]\nFile: %s Line: %d\n", info, __FILE__, __LINE__);                     \
            pthread_exit(NULL);                                                                                        \
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
        double t_elapsed;                                                                                              \
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

// Extract IP head length and version.
#define IP_HL(ip) (((ip)->ip_vhl) & 0x0f)
#define IP_V(ip)  (((ip)->ip_vhl) >> 4)

// Calculate time elapsed between two struct timespec, return struct timespec.
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

// helper function to set current thread affinity's to specific single core -- currently unused
static inline int thread_affinity(int core)
{
    int total_cores = sysconf(_SC_NPROCESSORS_ONLN);
    cpu_set_t cpuset;

    if (core < 0 || core >= total_cores)
        return EINVAL;

    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    pthread_t current_thread = pthread_self();
    return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

// function prototypes.
static void *accolade_worker(void *untyped_p);
static void *graphblas_worker(void *untyped_p);
static void *listener(void *untyped_p);
void print_thread_affinity(void);

int main(int argc, char **argv)
{
    // holds arguments for accolade_worker thread.
    struct accolade_worker_args args = {
        .anic_id = -1, .ring_count = -1, .block_count = -1, .block_quota = -11, .cache = 0
    };

    while (1)
    {
        static struct option long_options[] = {
            {"id",      required_argument, 0, 'i'},
            { "mode",   required_argument, 0, 'm'},
            { "blocks", required_argument, 0, 'b'},
            { "quota",  required_argument, 0, 'q'},
            { "cache",  required_argument, 0, 'c'},
            { 0,        0,                 0, 0  }
        };
        int option_index = 0;
        char c;

        if ((c = getopt_long(argc, argv, "a:c:i:m:b:q:", long_options, &option_index)) == -1)
            break;

        switch (c)
        {
            case 'c':
                snprintf(cachefile, sizeof(cachefile) - 1, "%s", optarg);
                args.cache = 1;
                break;
            case 'i':
                args.anic_id = atoi(optarg);
                break;
            case 'm':
                args.ring_count = atoi(optarg);
                numrings        = args.ring_count;
                break;
            case 'b':
                args.block_count = atoi(optarg);
                break;
            case 'q':
                args.block_quota = atoi(optarg);
                break;
            case '?':
                /* getopt_long already printed an error message. */
                break;
            default:
                abort();
        }
    }

    if (args.cache == 1)
    {
        FILE *fp;
        size_t filesize = 0, ret = 0;
        struct timespec ts_start;

        fprintf(stderr, "loading anonymization table: %s\n", cachefile);
        ip4cache = malloc(cachesize);

        TIC(CLOCK_MONOTONIC, "table load");
        if ((fp = fopen(cachefile, "rb")) == NULL)
        {
            perror("fopen");
            return 1;
        }

        fseek(fp, 0L, SEEK_END);
        filesize = ftell(fp);
        fseek(fp, 0L, SEEK_SET);
        fprintf(stderr, "table is %ld bytes\n", filesize);

        if ((ret = fread(ip4cache, filesize, 1, fp)) < 1)
        {
            perror("fread");
            return 1;
        }

        fclose(fp);
        TOC(CLOCK_MONOTONIC, "table load");
    }

    // launch anic worker threads, one per hardware ring
    int i;
    struct accolade_worker_args *args_p;
    pthread_t thread;

    pthread_create(&thread, NULL, listener, NULL);

    for (i = 0; i < ANIC_MAX_NUMBER_OF_RINGS; i++)
    {
        if (i < args.ring_count || i == 63)
        {
            args_p = malloc(sizeof(*args_p));
            if (args_p == NULL)
                abort();

            *args_p         = args;
            args_p->ring_id = i;

            pthread_create(&thread, NULL, accolade_worker, args_p);
        }
    }

    // main thread does nothing
    while (1)
        sleep(1);
}

// main anic worker thread.  retrieves packets from anic ring buffer, extracts src/dst IP addresses
// (ipv4 only) and builds list for GraphBLAS matrix creation.
static void *accolade_worker(void *untyped_p)
{
    struct accolade_worker_args *args_p = (struct accolade_worker_args *)untyped_p;
    const int max_source                = 256;
    char source[max_source];
    const int buffer_size = (sizeof(uint32_t) * WINDOWSIZE) * 2;
    uint32_t *pktbuf, *bufptr;
    int i;

    // parent thread doesn't look for termination
    pthread_detach(pthread_self());

    // build argument string
    i = snprintf(source, max_source, "anic --ring %u", args_p->ring_id);
    if (args_p->anic_id >= 0)
        i += snprintf(&source[i], max_source - i, " --id %u", args_p->anic_id);
    if (args_p->ring_count >= 0)
        i += snprintf(&source[i], max_source - i, " --mode %u", args_p->ring_count);
    if (args_p->block_count >= 0)
        i += snprintf(&source[i], max_source - i, " --blocks %u", args_p->block_count);
    if (args_p->block_quota >= 0)
        i += snprintf(&source[i], max_source - i, " --quota %u", args_p->block_count);

    // hacky, pin each hardware ring worker thread to the cpu corresponding to its ring id
    // thread_affinity(args_p->ring_id);

    // create libpcap handle
    pcap_t *pcap_p;
    char errbuf[PCAP_ERRBUF_SIZE];
    int rtn;

    pcap_p = pcap_create(source, errbuf);

    if (pcap_p == NULL)
    {
        fprintf(stderr, "pcap_create(\"%s\") failed: %s\n", source, errbuf);
        abort();
    }

    rtn = pcap_activate(pcap_p);

    if (rtn)
    {
        fprintf(stderr, "pcap_activate for ring %u failed: %s\n", args_p->ring_id, pcap_statustostr(rtn));
        return NULL;
    }

    // Allocate memory for packet buffer.
    if ((pktbuf = malloc(buffer_size)) == NULL) // WINDOWSIZE ip address pairs
    {
        perror("malloc");
        return NULL;
    }
    bufptr = pktbuf;

    const uint8_t *buf_p;
    struct pcap_pkthdr *hdr_p;
    unsigned prev_sec    = 0;
    unsigned elapsed_sec = 0;
    unsigned pkts = 0, tpkts = 0;
    unsigned infile = 0;
    long bytes      = 0;
    struct pcap_stat stats;
    struct pcap_stat pstats;

    memset(&pstats, 0, sizeof(pstats));

    // main loop; read packets serially using Accolade-provided anic_libpcap interface and store them in GrB matrix.
    fprintf(stderr, "thread READY: %d\n", args_p->ring_id);
    while (1)
    {
        rtn = pcap_next_ex(pcap_p, &hdr_p, &buf_p);

        // no packets available in the ring buffer, wait.
        if (rtn == 0)
        {
            usleep(100);
            continue;
        }

        // Non-IP packets go to ring 63, we ignore them for now.
        if (args_p->ring_id != 63)
        {
            const struct ether_header *eth_hdr = (struct ether_header *)buf_p;

            if (ntohs(eth_hdr->ether_type) == ETHERTYPE_IP)
            {
                const struct ip *ip_hdr = (struct ip *)(buf_p + ETH_HLEN);

                if (ip_hdr->ip_v == IPVERSION)
                {
                    uint32_t srcip = 0, dstip = 0;

                    memcpy(&srcip, &(ip_hdr->ip_src), 4);
                    memcpy(&dstip, &(ip_hdr->ip_dst), 4);

                    if (srcip > UINT_MAX - 1 || dstip > UINT_MAX - 1)
                        continue;

                    *bufptr++ = srcip;
                    *bufptr++ = dstip;

                    infile++;
                }
            }
            else if (ntohs(eth_hdr->ether_type) == ETH_P_8021Q) // handle vlan tagged packets.
            {
                if (ntohs(*(uint16_t *)(buf_p + 16)) == ETHERTYPE_IP)
                {
                    const struct ip *ip_hdr = (struct ip *)(buf_p + 18);
                    if (ip_hdr->ip_v == IPVERSION)
                    {
                        uint32_t srcip = 0, dstip = 0;

                        memcpy(&srcip, &(ip_hdr->ip_src), 4);
                        memcpy(&dstip, &(ip_hdr->ip_dst), 4);

                        if (srcip > UINT_MAX - 1 || dstip > UINT_MAX - 1)
                            continue;

                        *bufptr++ = srcip;
                        *bufptr++ = dstip;

                        infile++;
                    }
                }
            }

            // we have WINDOWSIZE packets
            if (infile == WINDOWSIZE)
            {
                pthread_t thread;
                struct graphblas_worker_args *gb_args_p = malloc(sizeof(*gb_args_p));

                gb_args_p->pktbuf  = pktbuf;
                gb_args_p->t       = localtime(&hdr_p->ts.tv_sec);
                gb_args_p->ring_id = args_p->ring_id;

                // spawn new thread to graphblas_worker on full packet buffer (nnz=WINDOWSIZE)
                pthread_create(&thread, NULL, graphblas_worker, gb_args_p);

                // create new packet processing buffer and continue..
                if ((pktbuf = malloc(buffer_size)) == NULL)
                {
                    perror("malloc");
                    return NULL;
                }
                bufptr = pktbuf;
                infile = 0;
            }
        }

        // emit stats every second; print number of dropped packets.
        if (hdr_p->ts.tv_sec != prev_sec)
        {
            unsigned dropped = 0;

            prev_sec = hdr_p->ts.tv_sec;
            pcap_stats(pcap_p, &stats);

            dropped = (stats.ps_ifdrop - pstats.ps_ifdrop) + (stats.ps_drop - pstats.ps_drop);
            tpkts += pkts;

            if (args_p->ring_id != 63) // non-IP packets go to ring 63
            {
                fprintf(stderr, "%4u,%2u,%11u,%11u,%11u,%11u%s\n", elapsed_sec++, args_p->ring_id, pkts, tpkts, dropped,
                        (stats.ps_ifdrop + stats.ps_drop), (dropped > 0 ? " D" : ""));
            }

            pkts   = 0;
            bytes  = 0;
            pstats = stats;
        }

        pkts++;
        bytes += hdr_p->caplen;
    }
}

void print_thread_affinity(void)
{
    int s;
    cpu_set_t cpuset;
    s = pthread_getaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    if (s != 0)
    {
        perror("pthread_getaffinity_np");
        return;
    }

    fprintf(stderr, "Runnable CPU set:\n");
    fprintf(stderr, "CPUs: ");
    for (int j = 0; j < CPU_SETSIZE; j++)
        if (CPU_ISSET(j, &cpuset))
            fprintf(stderr, "%d,", j);
    fprintf(stderr, "\n");
}

// thread for serializing GrB_Matrix to avoid blocking packet processing
// takes packed uint32_t vector containing 2^23 packets and spits out tar
// file containing 64 x 2^17 serialized GraphBLAS matrices.
// Ensure env variable OMP_NUM_THREADS=1 or will perform poorly.
static void *graphblas_worker(void *untyped_p)
{
    struct tm *t     = ((struct graphblas_worker_args *)untyped_p)->t;
    uint32_t *pktbuf = ((struct graphblas_worker_args *)untyped_p)->pktbuf;
    uint32_t *bufptr = pktbuf;
    int ring_id      = ((struct graphblas_worker_args *)untyped_p)->ring_id;

    const int blocks_per_file = (WINDOWSIZE / SUBWINSIZE);
    struct _serialized_blob blob_list[blocks_per_file];

    const int windowsize = WINDOWSIZE;
    pid_t thread_id      = syscall(SYS_gettid);

    struct timespec ts_start;

    char tmp_t[64], f_name[PATH_MAX];

    // parent thread doesn't look for termination here either
    pthread_detach(pthread_self());
    GrB_init(GrB_NONBLOCKING);

    TIC(CLOCK_MONOTONIC, "GraphBLAS");

    // row, column and value vectors
    GrB_Index *R = malloc(sizeof(GrB_Index) * SUBWINSIZE);
    GrB_Index *C = malloc(sizeof(GrB_Index) * SUBWINSIZE);
    uint32_t *V  = malloc(sizeof(uint32_t) * SUBWINSIZE);

    for (int subblock = 0; subblock < (WINDOWSIZE / SUBWINSIZE); subblock++)
    {
        GrB_Matrix Gmat;

        void *blob          = NULL;
        GrB_Index blob_size = 0;

        LAGRAPH_TRY_EXIT(GrB_Matrix_new(&Gmat, GrB_UINT32, UINT_MAX - 1, UINT_MAX - 1));

        for (int i = 0; i < SUBWINSIZE; i++)
        {
            uint32_t srcip = (ip4cache == NULL ? *bufptr++ : ip4cache[*bufptr++]);
            uint32_t dstip = (ip4cache == NULL ? *bufptr++ : ip4cache[*bufptr++]);

            R[i] = srcip;
            C[i] = dstip;
            V[i] = 1;
        }

        LAGRAPH_TRY_EXIT(GrB_Matrix_build(Gmat, R, C, V, SUBWINSIZE, GrB_PLUS_UINT32));

        LAGRAPH_TRY_EXIT(GrB_Matrix_serializeSize(&blob_size, Gmat));
        blob = malloc(blob_size);
        LAGRAPH_TRY_EXIT(GrB_Matrix_serialize(blob, &blob_size, Gmat));

        blob_list[subblock].blob_size = blob_size;
        blob_list[subblock].blob_data = blob;

        GrB_free(&Gmat);
    }
    free(R);
    free(C);
    free(V);
    TOC(CLOCK_MONOTONIC, "GraphBLAS");
    free(pktbuf);

#define SAVETHEBLOB
#ifdef SAVETHEBLOB
    strftime(tmp_t, sizeof(tmp_t), "%Y%m%d-%H%M%S", t);
    snprintf(f_name, sizeof(f_name), "%s/%s.%d.r%d.%d.tar", OUTPUT_PATH, tmp_t, WINDOWSIZE, ring_id, thread_id);

    TIC(CLOCK_MONOTONIC, "TAR File");
    int fd;
    if ((fd = open(f_name, O_CREAT | O_WRONLY | O_TRUNC, 0660)) != -1)
    {
        size_t offset                     = 0;
        const unsigned char padblock[512] = { 0 };

        for (int i = 0; i < (WINDOWSIZE / SUBWINSIZE); i++)
        {
            struct posix_tar_header th = { 0 }; // let's make a tar file, or close enough

            const unsigned char *th_ptr = &th;
            size_t tmp_chksum           = 0;
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

            write(fd, &th, sizeof(th));
            write(fd, blob_list[i].blob_data, blob_list[i].blob_size);

            offset += sizeof(th) + blob_list[i].blob_size;
            aligned = offset % 512; // pad output file to 512 byte alignment
            if (aligned != 0)
            {
                write(fd, padblock, 512 - aligned);
                offset += 512 - aligned;
            }
        }
        close(fd);
    }
    else
        perror("open");
    TOC(CLOCK_MONOTONIC, "TAR File");
#endif

    for (int i = 0; i < (WINDOWSIZE / SUBWINSIZE); i++)
    {
        free(blob_list[i].blob_data);
    }
    free(untyped_p);
    pthread_exit(NULL);
}

// trivial TCP listener - outputs received strings on stderr.
// This is only used to provide a clear demarcation for events in the logs of benchmark runs.
static void *listener(void *untyped_p)
{
    char buf[256] = { 0 }, sbuf[256] = { 0 };
    int fd, s, n, one = 1;
    struct sockaddr_in serv_addr, client_addr;
    struct timespec ts_start, ts_end, tsdiff;
    double t_elapsed, t_ts;
    socklen_t client_addr_len = sizeof(client_addr);

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        pthread_exit(NULL);
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    bzero(&serv_addr, sizeof(serv_addr));
    bzero(&client_addr, sizeof(client_addr));

    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    serv_addr.sin_port        = htons(1732);

    if (bind(fd, &serv_addr, sizeof(struct sockaddr_in)) < 0)
    {
        perror("bind");
        pthread_exit(NULL);
    }

    if (listen(fd, 1) != 0)
    {
        perror("listen");
        pthread_exit(NULL);
    }

    fprintf(stderr, "thread READY: tcp listener\n");
    while (1)
    {
        if ((s = accept(fd, &client_addr, &client_addr_len)) < 0)
        {
            perror("accept");
            pthread_exit(NULL);
        }
        bzero(buf, sizeof(buf));
        if ((n = recv(s, &buf, sizeof(buf), 0)) > 0)
        {
            char tbuf[64] = { 0 };
            bzero(sbuf, sizeof(sbuf));
            snprintf(sbuf, sizeof(sbuf) - 1, "RECV: %s\n", buf);
            if (strstr(sbuf, "START") != NULL)
            {
                pid_t _mytid = syscall(SYS_gettid);
                clock_gettime(CLOCK_MONOTONIC, &ts_start);
                t_ts = ((ts_start.tv_sec * 1e9) + (ts_start.tv_nsec)) * 1e-9;
                sprintf(tbuf, "[%d] [%.2f]", _mytid, t_ts);
            }
            else if (strstr(sbuf, "END") != NULL)
            {
                pid_t _mytid = syscall(SYS_gettid);
                clock_gettime(CLOCK_MONOTONIC, &ts_end);
                timespec_diff(&ts_end, &ts_start, &tsdiff);
                t_ts      = ((ts_start.tv_sec * 1e9) + (ts_start.tv_nsec)) * 1e-9;
                t_elapsed = ((tsdiff.tv_sec * 1e9) + (tsdiff.tv_nsec)) * 1e-9;
                sprintf(tbuf, "[%d] [%.2f] [%.2f]", _mytid, t_ts, t_elapsed);
            }
            snprintf(sbuf, sizeof(sbuf) - 1, "%s RECV: %s\n", tbuf, buf);
            fprintf(stderr, sbuf);
        }
        close(s);
    }
    close(fd);
    free(untyped_p);
}
