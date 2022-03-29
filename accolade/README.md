Code used in "GraphBLAS on the Edge: High Performance Streaming of Network Traffic".
https://arxiv.org/abs/2203.13934

anic2grb - Read packets received on an Accolade FPGA adapter and save them into serialized GraphBLAS matrices.
           Dependencies: libpcap >=0.8, GraphBLAS >= 6.1.0, cryptoPANT >= 1.2.2 (see below), Accolade SDK.

pcap2grb - Read packets from a pcap file and save them into serialized GraphBLAS matrices.
           Dependencies: libpcap >=0.8, GraphBLAS >= 6.1.0, cryptoPANT >= 1.2.2 (see below)

gbdeserialize - Print the header and first few entries of a serialized GraphBLAS matrix.

makecache - Create a 2^32 IPv4 lookup table to speedup CryptoPAN anonymization.

1) CryptoPANT files can be downloaded here: https://ant.isi.edu/software/cryptopANT/index.html

Version 1.2.2 was used in our testing.
Place both cryptoPANT.c and cryptoPANT.h from the cryptoPANT source release into this directory to build.

# wget https://ant.isi.edu/software/cryptopANT/cryptopANT-1.2.2.tar.gz
# tar --strip=2 -xf cryptopANT*gz --wildcards '*/src/cryptopANT.*'

2) SuiteSparse:GraphBLAS version 6.1.0 or higher is required to run all code.
It can be downloaded here: https://github.com/DrTimothyAldenDavis/GraphBLAS
Latest as of this README is v6.2.5:

# wget https://github.com/DrTimothyAldenDavis/GraphBLAS/archive/refs/tags/v6.2.5.tar.gz
# tar xf v6.2.5.tar.gz
# cd GraphBLAS-*
# make
# sudo make install

Compilation and installation of GraphBLAS may fail if fairly recent versions of cmake and gcc are not available; see GraphBLAS GitHub linked above for detailed instructions.

3) Building the Accolade version (anic2grb) requires installation of the Accolade SDK, which can be obtained from Accolade Technology, Inc.
Place anic_libpcap.c from the 'examples' directory in the Accolade SDK in this directory to build.
Replace SDK_1_2_20200812.tgz with your version of the Accolade SDK in the command below.  Alternatively, copy it from your installation directory.
If the Accolade SDK is unavailable, only the 'anic2grb' binary will fail to build and this can be ignored.

# tar --strip=2 -xf SDK_1_2_20200812.tgz --wildcards '*/examples/anic_libpcap.c'

4) Finally, libpcap >= 0.8 is required:

CentOS/RHEL:

# yum -y install libpcap-devel

Debian/Ubuntu:

# apt install libpcap-dev

Edit the Makefile to point to the location of your GraphBLAS / Accolade SDK installations if necessary.

To build the software, simply type 'make':

# make

