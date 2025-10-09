# Python Implementation of Anonymized Network Sensing Graph Challenge

This is an implementation of the [Anonymized Network Sensing Graph Challenge](https://graphchallenge.mit.edu/challenges) in Python using the [python-graphblas](https://github.com/python-graphblas/python-graphblas) interface to the [SuiteSparse:GraphBLAS](https://github.com/DrTimothyAldenDavis/GraphBLAS) library and [D4M for Python](https://github.com/Accla/D4M.py).

## Prerequisites: 
  - Python >= 3.9
  - python-graphblas (and dependencies)
  - D4M for Python (and dependencies)

## Installation instructions:

    pip install python-graphblas
    git clone https://github.com/Accla/D4M.py.git
    cd D4M.py
    python setup.py install

## Usage:

Most users will want to run GraphChallenge_analyses_filelist.py, which takes as input a list of files which are either serialized GraphBLAS matrices or UNIX tar files comprised exclusively of serialized GraphBLAS matrices as produced by the other tools in the top-level 'src' directory which represent network traffic flows, computes a variety of network statistics on them, and outputs the results both on the command line and into a file called 'stats.tsv'.

    python GraphChallenge_analyses_filelist.py <filelist.txt> [optional path to tar file containing subrange matrices]

An example filelist.txt contents might look like:

    /data/tar/2024/10/22/03/20241022-035742-648596.8388608.tar
    /data/tar/2024/10/22/03/20241022-035824-301860.8388608.tar
    /data/tar/2024/10/22/03/20241022-035903-972284.8388608.tar

Subrange matrices are created from lists of CIDR network ranges with the 'iplist2grb' utility under 'src'.



