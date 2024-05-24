# Accolade

Reference: [GraphBLAS on the Edge: Anonymized High Performance Streaming of Network](https://doi.org/10.48550/arXiv.2203.13934) (IEEE HPEC 2022)

**DEPRECATED: This code REQUIRES the Accolade FPGA SDK, which can be obtained from Accolade Technology, Inc.**

anic2grb - Read packets received on an Accolade FPGA adapter and save them into serialized GraphBLAS matrices.
           Dependencies: libpcap >=0.8, GraphBLAS >= 6.1.0, cryptoPANT >= 1.2.2 (see below), Accolade SDK.

1) CryptoPANT files can be downloaded here: https://ant.isi.edu/software/cryptopANT/index.html

Version 1.2.2 was used in our testing.
Place both cryptoPANT.c and cryptoPANT.h from the cryptoPANT source release into this directory to build.

# wget https://ant.isi.edu/software/cryptopANT/cryptopANT-1.2.2.tar.gz
# tar --strip=2 -xf cryptopANT*gz --wildcards '*/src/cryptopANT.*'

2) SuiteSparse:GraphBLAS version 6.1.0 or higher is required to run all code.
It can be downloaded here: https://github.com/DrTimothyAldenDavis/GraphBLAS

3) Place anic_libpcap.c from the 'examples' directory in the Accolade SDK in this directory to build.
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

