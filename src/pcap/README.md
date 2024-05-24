# libpcap tools

pcap2grb - This program takes a single input .pcap file and generates GraphBLAS network traffic matrices into a
specified output directory.

Optionally, the path to a CryptopANT anonymization key can be provided to perform prefix-preserving
IP address anonymization.  The final 16 bits of the network address are masked in this mode.  If the
file specified does not already exist, a random key will be generated and saved with that name.

Usage:

    ./pcap2grb [-a anonymize.key] -i INPUT_PCAP_FILE -o OUTPUT_DIRECTORY

Example:

    ./pcap2grb -a anon.key -i dump.pcap -o /scratch/outdir
Reference: [Focusing and Calibration of Large Scale Network Sensors using GraphBLAS Anonymized Hypersparse Matrices](https://doi.org/10.48550/arXiv.2309.01806) (IEEE HPEC 2023)