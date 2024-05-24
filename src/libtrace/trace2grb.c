#include "common.h"
#include "libtrace_parallel.h"

#define DEFAULT_OUTPUT_PATH "/scratch"

uint32_t *ip4cache           = NULL;
const size_t cachesize       = sizeof(uint32_t) * (UINT_MAX - 1);
const int packet_buffer_size = (sizeof(uint32_t) * WINDOWSIZE) * 2;
const int thread_buffer_size = (sizeof(uint32_t) * PKTBUFSIZE) * 2;
char *output_path            = NULL;

volatile int done    = 0;
libtrace_t *inptrace = NULL;

/* Thread local storage for the reporting thread */
struct reporting_args
{
    uint32_t *pktbuf, *bufptr;
    int inbuffer;
    struct tm *t;
};

/* Thread local storage for each processing thread */
struct packet_args
{
    uint32_t *pktbuf, *bufptr;
    int inbuffer;
};

static void cleanup_signal(int sig)
{
    (void)sig; /* avoid warnings about unused parameter */
    done = 1;
    if (inptrace)
        trace_pstop(inptrace);
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

    for (int i = 0; i < (windowsize / subwinsize); i++)
    {
        free(blob_list[i].blob_data);
    }

    free(blob_list);
#endif
    return;
}

static void *graphblas_worker(void *untyped_p)
{
    size_t windowsize = ((struct graphblas_worker_args *)untyped_p)->windowsize;
    size_t subwinsize = ((struct graphblas_worker_args *)untyped_p)->subwinsize;
    uint32_t *pktbuf  = ((struct graphblas_worker_args *)untyped_p)->pktbuf;
    struct tm *t      = ((struct graphblas_worker_args *)untyped_p)->t;

    pthread_detach(pthread_self());

    time_t dummytime;
    time(&dummytime);
    fprintf(stderr, "spawned graphblas_worker thread for full block @ %s", ctime(&dummytime));
    fflush(stderr);
    buffer_to_grb(pktbuf, windowsize, subwinsize, t);

    free(pktbuf);
    free(untyped_p);

    return NULL; // not reached
}

static void *report_start(libtrace_t *trace, libtrace_thread_t *t, void *global)
{
    struct reporting_args *rs = (struct reporting_args *)malloc(sizeof(struct reporting_args));
    time_t dummytime;

    if ((rs->pktbuf = malloc(packet_buffer_size)) == NULL)
    {
        perror("malloc failure");
        exit(1);
    }
    rs->inbuffer = 0;
    rs->bufptr   = rs->pktbuf;

    time(&dummytime);
    rs->t = localtime(&dummytime);

    return rs;
}

// Called every time thread_buffer_size is reached in a worker thread.  Append block and dispatch GrB thread if buffer
// full.
static void report_cb(libtrace_t *trace, libtrace_thread_t *sender, void *global, void *tls, libtrace_result_t *res)
{
    struct reporting_args *rs = (struct reporting_args *)tls;
    time_t dummytime;
    time(&dummytime);

    /* Process the result */
    if (res->type != RESULT_USER)
        return;

    memcpy(rs->bufptr, res->value.ptr, thread_buffer_size);
    free(res->value.ptr);
    rs->inbuffer += thread_buffer_size;
    rs->bufptr += (PKTBUFSIZE * 2);
    fflush(stderr);

    if (rs->inbuffer >= packet_buffer_size)
    {
        pthread_t thread;
        struct graphblas_worker_args *gb_args_p = malloc(sizeof(*gb_args_p));

        gb_args_p->pktbuf     = rs->pktbuf;
        gb_args_p->t          = rs->t;
        gb_args_p->subwinsize = SUBWINSIZE;
        gb_args_p->windowsize = WINDOWSIZE;

        pthread_create(&thread, NULL, graphblas_worker, gb_args_p);

        time_t dummytime;
        time(&dummytime);

        if ((rs->pktbuf = malloc(packet_buffer_size)) == NULL)
        {
            perror("malloc failure");
            exit(1);
        }

        rs->inbuffer = 0;
        rs->bufptr   = rs->pktbuf;
        rs->t        = localtime(&dummytime);
    }
}

static void report_end(libtrace_t *trace, libtrace_thread_t *t, void *global, void *tls)
{
    /* Free the local storage and print any final results */
    struct rstorage *rs = (struct rstorage *)tls;
    free(rs);
}

static libtrace_packet_t *per_packet(libtrace_t *trace, libtrace_thread_t *t, void *global, void *tls,
                                     libtrace_packet_t *packet)
{
    struct packet_args *p_args_p = (struct packet_args *)tls;
    struct libtrace_ip *ip       = trace_get_ip(packet);

    // trace_get_ip returns NULL if not IPv4.  We only handle IPv4.
    if (!ip)
        return packet;

    *p_args_p->bufptr++ = ip->ip_src.s_addr;
    *p_args_p->bufptr++ = ip->ip_dst.s_addr;
    p_args_p->inbuffer++;

    if (p_args_p->inbuffer == PKTBUFSIZE)
    {
        trace_publish_result(trace, t, 0, (libtrace_generic_t){ .ptr = p_args_p->pktbuf }, RESULT_USER);
        if ((p_args_p->pktbuf = malloc(thread_buffer_size)) == NULL)
        {
            perror("malloc failure");
            exit(1);
        }
        p_args_p->inbuffer = 0;
        p_args_p->bufptr   = p_args_p->pktbuf;
    }

    return packet;
}

static void *start_processing(libtrace_t *trace, libtrace_thread_t *t UNUSED, void *global)
{
    /* Create any local storage required by the reporter thread and
     * return it. */
    struct packet_args *ps = (struct packet_args *)malloc(sizeof(struct packet_args));

#ifndef NO_GRAPHBLAS_DEBUG // msj
    GrB_init(GrB_NONBLOCKING);
#endif

    if ((ps->pktbuf = malloc(thread_buffer_size)) == NULL)
    {
        perror("malloc failure");
        exit(1);
    }
    ps->inbuffer = 0;
    ps->bufptr   = ps->pktbuf;

    return ps;
}

static void stop_processing(libtrace_t *trace, libtrace_thread_t *t, void *global, void *tls)
{
    struct packet_args *ps = (struct packet_args *)tls;

    /* May want to do a final publish here... */
    free(ps);
}

static void usage(char *prog)
{
    fprintf(stderr, "Usage for %s\n\n", prog);
    fprintf(stderr, "%s [options] inputURI [inputURI ...]\n\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "\t-t threads Set the number of processing threads\n");
    fprintf(stderr, "\t-c file    Point to a precomputed IPv4 anonymization table\n");
    fprintf(stderr, "\t-o dir     Directory to place output files.  Default: %s\n", DEFAULT_OUTPUT_PATH);
    fprintf(stderr, "\t-f expr    Discard all packets that do not match the BPF expression\n");

    exit(0);
}

int main(int argc, char *argv[])
{
    libtrace_callback_set_t *processing = NULL;
    libtrace_callback_set_t *reporter   = NULL;
    libtrace_filter_t *filter           = NULL;
    struct stat st;
    char *filterstring = NULL;
    int i, opt, retcode = 0, usecache = 0;
    struct sigaction sigact;
    int threads              = 8;
    char cachefile[PATH_MAX] = { 0 };

    /* TODO replace this with whatever global data your threads are
     * likely to need. */
    uint32_t global = 0;

    if (argc < 2)
    {
        usage(argv[0]);
    }

    while ((opt = getopt(argc, argv, "o:c:f:t:")) != EOF)
    {
        switch (opt)
        {
            case 'c':
                snprintf(cachefile, sizeof(cachefile) - 1, "%s", optarg);
                usecache = 1;
                break;
            case 'f':
                filterstring = optarg;
                break;
            case 't':
                threads = atoi(optarg);
                break;
            case 'o':
                output_path = strdup(optarg);
                break;
            default:
                usage(argv[0]);
        }
    }

    if (output_path == NULL)
        output_path = strdup(DEFAULT_OUTPUT_PATH);

    stat(output_path, &st);
    if (!S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "invalid directory for output: %s\n", output_path);
        exit(2);
    }
    else
        fprintf(stderr, "using output directory: %s\n", output_path);

    if (usecache)
    {
        FILE *fp;
        size_t filesize = 0, ret = 0;
        struct timespec ts_start;

        fprintf(stderr, "loading anonymization table: %s\n", cachefile);
        ip4cache = malloc(cachesize);

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
    }

    if (optind + 1 > argc)
    {
        usage(argv[0]);
        return 1;
    }

    if (filterstring)
    {
        filter = trace_create_filter(filterstring);
    }

    sigact.sa_handler = cleanup_signal;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART;

    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);

    processing = trace_create_callback_set();
    trace_set_starting_cb(processing, start_processing);
    trace_set_stopping_cb(processing, stop_processing);
    trace_set_packet_cb(processing, per_packet);

    reporter = trace_create_callback_set();
    trace_set_starting_cb(reporter, report_start);
    trace_set_stopping_cb(reporter, report_end);
    trace_set_result_cb(reporter, report_cb);

    for (i = optind; i < argc; i++)
    {

        inptrace = trace_create(argv[i]);

        if (trace_is_err(inptrace))
        {
            trace_perror(inptrace, "Opening trace file");
            retcode = -1;
            break;
        }

        if (filter && trace_config(inptrace, TRACE_OPTION_FILTER, filter) == -1)
        {
            trace_perror(inptrace, "trace_config(filter)");
            retcode = -1;
            break;
        }

        trace_set_perpkt_threads(inptrace, threads);
        trace_set_reporter_thold(inptrace, 5);

        if (trace_pstart(inptrace, &global, processing, reporter))
        {
            trace_perror(inptrace, "Starting trace");
            break;
        }

        /* Wait for all threads to stop */
        trace_join(inptrace);

        if (trace_is_err(inptrace))
        {
            trace_perror(inptrace, "Processing packets");
            retcode = -1;
            break;
        }

        if (done)
            break;
    }

    if (filter)
        trace_destroy_filter(filter);

    trace_destroy(inptrace);
    trace_destroy_callback_set(processing);
    trace_destroy_callback_set(reporter);
    return retcode;
}
