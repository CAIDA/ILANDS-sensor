#!/bin/bash

# Load Module
source /etc/profile
module load anaconda/2023b

echo "My task ID: " $LLSUB_RANK
echo "Number of tasks: " $LLSUB_SIZE

python GraphChallenge_analyses_hpec_benchmarking.py $LLSUB_RANK $LLSUB_SIZE
