# LibTrace utilities

This program accepts network packets input from any source supported by the LibTrace API.

Output path should be a directory to which .tar files containing GraphBLAS matrices are saved.

Optionally accepts a path to a precomputed IPv4 anonymization table, generated with 'makecache'
under the 'utils' directory.

    ./trace2grb [-c <path to IP anonymization table>] [-t threads] -o <output directory> LIBTRACE-URI

Example:

    ./trace2grb -c /data/anon_table -t 8 ndag:ens4,225.44.0.1,44000 -o /scratch/output
Reference: [Deployment of Real-Time Network Traffic Analysis using GraphBLAS Hypersparse Matrices and D4M Associative Arrays](https://doi.org/10.48550/arXiv.2309.02464) (IEEE HPEC 2023)

