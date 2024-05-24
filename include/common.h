#include <sys/time.h>
#include <netinet/in.h>
#include <net/ethernet.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#ifndef NO_GRAPHBLAS_DEBUG
#include <GraphBLAS.h>
#endif

#define WINDOWSIZE (1 << 23)
#define SUBWINSIZE (1 << 17)
#define PKTBUFSIZE (1 << 17)

#define LINEBUFSIZE 1024

#define BSWAP(a) (pstate->swapped ? ntohl(a) : (a))

struct posix_tar_header
{                     /* byte offset */
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

// If at first you don't succeed, just abort.
#ifndef NO_GRAPHBLAS_DEBUG
#ifndef LAGRAPH_TRY_EXIT
#define LAGRAPH_TRY_EXIT(method)                                                             \
  {                                                                                          \
    GrB_Info info = (method);                                                                \
    if (!(info == GrB_SUCCESS))                                                              \
    {                                                                                        \
      fprintf(stderr, "LAGraph error: [%d]\nFile: %s Line: %d\n", info, __FILE__, __LINE__); \
      exit(1);                                                                    \
    }                                                                                        \
  }
#endif
#endif

// Convenience macros to time code sections.
#ifndef NDEBUG
#define TIC(clocktype, msg)                                                  \
  {                                                                          \
    double t_ts;                                                             \
    pid_t _mytid = syscall(SYS_gettid);                                      \
    clock_gettime(clocktype, &ts_start);                                     \
    t_ts = ((ts_start.tv_sec * 1e9) + (ts_start.tv_nsec)) * 1e-9;            \
    fprintf(stderr, "[%d] [%.2f] [%s] Section begin.\n", _mytid, t_ts, msg); \
  }

#define TOC(clocktype, msg)                                                            \
  {                                                                                    \
    double t_elapsed;                                                                  \
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

// Extract IP head length and version.
#define IP_HL(ip) (((ip)->ip_vhl) & 0x0f)
#define IP_V(ip) (((ip)->ip_vhl) >> 4)

// Calculate time elapsed between two struct timespec, return struct timespec.
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

/// @brief Find the IPv4 header in a packet (look for ETHERTYPE_IP).
/// @param base Beginning of packet Ethernet header. (uint8_t)
/// @return Pointer to the beginning of the IPv4 header, or NULL if not found.
static inline const uint8_t *find_iphdr(const uint8_t *base)
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
