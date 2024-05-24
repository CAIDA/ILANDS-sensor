# DPDK utilities

This program will read packets from multiple RSS queues on an interface supported by the DPDK and emit
timing data for the rate at which matrices can be serialized from network input.

EAL arguments from the DPDK are supported.  

This code is incomplete.

Example to run on a device located at PCI 03:00.0, with 4 cores:

    ./dpdk2grb -a 03:00.0,representor=[0,65535] -l 0-4 

For a complete list of EAL arguments:
    
    ./dpdk2grb --help

Reference: [Hypersparse Traffic Matrix Construction using GraphBLAS on a DPU](https://doi.org/10.48550/arXiv.2310.18334) (IEEE HPEC 2023)