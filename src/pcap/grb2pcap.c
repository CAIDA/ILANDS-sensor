#define _GNU_SOURCE

#include <GraphBLAS.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pcap.h>
#include <pcap/pcap.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"

struct global_state
{
    struct config *config;
    int total_packets;
    unsigned int swapped;
    pcap_dumper_t *dumper;
};

struct config
{
    char path[PATH_MAX];
    time_t start_time, end_time;
    uint16_t ip_id, tcp_sport, tcp_dport, tcp_window;
    uint32_t tcp_seq;
    struct config *next;
};

#define BSWAP(a) (pstate->swapped ? ntohl(a) : (a))

void insert(struct config **root, const char *path, const char *start_time, const char *end_time, uint16_t ip_id,
            uint16_t tcp_sport, uint16_t tcp_dport, uint16_t tcp_window, uint32_t tcp_seq)
{
    struct config *node = (struct config *)calloc(sizeof(struct config), 1);
    char *tmp           = NULL;
    struct tm s_tm      = { 0 };

    tmp = strdup(start_time);
    strptime(tmp, "%Y%m%d-%H%M%S", &s_tm);
    free(tmp);
    node->start_time = mktime(&s_tm);

    tmp = strdup(end_time);
    strptime(tmp, "%Y%m%d-%H%M%S", &s_tm);
    free(tmp);
    node->end_time = mktime(&s_tm);

    strcpy(node->path, path);

    node->ip_id      = ip_id;
    node->tcp_sport  = tcp_sport;
    node->tcp_dport  = tcp_dport;
    node->tcp_window = tcp_window;
    node->tcp_seq    = tcp_seq;
    

    /*
    node->next = *root;
    *root      = node;
    */

    node->next = NULL;

    if( *root == NULL )
    {
        *root = node;
        return;
    }

    struct config *last = *root;
    while( last->next )
    {
        last = last->next;
    }
    last->next = node;    
}

void parse_config(struct config **root, const char *filename)
{
    FILE *fp;
    char *line   = NULL;
    size_t len   = 0;
    ssize_t read = 0;

    if ((fp = fopen(filename, "r")) == NULL)
    {
        perror("parse_config: fopen");
        exit(EXIT_FAILURE);
    }

    while ((read = getline(&line, &len, fp)) != -1)
    {

        char *tmp;
        char tmp_fn[PATH_MAX];
        char _tmp_buf[128], _start[128], _end[128];
        uint16_t ip_id, tcp_sport, tcp_dport, tcp_window;
        uint32_t tcp_seq;

        if (line[0] == '#' || line[0] == ' ')
            continue;

        if ((tmp = strtok(line, "\t")) != NULL)
            strncpy(tmp_fn, tmp, sizeof(tmp_fn) - 1);
        else
        {
            fprintf(stderr, "parse error.\n");
            exit(EXIT_FAILURE);
        }

        if ((tmp = strtok(NULL, "\t")) != NULL)
            strncpy(_start, tmp, sizeof(_start) - 1);
        else
        {
            fprintf(stderr, "parse error.\n");
            exit(EXIT_FAILURE);
        }

        if ((tmp = strtok(NULL, "\t")) != NULL)
            strncpy(_end, tmp, sizeof(_end) - 1);
        else
        {
            fprintf(stderr, "parse error.\n");
            exit(EXIT_FAILURE);
        }

        if ((tmp = strtok(NULL, "\t")) != NULL)
        {
            strncpy(_tmp_buf, tmp, sizeof(_tmp_buf) - 1);
            if ((sscanf(_tmp_buf, "%" SCNu16, &ip_id)) != 1)
            {
                fprintf(stderr, "parse error.\n");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            fprintf(stderr, "parse error.\n");
            exit(EXIT_FAILURE);
        }

        if ((tmp = strtok(NULL, "\t")) != NULL)
        {
            strncpy(_tmp_buf, tmp, sizeof(_tmp_buf) - 1);
            if ((sscanf(_tmp_buf, "%" SCNu16, &tcp_sport)) != 1)
            {
                fprintf(stderr, "parse error.\n");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            fprintf(stderr, "parse error.\n");
            exit(EXIT_FAILURE);
        }

        if ((tmp = strtok(NULL, "\t")) != NULL)
        {
            strncpy(_tmp_buf, tmp, sizeof(_tmp_buf) - 1);
            if ((sscanf(_tmp_buf, "%" SCNu16, &tcp_dport)) != 1)
            {
                fprintf(stderr, "parse error.\n");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            fprintf(stderr, "parse error.\n");
            exit(EXIT_FAILURE);
        }

        if ((tmp = strtok(NULL, "\t")) != NULL)
        {
            strncpy(_tmp_buf, tmp, sizeof(_tmp_buf) - 1);
            if ((sscanf(_tmp_buf, "%" SCNu16, &tcp_window)) != 1)
            {
                fprintf(stderr, "parse error.\n");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            fprintf(stderr, "parse error.\n");
            exit(EXIT_FAILURE);
        }

        if ((tmp = strtok(NULL, "\t")) != NULL)
        {
            strncpy(_tmp_buf, tmp, sizeof(_tmp_buf) - 1);
            if ((sscanf(_tmp_buf, "%" SCNu32, &tcp_seq)) != 1)
            {
                fprintf(stderr, "parse error.\n");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            fprintf(stderr, "parse error.\n");
            exit(EXIT_FAILURE);
        }

        insert(root, tmp_fn, _start, _end, ip_id, tcp_sport, tcp_dport, tcp_window, tcp_seq);
    }

    if (line != NULL)
        free(line);

    fclose(fp);
}

unsigned short ip_cksum(unsigned short *ptr, int len)
{
    int sum           = 0;
    unsigned short *w = ptr;
    int nleft         = len;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return (~sum);
}

unsigned short tcp_cksum(const unsigned char *packet, int packet_len)
{
    struct iphdr *iph   = (struct iphdr *)packet;
    struct tcphdr *tcph = (struct tcphdr *)(packet + (iph->ihl * 4));
    unsigned long sum   = 0;

    // Pseudo-header source address
    sum += (iph->saddr >> 16) & 0xFFFF;
    sum += (iph->saddr) & 0xFFFF;

    // Pseudo-header destination address
    sum += (iph->daddr >> 16) & 0xFFFF;
    sum += (iph->daddr) & 0xFFFF;

    // Pseudo-header protocol, "reserved", length
    sum += htons(IPPROTO_TCP);

    unsigned short tcp_len = ntohs(iph->tot_len) - (iph->ihl * 4);
    sum += htons(tcp_len);

    // Checksum the rest of the packet.
    unsigned short *tmp_ptr = (unsigned short *)tcph;
    while (tcp_len > 1)
    {
        sum += *tmp_ptr++;
        tcp_len -= 2;
    }

    // Handle trailing byte if present.
    if (tcp_len > 0)
    {
        sum += ((*tmp_ptr) & htons(0xFF00));
    }

    while (sum >> 16)
    {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    return ~(sum & 0xffff);
}

static unsigned char *make_packet(const uint32_t srcip, const uint32_t dstip, struct config *config)
{
    static unsigned char pktbuf[40]; // sizeof(iphdr) + sizeof(tcphdr)
    struct iphdr iph   = { 0 };
    struct tcphdr tcph = { 0 };

    iph.version  = 4;
    iph.ihl      = 5;
    iph.tot_len  = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
    iph.id       = htons(config->ip_id);
    iph.frag_off = 0;
    iph.ttl      = 64;
    iph.protocol = IPPROTO_TCP;
    iph.check    = 0;
    iph.saddr    = srcip;
    iph.daddr    = dstip;
    iph.check    = ip_cksum(((unsigned short *)&iph), sizeof(struct iphdr));

    tcph.source  = htons(config->tcp_sport);
    tcph.dest    = htons(config->tcp_dport);
    tcph.seq     = htonl(config->tcp_seq);
    tcph.ack_seq = 0;
    tcph.doff    = 5;
    tcph.syn     = 1;
    tcph.window  = htons(config->tcp_window);
    tcph.urg_ptr = 0;
    tcph.check   = 0;

    memcpy(pktbuf, &iph, sizeof(iph));
    memcpy(pktbuf + sizeof(iph), &tcph, sizeof(tcph));

    tcph.check = tcp_cksum(pktbuf, sizeof(pktbuf));

    memcpy(pktbuf + sizeof(iph), &tcph, sizeof(tcph));

    return pktbuf;
}

time_t get_time_at_offset(time_t start_time, time_t end_time, int offset, int total)
{
    if (start_time < 0 || end_time <= 0 || total <= 0 || offset < 0 || offset > total)
    {
        return 0;
    }

    return start_time + ((end_time - start_time) * offset) / total;
}

void print_entry(const uint32_t srcip, const uint32_t dstip, const uint32_t npkts)
{
    uint8_t s_octet[4], d_octet[4];

    for (int idx = 0; idx < 4; idx++)
    {
        s_octet[idx] = srcip >> (idx * 8);
        d_octet[idx] = dstip >> (idx * 8);
    }

    fprintf(stdout, "%d.%d.%d.%d ", s_octet[0], s_octet[1], s_octet[2], s_octet[3]);
    fprintf(stdout, "%d.%d.%d.%d ", d_octet[0], d_octet[1], d_octet[2], d_octet[3]);
    fprintf(stdout, "%d\n", npkts);
}

void dump_matrix_to_pcap(void *blob_data, size_t blob_size, struct global_state *pstate, struct config *config)
{
    GrB_Matrix Gmat;
    GxB_Iterator iterator;
    GrB_Info info;
    int verbose                  = 0;
    int total_packets            = 0;
    time_t last_tv_sec           = 0;
    static struct pcap_pkthdr ph = { 0 };

    ph.ts.tv_sec  = 0;
    ph.ts.tv_usec = 0;
    ph.caplen     = 40;
    ph.len        = 40;

    LAGRAPH_TRY_EXIT(GrB_Matrix_deserialize(&Gmat, GrB_UINT32, blob_data, blob_size));

    GxB_Iterator_new(&iterator);

    LAGRAPH_TRY_EXIT(GxB_Matrix_Iterator_attach(iterator, Gmat, NULL));
    LAGRAPH_TRY_EXIT(GxB_Matrix_Iterator_seek(iterator, 0));

    while (info != GxB_EXHAUSTED)
    {
        GrB_Index i, j;

        GxB_Matrix_Iterator_getIndex(iterator, &i, &j);
        uint32_t aij = GxB_Iterator_get_UINT32(iterator);

        pstate->total_packets += aij;
        total_packets += aij;

        i = BSWAP(i);
        j = BSWAP(j);

        const unsigned char *pkt = make_packet(i, j, config);

        if (verbose)
        {
            print_entry(i, j, aij);
        }

        ph.ts.tv_sec  = get_time_at_offset(config->start_time, config->end_time, total_packets, WINDOWSIZE);
        ph.ts.tv_usec = total_packets / 10;

        for (int i = 0; i < aij; i++)
        {
            pcap_dump((unsigned char *)pstate->dumper, &ph, pkt);
        }

        info = GxB_Matrix_Iterator_next(iterator);
    }
    // fprintf(stderr, "total packets: %d\n", total_packets);
}

void usage(const char *name)
{
    printf("usage: %s -o OUTPUT_PCAP INPUT_FILE1 ... INPUT_FILEn\n", name);
}

int main(int argc, char *argv[])
{
    char *value = NULL;      // for getopt
    int c, ret, reqargs = 0; // for getopt
    char *tmp = NULL;
    char in_f[PATH_MAX], out_f[PATH_MAX];
    struct tm s_tm = { 0 };
    struct global_state *pstate;

    void *blob = NULL;
    FILE *fp;
    size_t filesize, n;

    pstate = calloc(1, sizeof(struct global_state));
    GrB_init(GrB_NONBLOCKING);

    while ((c = getopt(argc, argv, "Si:o:")) != -1)
    {
        switch (c)
        {
            case 'S':
                pstate->swapped = 1;
                break;
            case 'i':
                // input tsv file
                value = optarg;
                reqargs++;
                snprintf(in_f, sizeof(in_f), "%s", value);
                break;
            case 'o':
                // output file
                value = optarg;
                reqargs++;
                snprintf(out_f, sizeof(out_f), "%s", value);
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
        usage(argv[0]);
        exit(EXIT_SUCCESS);
    }

    pcap_t *pcap   = pcap_open_dead(DLT_RAW, 65535);
    pstate->dumper = pcap_dump_open(pcap, out_f);

    if (pstate->dumper == NULL)
    {
        perror("pcap_dump_open: cannot open pcap dumpfile");
        exit(1);
    }

    parse_config(&(pstate->config), in_f);

    for (struct config *c = pstate->config; c; c = c->next)
    {
        fprintf(stderr, "Processing: %s\n", c->path);

        if ((fp = fopen(c->path, "rb")) == NULL)
        {
            perror("fopen: cannot open input file");
            exit(1);
        }

        if ((strstr(c->path, ".tar") == NULL) || (strstr(c->path, ".tar") - c->path != strlen(c->path) - 4))
        {
            fseek(fp, 0L, SEEK_END);
            filesize = ftell(fp);
            fseek(fp, 0L, SEEK_SET);

            // fprintf(stderr, "file is %ld bytes\n", filesize);

            blob = malloc(filesize);
            n    = fread(blob, filesize, 1, fp);
            if (n < 1)
            {
                perror("fread");
                exit(1);
            }

            dump_matrix_to_pcap(blob, filesize, pstate, c);
            free(blob);
        }
        else
        {
            size_t tar_size;
            struct posix_tar_header th;
            unsigned char padding[512];

            fseek(fp, 0L, SEEK_END);
            tar_size = ftell(fp);
            fseek(fp, 0L, SEEK_SET);

            while (ftell(fp) < tar_size)
            {
                if (fread(&th, sizeof(th), 1, fp) < 1)
                {
                    perror("fread1");
                    exit(1);
                }

                if (strcmp(TMAGIC, th.magic) != 0)
                {
                    perror("invalid tar file");
                    exit(2);
                }

                sscanf(th.size, "%011lo", &filesize);
                // fprintf(stderr, "file '%s' is %ld bytes\n", th.name, filesize);

                blob = malloc(filesize);
                n    = fread(blob, filesize, 1, fp);
                if (n < 1)
                {
                    perror("fread2");
                    exit(1);
                }

                dump_matrix_to_pcap(blob, filesize, pstate, c);
                free(blob);

                if( ftell(fp) % 512 != 0)
                {
                    n = fread(padding, 512 - (ftell(fp) % 512), 1, fp);
                    if (ftell(fp) % 512 != 0)
                    {
                        perror("corrupt tar");
                        exit(2);
                    
                    }
                }
            }
        }
    }

    pcap_dump_close(pstate->dumper);
    pcap_close(pcap);
    free(pstate);

    fclose(fp);
}
