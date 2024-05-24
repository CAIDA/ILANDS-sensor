cmake_minimum_required(VERSION 3.18.0)
project(GraphBLAS-Network-Tools VERSION 0.1.0 LANGUAGES C)

include_directories(../include "${GRAPHBLAS_INCLUDE_DIR}")

# The version of CryptopANT we're including uses interfaces deprecated since OpenSSL 3.0, suppress those warnings.
add_compile_options(-Wno-deprecated-declarations)

add_subdirectory(pcap)
add_subdirectory(util)
add_subdirectory(suricata)

if(LIBDPDK_FOUND)
    add_subdirectory(dpdk)
endif(LIBDPDK_FOUND)

if(HAVE_LIBTRACE_H)
    add_subdirectory(libtrace)
endif()


