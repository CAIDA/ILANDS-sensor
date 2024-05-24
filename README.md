# GraphBLAS Network Tools

Prototype utilities for generating GraphBLAS network traffic matrices from various input sources.
In most cases, detailed program usage can be determined by running a specific program with no arguments.
More detailed information is provided in the subdirectories below 'src'.

SuiteSparse:GraphBLAS v7.2.0 or higher is required to build and run all code in this repository.
It can be downloaded here: https://github.com/DrTimothyAldenDavis/GraphBLAS

Additionally, support for prefix-preserving anonymization is provided by the CryptopANT library, which will
automatically be downloaded and extracted if missing, from: 
https://ant.isi.edu/software/cryptopANT/cryptopANT-1.2.2.tar.gz

Finally, the 'yyjson' library is included as-is with it's C source and header, retrieved from:
https://github.com/ibireme/yyjson

To build this code on a system, simply do the following in this directory:

    make

To remove all compiled code:

    make clean

## Folders in the 'src' directory:


           

dpdk     - Example code receiving packets on an interface supported by the Intel Data Plane Development Kit (DPDK).  Processes, converts and serializes GraphBLAS matrices only, does not currently save them.
Requires DPDK v21+ to build and install. Reference: [Hypersparse Traffic Matrix Construction using GraphBLAS on a DPU](https://doi.org/10.48550/arXiv.2310.18334) (IEEE HPEC 2023)

libtrace - Receives packets on an interface supported by the parallel LibTrace API and saves the resulting 
           GraphBLAS matrices into a specificed directory.  Requires LibTrace 4.0+ to build and install.
           (https://github.com/LibtraceTeam/libtrace). Reference: [Deployment of Real-Time Network Traffic Analysis using GraphBLAS Hypersparse Matrices and D4M Associative Arrays](https://doi.org/10.48550/arXiv.2309.02464) (IEEE HPEC 2023) 

pcap     - Converts a standard .pcap file into GraphBLAS network traffic matrices.
           Requires libpcap v0.8+ to build and install. Reference: [Focusing and Calibration of Large Scale Network Sensors using GraphBLAS Anonymized Hypersparse Matrices](https://doi.org/10.48550/arXiv.2309.01806) (IEEE HPEC 2023)

suricata - Converts Suricata EVE JSON 'flow' format data from either a flat file or received on a UNIX domain
           socket to GraphBLAS network traffic matrices. 

util     - Various utilities for manipulating both serialized GraphBLAS matrices and .tar archives containing
           them.

accolade - DEPRECATED.  Read packets received on an Accolade FPGA adapter and save them into serialized GraphBLAS matrices.    Reference: [GraphBLAS on the Edge: Anonymized High Performance Streaming of Network](https://doi.org/10.48550/arXiv.2203.13934) (IEEE HPEC 2022)


