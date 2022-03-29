#include <GraphBLAS.h>

/* borrowed from LAGraph */
#define LAGRAPH_TRY_EXIT(method)                                                             \
  {                                                                                          \
    GrB_Info info = (method);                                                                \
    if (!(info == GrB_SUCCESS))                                                              \
    {                                                                                        \
      fprintf(stderr, "LAGraph error: [%d]\nFile: %s Line: %d\n", info, __FILE__, __LINE__); \
      exit(1);                                                                               \
    }                                                                                        \
  }

int main(int argc, const char **argv)
{

  GrB_init(GrB_NONBLOCKING);
  GrB_Matrix Gmat;
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

  GxB_fprint(Gmat, 2, NULL);
}
