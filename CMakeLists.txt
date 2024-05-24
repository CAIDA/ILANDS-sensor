cmake_minimum_required(VERSION 3.18.0)
project(GraphBLAS-Network-Tools VERSION 0.1.0 LANGUAGES C)

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckLibraryExists)

# Emit compilation commands.
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 7)
    message(FATAL_ERROR "GCC version >= 7.5 required.  Fix environment and 'make clean'.")
endif()

# Point to Suricata source location.
set(SURICATA_SOURCE_DIR "../../suricata-6.0.16/src")

set(CRYPTOPANT_SOURCE_URL "https://ant.isi.edu/software/cryptopANT/cryptopANT-1.2.2.tar.gz")
set(CRYPTOPANT_DOWNLOAD_PATH "${CMAKE_BINARY_DIR}/cryptopANT-1.2.2.tar.gz")
set(CRYPTOPANT_EXTRACT_DIR "${CMAKE_BINARY_DIR}/unpack")

set(CRYPTOPANT_FILES "${CMAKE_SOURCE_DIR}/src/extern/cryptopANT.c" "${CMAKE_SOURCE_DIR}/include/cryptopANT.h")

function(check_if_files_exist file_list retval)
    set(result TRUE)
    foreach(file IN LISTS file_list)
        if(NOT EXISTS "${file}")
            message("Can't find ${file}")
            set(result FALSE)
            break()
        endif()
    endforeach()
    set(${retval} ${result} PARENT_SCOPE)
endfunction()

check_if_files_exist("${CRYPTOPANT_FILES}" have_cryptopant)

if(NOT have_cryptopant)
    file(DOWNLOAD ${CRYPTOPANT_SOURCE_URL} ${CRYPTOPANT_DOWNLOAD_PATH} STATUS download_status SHOW_PROGRESS)
    file(ARCHIVE_EXTRACT INPUT ${CRYPTOPANT_DOWNLOAD_PATH} DESTINATION ${CRYPTOPANT_EXTRACT_DIR})
    file(COPY "${CRYPTOPANT_EXTRACT_DIR}/cryptopANT-1.2.2/src/cryptopANT.c" DESTINATION "${CMAKE_SOURCE_DIR}/src/extern")
    file(COPY "${CRYPTOPANT_EXTRACT_DIR}/cryptopANT-1.2.2/src/cryptopANT.h" DESTINATION "${CMAKE_SOURCE_DIR}/include")
endif()

# Set to the location of your GraphBLAS installation if needed - default is /usr/local
list(APPEND CMAKE_PREFIX_PATH /usr/local/lib/cmake)
list(APPEND CMAKE_PREFIX_PATH /home/gridsan/mjones/GraphBLAS/usr/local/lib/cmake)

find_package(PkgConfig REQUIRED)
find_package(GraphBLAS REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(LIBDPDK libdpdk-libs)
if(LIBDPDK_FOUND)
    message(STATUS "DPDK is present, compiling dpdk2grb")
endif()

pkg_check_modules(PCAP libpcap)
if(NOT PCAP_FOUND)
    set(PCAP_LIBRARIES "-lpcap")
endif()

check_include_files("csv.h" HAVE_CSV_H)
check_include_files("libtrace_parallel.h" HAVE_LIBTRACE_H)

add_subdirectory(src)

set(CPACK_PACKAGE_NAME "GraphBLASNetworkTools")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_CONTACT "None <none@none.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GraphBLAS Network Tools")
set(CPACK_RPM_PACKAGE_AUTOREQ "no")
include(CPack)
