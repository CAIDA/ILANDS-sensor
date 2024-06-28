#define _GNU_SOURCE 1
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
// #include <limits.h>
// #include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
// #include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
// #include <netinet/if_ether.h>
// #include <netinet/ip.h>
// #include <netinet/tcp.h>
// #include <netinet/udp.h>
// #include <openssl/evp.h>
// #include <openssl/aes.h>
// #include <openssl/evp.h>
// #include <openssl/err.h>
// #include <malloc.h>
// #include <zlib.h>
// #include <pcap.h>
#include <GraphBLAS.h>
// #include "cJSON.h"
#include "cryptopANT.h"
#include "yyjson.h"
// #include "common.h"
#include <signal.h>
// #include <sys/select.h>

#define WINDOWSIZE (1 << 23)   // Packets per output tar file
#define SUBWINSIZE (1 << 17)   // Packets per GraphBLAS matrix file (.grb)
#define BUFFERSIZE 1024 * 1024 // 1MB input buffer size when processing files of json flow records

#define BSWAP(a)   (pstate->swapped ? ntohl(a) : (a))

// Simple error handler wrapper for GraphBLAS calls
#define LAGRAPH_TRY_EXIT(method)                                                                                       \
    {                                                                                                                  \
        GrB_Info info = (method);                                                                                      \
        if (!(info == GrB_SUCCESS))                                                                                    \
        {                                                                                                              \
            fprintf(stderr, "LAGraph error: [%d]\nFile: %s Line: %d\n", info, __FILE__, __LINE__);                     \
            exit(1);                                                                                                   \
        }                                                                                                              \
    }

// Convenience macros to time code sections.
#define TIC(clocktype, msg)                                                                                            \
    {                                                                                                                  \
        double t_ts;                                                                                                   \
        pid_t _mytid = syscall(SYS_gettid);                                                                            \
        clock_gettime(clocktype, &ts_start);                                                                           \
        t_ts = ((ts_start.tv_sec * 1e9) + (ts_start.tv_nsec)) * 1e-9;                                                  \
        if (*msg != '\0') {                                                                                            \
            fprintf(stderr, "[%d] [%.2f] [%s] Section begin.\n", _mytid, t_ts, msg);                                   \
        }                                                                                                              \
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
        if (*msg != '\0') {                                                                                            \
            fprintf(stderr, "[%d] [%.2f] [%s] elapsed %.2fs\n", _mytid, t_ts, msg, t_elapsed);                         \
        }                                                                                                              \
    }

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

// Support for minimal output tar file creation
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

// Global state structure
struct px3_state
{
    unsigned int anonymize;
    unsigned int binary;
    unsigned int swapped;
    unsigned int rec;
    uint64_t total_packets;
    char *tmpdir;
    char *out_prefix;
    char f_name[PATH_MAX];
    int findex;
    bool wait;
    GrB_Index *R, *C;
    uint32_t *V;
    uint32_t npkts;
    uint32_t windowsize;
    uint32_t subwinsize;
    double t_grb;
    double t_json;
};

struct px3_state *pstate; // global state

void usage(const char *name)
{
//                   12345678901234567890123456789012345678901234567890123456789012345678901234567890
    fprintf(stderr, "usage: %s [-a anonymize.key] [-b[i][o]] [-O] [-o OUTPUT_DIRECTORY] [-S] [-s] [-t TMPDIR] [-W FILES_PER_WINDOW] [-w SUBWINSIZE] -i INPUT_FILE\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "    -a Anonymize using CryptopANT (https://ant.isi.edu/software/cryptopANT/index.html)\n");
    fprintf(stderr, "       If CryptoPAN anonymization keyfile does not exist, a random key will be generated and saved.\n");
    fprintf(stderr, "    -b Binary (raw) input/output");
    fprintf(stderr, "    -i Input file (json formatted flow records).\n");
    fprintf(stderr, "    -O Single file mode - one tar file containing one GraphBLAS matrix.\n");
    fprintf(stderr, "    -o Output directory (where filled tar files are moved).\n");
    fprintf(stderr, "    -S Swap byte order of IPv4 addresses.\n");
    fprintf(stderr, "    -s Select socket input mode.\n");
    fprintf(stderr, "    -t Temporary directory for building unfilled tar files.\n");
    fprintf(stderr, "    -W Number of GraphBLAS matrices to save in the output tar file.\n");
    fprintf(stderr, "    -w Window size (number of entries) in the saved GraphBLAS matrices.\n");
}

void set_output_filename(const char *f_name)
{
    if (f_name == NULL)
    {
        time_t tm;
        char timestr[16];

        tm = time(0);
        strftime(timestr, sizeof(timestr), "%Y%m%d-%H%M%S", localtime(&tm));
        snprintf(pstate->f_name, sizeof(pstate->f_name), "%s/%s.%d.%s", pstate->tmpdir, timestr, pstate->windowsize,
                 (pstate->binary & 2) ? "dat" : "tar");
    }
    else
    {
        strncpy(pstate->f_name, f_name, sizeof(pstate->f_name));
    }

    if (pstate->f_name[0] != '\0')
    {
        fprintf(stderr, "Output %s file is '%s'.\n", (pstate->binary & 2) ? "dat" : "tar", pstate->f_name);
    }

    pstate->findex = 0;
}

void dump_binary(void)
{
    int fd;

    if (pstate->f_name[0] == '\0')
    {
        set_output_filename(NULL);
    }

    if ((fd = open(pstate->f_name, O_CREAT | O_WRONLY | O_APPEND, 0660)) == -1)
    {
        perror("open dat");
        exit(errno);
    }

    if (write(fd, pstate->R, sizeof(GrB_Index) * pstate->rec) != sizeof(GrB_Index) * pstate->rec)
    {
        perror("write dat error [R]");
        exit(errno);
    }

    if (write(fd, pstate->C, sizeof(GrB_Index) * pstate->rec) != sizeof(GrB_Index) * pstate->rec)
    {
        perror("write dat error [C]");
        exit(errno);
    }

    if (write(fd, pstate->V, sizeof(uint32_t) * pstate->rec) != sizeof(uint32_t) * pstate->rec)
    {
        perror("write dat error [V]");
        exit(errno);
    }

    close(fd);

    pstate->npkts = 0;
    pstate->rec   = 0;
}

int load_binary(FILE *in)
{
    uint64_t i;

    if (pstate->rec > pstate->subwinsize)
    {
        pstate->rec = pstate->subwinsize;
    }
    if (fread(pstate->R, sizeof(GrB_Index), pstate->rec, in) != pstate->rec)
    {
        perror("fread dat error [R]");
        exit(EXIT_FAILURE);
    }
    if (fread(pstate->C, sizeof(GrB_Index), pstate->rec, in) != pstate->rec)
    {
        perror("fread dat error [C]");
        exit(EXIT_FAILURE);
    }
    if (fread(pstate->V, sizeof(uint32_t), pstate->rec, in) != pstate->rec)
    {
        perror("fread dat error [V]");
        exit(EXIT_FAILURE);
    }
    pstate->total_packets -= pstate->npkts;
    for (i = 0; i < pstate->rec; i++)
    {
        if (pstate->anonymize != 0)
        {
            pstate->R[i] = scramble_ip4(pstate->R[i], 16);
            pstate->C[i] = scramble_ip4(pstate->C[i], 16);
        }

        pstate->npkts += pstate->V[i];
    }
    pstate->total_packets += pstate->npkts;
    return (pstate->rec);
}

void move_file_to_dir(const char *src, const char *dst_dir)
{
    char cmd[PATH_MAX * 2 + 12] = "";

    fprintf(stderr, "Moving output tar file, '%s', to output directory.\n", src);
    snprintf(cmd, sizeof(cmd), "/bin/mv %s %s/", src, dst_dir);
    system(cmd);
}

void add_to_tar(void *blob_data, unsigned int blob_size)
{
    int tarfd;
    struct posix_tar_header th  = { 0 }; // let's make a tar file, or close enough
    const unsigned char *th_ptr = (const unsigned char *)&th;
    size_t tmp_chksum           = 0;
    size_t aligned;

    if (pstate->f_name[0] == '\0')
    {
        set_output_filename(NULL);
    }

    if ((tarfd = open(pstate->f_name, O_CREAT | O_WRONLY | O_APPEND, 0660)) == -1)
    {
        perror("open tar");
        exit(1);
    }

    sprintf(th.name, "%d.grb", pstate->findex++);
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

    if (pstate->findex >= pstate->windowsize)
    {
        if (pstate->out_prefix != NULL)
        {
            move_file_to_dir(pstate->f_name, pstate->out_prefix);
        }
        set_output_filename("");
    }
}

void build_and_store_matrix(void)
{
    GrB_Matrix Gmat;
    void *blob          = NULL;
    GrB_Index blob_size = 0;
    GrB_Descriptor desc = NULL;

    struct timespec ts_start; // for TIC() and TOC()
    double t_elapsed = 0;     // for TIC() and TOC()

    TIC(CLOCK_REALTIME, "");
    LAGRAPH_TRY_EXIT(GrB_Matrix_new(&Gmat, GrB_UINT32, 4294967296, 4294967296));
    LAGRAPH_TRY_EXIT(GrB_Matrix_build(Gmat, pstate->R, pstate->C, pstate->V, pstate->rec, GrB_PLUS_UINT32));

    GrB_Descriptor_new(&desc);
    GxB_Desc_set(desc, GxB_COMPRESSION, GxB_COMPRESSION_ZSTD + 1);
    LAGRAPH_TRY_EXIT(GxB_Matrix_serialize(&blob, &blob_size, Gmat, desc));
    TOC(CLOCK_REALTIME, "");
    pstate->t_grb += t_elapsed;

    add_to_tar(blob, blob_size);

    free(blob);

    GrB_free(&Gmat);

    pstate->npkts = 0;
    pstate->rec   = 0;
}

void add_packets(in_addr_t src_saddr, in_addr_t dst_saddr, uint32_t npkts)
{
    if (npkts == 0)
        return;

    pstate->total_packets += npkts;

    pstate->R[pstate->rec] = src_saddr;
    pstate->C[pstate->rec] = dst_saddr;
    pstate->V[pstate->rec] = npkts;
    pstate->npkts += npkts;
    pstate->rec++;

    // Generate output whenever the number of packets exceeds SUBWINSIZE
    if (pstate->npkts >= pstate->subwinsize)
    {
        // JSON flow records can represent multiple packets, so the packet
        // count in the buffer may exceed SUBWINSIZE.

        // If the number of packets in the buffer exceeds SUBWINSIZE, reduce
        // the count to match SUBWINSIZE exactly.
        npkts         = pstate->npkts - pstate->subwinsize;
        pstate->npkts = 0;
        if (npkts > 0)
        {
            pstate->V[pstate->rec - 1] -= npkts;
            pstate->total_packets -= npkts;
        }

        // Output and clear the packet buffer.
        if (pstate->binary & 2)
        {
            dump_binary();
        }
        else
        {
            build_and_store_matrix();
        }

        // If the packet count was over SUBWINSIZE, initialize the buffer
        // with the overage.
        if (npkts > 0)
        {
            pstate->R[0]  = src_saddr;
            pstate->C[0]  = dst_saddr;
            pstate->V[0]  = npkts;
            pstate->npkts = npkts;
            pstate->rec   = 1;
        }
    }
}

#define process_suricata_flow_json_line process_suricata_flow_yyjson_line

void process_suricata_flow_yyjson_line(char *str, size_t len)
{
    struct timespec ts_start; // for TIC() and TOC()
    double t_elapsed = 0;     // for TIC() and TOC()

    TIC(CLOCK_REALTIME, "");

    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts(str, len, YYJSON_READ_STOP_WHEN_DONE, NULL, &err);

    struct in_addr tmp_inaddr;
    in_addr_t src_saddr, dst_saddr;

    if (doc == NULL)
    {
        fprintf(stderr, "yyjson parse error: %s\n", err.msg);
        goto errexit_yyjson;
    }

    yyjson_val *root_val = yyjson_doc_get_root(doc);
    yyjson_val *flow_val = yyjson_obj_get(root_val, "flow");

    if (flow_val == NULL)
    {
        fprintf(stderr, "yyjson: no 'flow' found in JSON string.\n");
        goto errexit_yyjson;
    }

    yyjson_val *src_ip_val = yyjson_obj_get(root_val, "src_ip");
    const char *src_ip     = yyjson_get_str(src_ip_val);

    yyjson_val *dest_ip_val = yyjson_obj_get(root_val, "dest_ip");
    const char *dest_ip     = yyjson_get_str(dest_ip_val);

    if (src_ip == NULL || inet_aton(src_ip, &tmp_inaddr) == 0)
    {
        fprintf(stderr, "Malformed input: %s\n", src_ip);
        goto errexit_yyjson;
    }
    else
        src_saddr = BSWAP(tmp_inaddr.s_addr);

    if (dest_ip == NULL || inet_aton(dest_ip, &tmp_inaddr) == 0)
    {
        fprintf(stderr, "Malformed input: %s\n", dest_ip);
        goto errexit_yyjson;
    }
    else
        dst_saddr = BSWAP(tmp_inaddr.s_addr);

    if (pstate->anonymize != 0)
    {
        src_saddr = scramble_ip4(src_saddr, 16);
        dst_saddr = scramble_ip4(dst_saddr, 16);
    }

    yyjson_val *pkts_toclient_val = yyjson_obj_get(flow_val, "pkts_toclient");
    yyjson_val *pkts_toserver_val = yyjson_obj_get(flow_val, "pkts_toserver");

    uint64_t pkts_toclient = yyjson_get_uint(pkts_toclient_val);
    uint64_t pkts_toserver = yyjson_get_uint(pkts_toserver_val);

    add_packets(src_saddr, dst_saddr, pkts_toserver);
    add_packets(dst_saddr, src_saddr, pkts_toclient);

errexit_yyjson:
    if (doc != NULL)
    {
        yyjson_doc_free(doc);
    }
    TOC(CLOCK_REALTIME, "");
    pstate->t_json += t_elapsed;
    return;
}

int process_buffer(char *buffer, size_t *offset)
{
    char *start = buffer;
    char *end;
    int records = 0;

    if (*offset >= BUFFERSIZE)
    {
        perror("process buffer");
        exit(EINVAL);
    }

    buffer[*offset] = '\0';
    while ((end = strchr(start, '\n')) != NULL)
    {
        *end = '\0';
        records++;
        process_suricata_flow_json_line(start, end - start);
        start = end + 1;
    }
    *offset -= start - buffer;
    memmove(buffer, start, *offset);
    return (records);
}

int setup_socket(const char *socket_path)
{
    int s;
    struct sockaddr_un local, remote;
    int len = 0;

    fprintf(stderr, "Setting up socket to receive data...\n");

    s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s == -1)
    {
        perror("Error on socket() call");
        exit(1);
    }

    local.sun_family = AF_UNIX;
    strncpy(local.sun_path, socket_path, sizeof(local.sun_path));
    unlink(local.sun_path);
    if (bind(s, (struct sockaddr *)&local, sizeof(struct sockaddr_un)) != 0)
    {
        perror("Error on binding socket");
        exit(1);
    }

    if (listen(s, 1) != 0)
    {
        perror("Error on listen() call");
        exit(1);
    }

    fprintf(stderr, "  ...listening.\n");

    return (s);
}

void stop(int dummy)
{
    fprintf(stderr, "Signal recieved: will exit when current conection is closed.\n");

    pstate->wait = false;
}

int main(int argc, char *argv[])
{
    int c, ret, reqargs = 0; // for getopt
    FILE *in;
    char in_f[PATH_MAX] = "";
    int socket                 = 0;
    char anonkey[PATH_MAX];
    size_t filesize;
    size_t offset;
    struct timespec ts_start;   // for TIC() and TOC()
    double t_elapsed       = 0; // for TIC() and TOC()
    uint64_t total_records = 0;
    char buf[BUFFERSIZE];
    int partial = 0;

    pstate                = calloc(1, sizeof(struct px3_state));
    pstate->anonymize     = 0;
    pstate->binary        = 0;
    pstate->swapped       = 0;
    pstate->npkts         = 0;
    pstate->total_packets = 0;
    pstate->tmpdir        = ".";
    pstate->windowsize    = 64;      // WINDOWSIZE;
    pstate->subwinsize    = 1 << 17; // SUBWINSIZE;
    pstate->t_grb         = 0;
    pstate->t_json        = 0;

    while ((c = getopt(argc, argv, "Sa:b:i:O::o:pst:W:w:")) != -1)
    {
        switch (c)
        {
            case 'a':
                pstate->anonymize = 1;
                snprintf(anonkey, sizeof(anonkey), "%s", optarg);
                break;
            case 'b':
                // binary input
                if (*optarg == 'i')
                    pstate->binary |= 1;
                // binary output
                if (*optarg == 'o')
                    pstate->binary |= 2;
                break;
            case 'i':
                // input file
                reqargs++;
                snprintf(in_f, sizeof(in_f), "%s", optarg);
                break;
            case 'S':
                pstate->swapped = 1;
                break;
            case 'O':
                pstate->windowsize = 1;
                pstate->subwinsize = UINT_MAX;
                if (optarg != NULL)
                {
                    set_output_filename(optarg);
                }
                break;
            case 'o':
                // output dir
                pstate->out_prefix = strdup(optarg);
                break;
            case 'p':
                // output partial matricies
                partial = 1;
                break;
            case 's':
                // unix socket input
                socket = 1;
                break;
            case 't':
                pstate->tmpdir = strdup(optarg);
                break;
            case 'W':
                // windowsize
                pstate->windowsize = strtoul(optarg, NULL, 10);
                break;
            case 'w':
                // subwinsize
                pstate->subwinsize = strtoul(optarg, NULL, 10);
                break;
            case '?':
                if (optopt == 'i' || optopt == 'o' || optopt == 'a' || optopt == 'b' || optopt == 's' || optopt == 't' || optopt == 'W' || optopt == 'w')
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

    if (reqargs < 1)
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

    if (socket == 1)
    {
        socket = setup_socket(in_f);
    }
    else
    {
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
            fprintf(stderr, "%s input file is %ld bytes\n", (pstate->binary & 1) ? "binary" : "json", filesize);
            fflush(stderr);
        }
    }

    pstate->R = malloc(sizeof(GrB_Index) * pstate->subwinsize);
    pstate->C = malloc(sizeof(GrB_Index) * pstate->subwinsize);
    pstate->V = malloc(sizeof(uint32_t) * pstate->subwinsize);

    GrB_init(GrB_NONBLOCKING);

    offset = 0;
    if (socket == 0)
    {
        TIC(CLOCK_REALTIME, "json begin");
        while (!feof(in) && (ftell(in) < filesize))
        {
            if (pstate->binary & 1)
            {
                pstate->rec = (filesize - ftell(in)) / (2 * sizeof(GrB_Index) + sizeof(uint32_t));
                total_records += load_binary(in);
                if (pstate->binary & 2)
                {
                    dump_binary();
                }
                else
                {
                    build_and_store_matrix();
                }
            }
            else
            {
                size_t bytes_read = fread(buf + offset, 1, sizeof(buf) - offset - 1, in);

                offset += bytes_read;
                total_records += process_buffer(buf, &offset);
            }
        }
        TOC(CLOCK_REALTIME, "json end");
        fclose(in);
    }
    else
    {
        pstate->wait = true;
        signal(SIGINT, stop);
        while (pstate->wait)
        {
            int s;
            struct sockaddr_un remote;
            unsigned int s_len;
            int recv_len;

            struct timeval tv;
            fd_set fds;

            tv.tv_sec  = 1;
            tv.tv_usec = 0;

            FD_ZERO(&fds);
            FD_SET(socket, &fds);

            if (select(socket + 1, &fds, NULL, NULL, &tv) > 0)
            {
                if ((s = accept(socket, (struct sockaddr *)&remote, &s_len)) == -1)
                {
                    perror("Error on accept() call");
                    exit(1);
                }
                fprintf(stderr, "Connection received.\n");

                TIC(CLOCK_REALTIME, "json begin");
                do
                {
                    recv_len = recv(s, buf + offset, sizeof(buf) - offset - 1, 0);
                    if (recv_len > 0)
                    {
                        offset += recv_len;
                        total_records += process_buffer(buf, &offset);
                    }
                    else
                    {
                        fprintf(stderr, "Error on recv() call\n");
                    }
                } while (recv_len > 0);
                TOC(CLOCK_REALTIME, "json end");

                close(s);
            }
        }

        close(socket);

        unlink(in_f); // remove socket file
    }

    if (offset > 0) // handle any trailing JSON
    {
        buf[offset] = '\0';
        process_suricata_flow_json_line(buf, offset);
    }

    if (pstate->binary & 2)
    {
        dump_binary();
    }
    else
    {
        if (pstate->rec > 0)
        {
            if ((partial == 1) || (pstate->subwinsize == UINT_MAX))
            {
                fprintf(stderr, "Adding trailing %u packets to tar file.\n", pstate->rec);
                build_and_store_matrix();
            }
            else
            {
                fprintf(stderr, "INFO: Not processing %u remaining packets (less than matrix size of %u).\n", pstate->rec, pstate->subwinsize);
            }
        }
        if (pstate->findex > 0)
        {
            if (partial == 1)
            {
                fprintf(stderr, "INFO: Partial option selected -- unfilled tar file will be moved.\n", pstate->f_name);
                move_file_to_dir(pstate->f_name, pstate->out_prefix);
            }
            else
            {
                fprintf(stderr, "INFO: Not moving unfilled tar file, '%s' (less than window size of %u).\n", pstate->f_name, pstate->windowsize * pstate->subwinsize);
            }
        }
    }

    free(pstate->R);
    free(pstate->C);
    free(pstate->V);

    fprintf(stderr, "Done: %ld records.  (%.2f / sec)\n", total_records, total_records / t_elapsed);
    fprintf(stderr, "Done: %ld packets.  (%.2f pps)\n", pstate->total_packets, pstate->total_packets / t_elapsed);

    fprintf(stderr, "GrB: elapsed %.2fs\n", pstate->t_grb);
    fprintf(stderr, "yyjson: elapsed %.2fs\n", pstate->t_json);

    exit(0);
}
