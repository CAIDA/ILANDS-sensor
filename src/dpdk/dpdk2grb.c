#include "rte_build_config.h"
#define _GNU_SOURCE 1

#include <fcntl.h>
#include <pthread.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <GraphBLAS.h>

#define RX_RING_SIZE    8192 // Can be retrieved with ethtool -g <ADAPTER>
#define NUM_MBUFS       ((8192 - 1) * 32)
#define MBUF_CACHE_SIZE 500
#define BURST_SIZE      8192

#define WINDOWSIZE      (1 << 23)
#define SUBWINSIZE      (1 << 17)
#define PKTBUFSIZE      (1 << 17)

// If at first you don't succeed, just abort.
#ifndef NO_GRAPHBLAS_DEBUG
#ifndef LAGRAPH_TRY_EXIT
#define LAGRAPH_TRY_EXIT(method)                                                                                       \
    {                                                                                                                  \
        GrB_Info info = (method);                                                                                      \
        if (!(info == GrB_SUCCESS))                                                                                    \
        {                                                                                                              \
            fprintf(stderr, "LAGraph error: [%d]\nFile: %s Line: %d\n", info, __FILE__, __LINE__);                     \
            exit(1);                                                                                                   \
        }                                                                                                              \
    }
#endif
#endif

const int buffer_size = (sizeof(uint32_t) * WINDOWSIZE) * 2;
uint32_t *ip4cache    = NULL;
char *output_path     = "/scratch";

struct rte_ring *ring;

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

struct _serialized_blob
{
    void *blob_data;
#ifndef NO_GRAPHBLAS_DEBUG
    GrB_Index blob_size;
#endif
};

struct graphblas_worker_args
{
    uint32_t *pktbuf;
    struct tm *t;
    size_t windowsize;
    size_t subwinsize;
};

struct lcore_worker_args
{
    uint16_t port_id;
    uint16_t queue_id;
    struct rte_mempool *mbuf_pool;
};

static const struct rte_eth_conf port_conf_default = {
    .rxmode =
        {
                 .mq_mode = RTE_ETH_MQ_RX_RSS,
                 },
    .rx_adv_conf =
        {
                 .rss_conf =
                {
                    .rss_key = NULL,
                    .rss_hf  = RTE_ETH_RSS_TCP,
                }, },
};

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

static void buffer_to_grb(uint32_t *pktbuf, size_t windowsize, size_t subwinsize, struct tm *t)
{
#ifndef NO_GRAPHBLAS_DEBUG
    char tmp_t[64], f_name[PATH_MAX];
    GrB_Descriptor desc = NULL;
    void *blob          = NULL;
    GrB_Index blob_size = 0;
    GrB_Matrix Gmat;
    int i = 0;

    // Row, Col, Val vectors.
    GrB_Index *R = malloc(sizeof(GrB_Index) * subwinsize);
    GrB_Index *C = malloc(sizeof(GrB_Index) * subwinsize);
    uint32_t *V  = malloc(sizeof(uint32_t) * subwinsize);

    struct _serialized_blob *blob_list = malloc(sizeof(struct _serialized_blob) * (windowsize / subwinsize));
    uint32_t *bufptr                   = pktbuf;

    // With matrices this small (2^17), NTHREADS == 1 yields best performance for serialization.
    GxB_set(GxB_NTHREADS, 1);

    // Configure compression.  ZSTD level 1 is best.
    GrB_Descriptor_new(&desc);
    GxB_Desc_set(desc, GxB_COMPRESSION, GxB_COMPRESSION_ZSTD + 1);

    for (int subblock = 0; subblock < (windowsize / subwinsize); subblock++)
    {
        LAGRAPH_TRY_EXIT(GrB_Matrix_new(&Gmat, GrB_UINT32, 4294967296, 4294967296));

        for (i = 0; i < subwinsize; i++)
        {
            uint32_t srcip = (ip4cache == NULL ? *bufptr++ : ip4cache[*bufptr++]);
            uint32_t dstip = (ip4cache == NULL ? *bufptr++ : ip4cache[*bufptr++]);

            R[i] = srcip;
            C[i] = dstip;
            V[i] = 1;
        }

        LAGRAPH_TRY_EXIT(GrB_Matrix_build(Gmat, R, C, V, subwinsize, GrB_PLUS_UINT32));
        LAGRAPH_TRY_EXIT(GxB_Matrix_serialize(&blob, &blob_size, Gmat, desc));

        blob_list[subblock].blob_size = blob_size;
        blob_list[subblock].blob_data = blob;

        GrB_free(&Gmat);
    }
    free(R);
    free(C);
    free(V);

    strftime(tmp_t, sizeof(tmp_t), "%Y%m%d-%H%M%S", t);
    snprintf(f_name, sizeof(f_name), "%s/%s.%ld.tar", output_path, tmp_t, windowsize);

#ifdef SAVETHEBLOB
    int fd;
    if ((fd = open(f_name, O_CREAT | O_WRONLY | O_TRUNC, 0660)) != -1)
    {
        size_t offset                     = 0;
        const unsigned char padblock[512] = { 0 };

        for (int i = 0; i < (windowsize / subwinsize); i++)
        {
            struct posix_tar_header th = { 0 }; // let's make a tar file, or close enough

            const unsigned char *th_ptr = (const unsigned char *)&th;
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
#endif

    for (int i = 0; i < (windowsize / subwinsize); i++)
    {
        free(blob_list[i].blob_data);
    }

    free(blob_list);
#endif
    return;
}

static void graphblas_worker(void *arg)
{
    size_t windowsize = ((struct graphblas_worker_args *)arg)->windowsize;
    size_t subwinsize = ((struct graphblas_worker_args *)arg)->subwinsize;
    uint32_t *pktbuf  = ((struct graphblas_worker_args *)arg)->pktbuf;
    struct tm *t      = ((struct graphblas_worker_args *)arg)->t;

    // pthread_detach(pthread_self());

    time_t dummytime;
    time(&dummytime);
    fprintf(stderr, "spawned graphblas_worker thread for full block @ %s", ctime(&dummytime));
    fflush(stderr);
    buffer_to_grb(pktbuf, windowsize, subwinsize, t);

    free(pktbuf);
    free(arg);

    // pthread_exit(NULL);
}

static inline int port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf = port_conf_default;
    //const uint16_t rx_rings = rte_lcore_count() - 1, tx_rings = 0; // we'll save one lcore for the aggregation thread?
    const uint16_t rx_rings = 4, tx_rings = 0; // we'll save one lcore for the aggregation thread?
    int retval;

    if (port >= rte_eth_dev_count_avail())
        return -1;

    fprintf(stderr, "Initializing port: %u\n", port);
    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);

    if (retval != 0)
        return retval;

    fprintf(stderr, "Creating %u RX Queues.\n", rx_rings);
    for (uint16_t q = 0; q < rx_rings; q++)
    {
        retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE, rte_eth_dev_socket_id(port), NULL, mbuf_pool);

        if (retval < 0)
            return retval;
    }

    fprintf(stderr, "Starting device.\n");
    retval = rte_eth_dev_start(port);
    if (retval < 0)
        return retval;

    fprintf(stderr, "Enabling packet capture.\n");
    rte_eth_promiscuous_enable(port);

    return 0;
}

static __rte_noreturn void lcore_agg(void)
{
    const uint16_t port = 0;
    void *gb_args_p;
    struct timespec ts_start, ts_end, tsdiff;
    uint64_t npkts = 0;
    struct rte_eth_stats rstats;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    fprintf(stderr, "Spinning up aggregation lcore %u\n", rte_lcore_id());

    while (1)
    {
        while (rte_ring_dequeue(ring, &gb_args_p) != 0)
        {
            ;
        }
        rte_eth_stats_get(port, &rstats);
        npkts += WINDOWSIZE;
        graphblas_worker(gb_args_p);
        fprintf(stderr, "AGG: Processed block.  Total Packets: %lu, Dropped: %lu, Queued: %u\n", npkts, rstats.imissed,
                rte_ring_count(ring));
    }
}

static __rte_noreturn void lcore_main(void *arg)
{
    struct lcore_worker_args *args = (struct lcore_worker_args *) arg;
    const uint16_t port = args->port_id;
    uint16_t q          = args->queue_id;
    uint64_t packets    = 0;
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ip_hdr;
    time_t dummytime;
    struct tm *t;
    uint32_t *pktbuf, *bufptr;
    uint32_t npkts = 0;

    fprintf(stderr, "Spinning up lcore %u for RSS queue %u\n", rte_lcore_id(), q);

    /*
    if ((q + 1) > rte_lcore_count())
    {
        fprintf(stderr, "lcore %u not starting: rte_lcore_count(): %u\n", q, rte_lcore_count());
        while (1)
        {
            sleep(1);
        }
    }
    */


    if ((pktbuf = malloc(buffer_size)) == NULL) // WINDOWSIZE ip address pairs
        rte_exit(EXIT_FAILURE, "Cannot allocate packet buffer\n");

    bufptr = pktbuf;

    while (1)
    {
        struct rte_mbuf *bufs[BURST_SIZE];
        const uint16_t nb_rx = rte_eth_rx_burst(port, q, bufs, BURST_SIZE);

        if (unlikely(nb_rx == 0))
        {
            usleep(10);
            continue;
        }

        if (unlikely(npkts == 0))
        {
            time(&dummytime);
            t = localtime(&dummytime);
        }

        /* Process packets */
        for (uint16_t i = 0; i < nb_rx; i++)
        {
            eth_hdr = rte_pktmbuf_mtod(bufs[i], struct rte_ether_hdr *);
            if (likely(eth_hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)))
            {
                ip_hdr    = rte_pktmbuf_mtod_offset(bufs[i], struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
                *bufptr++ = ip_hdr->src_addr;
                *bufptr++ = ip_hdr->dst_addr;
                npkts++;

                if (npkts == WINDOWSIZE)
                {
                    struct graphblas_worker_args *gb_args_p = malloc(sizeof(*gb_args_p));

                    gb_args_p->pktbuf     = pktbuf;
                    gb_args_p->t          = t;
                    gb_args_p->subwinsize = SUBWINSIZE;
                    gb_args_p->windowsize = WINDOWSIZE;

                    if (rte_ring_enqueue(ring, gb_args_p) != 0)
                    {
                        fprintf(stderr, "ERR: Failed to enqueue block.\n");
                    }

                    if ((pktbuf = malloc(buffer_size)) == NULL) // WINDOWSIZE ip address pairs
                        rte_exit(EXIT_FAILURE, "Cannot allocate packet buffer\n");

                    bufptr = pktbuf;
                    fprintf(stderr, "[lcore %u] Got %u packets.\n", q, npkts);
                    npkts = 0;
                }
            }
            rte_pktmbuf_free(bufs[i]);
        }
    }
}

int main(int argc, char *argv[], char *envp[])
{
    struct rte_mempool *mbuf_pool;
    uint16_t portid, lcoreid;
    int ret;
    struct lcore_worker_args lcore_args[RTE_MAX_LCORE] = {0};

    ret = rte_eal_init(argc, argv);

    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

    fprintf(stderr, "EAL initialized.\n");

    mbuf_pool =
        rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    fprintf(stderr, "MBUF pool created. (%.2f MB)\n", (mbuf_pool->size * mbuf_pool->elt_size) / (1024.0 * 1024.0) );
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    if (port_init(0, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n", portid);

    fprintf(stderr, "Port initialized.\n");

#ifndef NO_GRAPHBLAS_DEBUG // msj
    GrB_init(GrB_NONBLOCKING);
#endif

    ring = rte_ring_create("RING", 1024, rte_socket_id(), 0);
    if (ring == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create ring.\n");

    uint16_t queue_id = 0;
    uint16_t num_cores = 4;
    RTE_LCORE_FOREACH_WORKER(lcoreid)
    {
        if( queue_id >= num_cores )
            break;

        lcore_args[lcoreid].port_id = 0;
        lcore_args[lcoreid].queue_id = queue_id;
        rte_eal_remote_launch((lcore_function_t *)lcore_main, &lcore_args[queue_id], queue_id);
        queue_id++;
    }

    fprintf(stderr, "Starting aggregation lcore.\n");
    lcore_agg();

    return 0;
}
