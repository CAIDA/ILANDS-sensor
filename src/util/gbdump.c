#include <GraphBLAS.h>

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

#define LAGRAPH_TRY_EXIT(method)                                                                                       \
    {                                                                                                                  \
        info = (method);                                                                                               \
        if (!(info == GrB_SUCCESS))                                                                                    \
        {                                                                                                              \
            fprintf(stderr, "LAGraph error: [%d]\nFile: %s Line: %d\n", info, __FILE__, __LINE__);                     \
            exit(1);                                                                                                   \
        }                                                                                                              \
    }

void dump_matrix(void *blob_data, size_t blob_size, int verbose)
{
    GrB_Matrix Gmat;
    GxB_Iterator iterator;
    GrB_Info info;
    int total_packets = 0;

    LAGRAPH_TRY_EXIT(GrB_Matrix_deserialize(&Gmat, GrB_UINT32, blob_data, blob_size));

    GxB_Iterator_new(&iterator);

    LAGRAPH_TRY_EXIT(GxB_Matrix_Iterator_attach(iterator, Gmat, NULL));
    LAGRAPH_TRY_EXIT(GxB_Matrix_Iterator_seek(iterator, 0));

    while (info != GxB_EXHAUSTED)
    {
        GrB_Index i, j;
        uint8_t s_octet[4], d_octet[4];

        GxB_Matrix_Iterator_getIndex(iterator, &i, &j);
        uint32_t aij = GxB_Iterator_get_UINT32(iterator);
        total_packets += aij;

        if (verbose)
        {
            for (int idx = 0; idx < 4; idx++)
            {
                s_octet[idx] = i >> (idx * 8);
                d_octet[idx] = j >> (idx * 8);
            }

            // fprintf(stderr, "%" PRIu64 " %" PRIu64 "\n", i, j);
            fprintf(stdout, "%d.%d.%d.%d ", s_octet[0], s_octet[1], s_octet[2], s_octet[3]);
            fprintf(stdout, "%d.%d.%d.%d ", d_octet[0], d_octet[1], d_octet[2], d_octet[3]);
            fprintf(stdout, "%d\n", aij);
        }

        info = GxB_Matrix_Iterator_next(iterator);
    }
    fprintf(stdout, "total packets: %d\n", total_packets);
}

int main(int argc, const char **argv)
{
    void *blob = NULL;
    FILE *fp;
    size_t filesize;
    size_t n;

    GrB_init(GrB_NONBLOCKING);

    if (argc < 2)
    {
        fprintf(stderr, "need one argument (GraphBLAS serialized matrix file)\n");
        exit(1);
    }

    if ((fp = fopen(argv[1], "rb")) == NULL)
    {
        perror("fopen");
        exit(1);
    }

    if ((strstr(argv[1], ".tar") == NULL) || (strstr(argv[1], ".tar") - argv[1] != strlen(argv[1]) - 4))
    {
        fseek(fp, 0L, SEEK_END);
        filesize = ftell(fp);
        fseek(fp, 0L, SEEK_SET);

        fprintf(stderr, "file is %ld bytes\n", filesize);

        blob = malloc(filesize);
        n    = fread(blob, filesize, 1, fp);
        if (n < 1)
        {
            perror("fread");
            exit(1);
        }

        dump_matrix(blob, filesize, 1);
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
            fprintf(stderr, "file '%s' is %ld bytes\n", th.name, filesize);

            blob = malloc(filesize);
            n    = fread(blob, filesize, 1, fp);
            if (n < 1)
            {
                perror("fread2");
                exit(1);
            }

            dump_matrix(blob, filesize, 0);

            n = fread(padding, 512 - (ftell(fp) % 512), 1, fp);
            if (ftell(fp) % 512 != 0)
            {
                perror("corrupt tar");
                exit(2);
            }
        }
    }

    fclose(fp);
}
