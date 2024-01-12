#include <GraphBLAS.h>

#define LAGRAPH_TRY_EXIT(method)                                                                   \
    {                                                                                              \
        info = (method);                                                                           \
        if (!(info == GrB_SUCCESS))                                                                \
        {                                                                                          \
            fprintf(stderr, "LAGraph error: [%d]\nFile: %s Line: %d\n", info, __FILE__, __LINE__); \
            exit(1);                                                                               \
        }                                                                                          \
    }

int main(int argc, const char **argv)
{

    GrB_init(GrB_NONBLOCKING);
    GrB_Matrix Gmat;
    GxB_Iterator iterator;
    GrB_Info info;

    void *blob = NULL;
    GrB_Index blob_size = 0;
    FILE *fp;
    size_t filesize;
    size_t n;

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

    fseek(fp, 0L, SEEK_END);
    filesize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    blob = malloc(filesize);

    fprintf(stderr, "file is %ld bytes\n", filesize);

    n = fread(blob, filesize, 1, fp);
    if (n < 1)
    {
        perror("fread");
        exit(1);
    }

    LAGRAPH_TRY_EXIT(GrB_Matrix_deserialize(&Gmat, GrB_UINT32, blob, filesize));

    GxB_Iterator_new(&iterator);

    LAGRAPH_TRY_EXIT(GxB_Matrix_Iterator_attach(iterator, Gmat, NULL));
    LAGRAPH_TRY_EXIT(GxB_Matrix_Iterator_seek(iterator, 0));

    while (info != GxB_EXHAUSTED)
    {
        GrB_Index i, j;
        uint8_t s_octet[4], d_octet[4];

        GxB_Matrix_Iterator_getIndex(iterator, &i, &j);

        for (int idx = 0; idx < 4; idx++)
        {
            s_octet[idx] = i >> (idx * 8);
            d_octet[idx] = j >> (idx * 8);
        }
        // fprintf(stderr, "%" PRIu64 " %" PRIu64 "\n", i, j);
        fprintf(stdout, "%d.%d.%d.%d ", s_octet[0], s_octet[1], s_octet[2], s_octet[3]);
        fprintf(stdout, "%d.%d.%d.%d\n", d_octet[0], d_octet[1], d_octet[2], d_octet[3]);
        uint32_t aij = GxB_Iterator_get_UINT32(iterator);
        info = GxB_Matrix_Iterator_next(iterator);
    }
}
