import GraphChallenge_analyses_subrange as gb_analysis 
import contextlib
import sys

# USER DEFINED VARIABLES
# LLsub_params - parameter used in MIT SuperCloud LLsub triples mode, e.g., (1,1,48);
#    (# of nodes, # of processes per node, # of threads per process); 
#    used as a suffix applied to the file path to distinguish between runs;
# filename - .txt file containing newline separated paths to tarballs containing graphblas matrices
# subrange - .tar file containing .grb matrices that specify the subrange 

LLsub_params = '(1,3,16)'
filename = "./filegroupings.txt" 
subrange = "./../src/util/IPlist/NonRoutableBogonIPList-LE-2.tar"

my_task_id = int(sys.argv[1])  # parameters passed in by scheduler
num_tasks = int(sys.argv[2])

file_list = []
with open(filename, "r") as f:
    for line in f:
        file_list.append(line.split("\n")[0])

file_list.sort()
file_sublist = file_list[my_task_id:len(file_list):num_tasks]

for file in file_sublist:
    # file is the path to a filelist containing .tar files to process
    row_label = file.split("/")[-1] + "|" + LLsub_params
    log_filepath = "./logs/GraphChallenge_analysis_log_" + row_label

    # open log_filepath to print to
    with open(log_filepath, "a+") as f:
        with contextlib.redirect_stdout(f):
            gb_analysis.process_filelist(file, subrange_tarfile=subrange, benchmark_name=row_label, save_range=True)
