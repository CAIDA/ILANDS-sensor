import analytics
import analysis
import graphblas as gb
import D4M.assoc as ac
import os
import sys
import contextlib
import time

# USER DEFINED VARIABLES
# LLsub_params - parameter used in MIT SuperCloud LLsub triples mode, e.g., (1,1,48);
#    (# of nodes, # of processes per node, # of threads per process); 
#    used as a suffix applied to the file path to distinguish between runs;
#    replace with None to suppress
# existing_stats_path - filepath to existing TSV with previously processed statistics, 
#    in case doing multiple runs where conflicts might otherwise occur
# output_stats_path - filepath to where the processed statistics should be stored
# print_to_log - Boolean indicating if log files should be created tracking grbs processed

LLsub_params = "(1,1,48)"
existing_stats_path = "stats.tsv"
output_stats_path = "stats_hpec_benchmarking.tsv"
print_to_log = True

my_task_id = int(sys.argv[1])  # parameters passed in by scheduler
num_tasks = int(sys.argv[2])

try:
    stats = ac.readcsv(existing_stats_path, labels=True, convert_values=True, delimiter="\t")
except (StopIteration, FileNotFoundError) as e:
    stats = ac.Assoc([], [], [])

file_list = []
prefix = "./../../data/CAIDA_GrB_anon/2022/"
for directory in os.listdir(prefix):
    for subdirectory in os.listdir(prefix + directory):
        file_list.append(prefix + directory + "/" + subdirectory)

print(file_list)

file_list.sort()

file_sublist = file_list[my_task_id:len(file_list):num_tasks]

stat_names = ["n_links", "n_packets", "n_sources", "n_destinations", 
              "max_packets", "max_source_packets", "max_destination_packets",
              "max_fan_out", "max_fan_in"
             ]
already_processed = stats.row

A = analysis.TrafficMatrix()

for directory in file_sublist:
    if LLsub_params is None:
        dir_row_label = directory
    else:
        dir_row_label = directory + "|" + LLsub_params
    if dir_row_label in already_processed:
        continue
    else:
        print(dir_row_label)
        filename = directory.split("/")[-1]
        if print_to_log:
            if LLsub_params is None:
                log_filepath = "./LLsub_logs/GraphChallenge_analysis_log_" + filename
            else:
                log_filepath = "./LLsub_logs/GraphChallenge_analysis_log_" + filename + "|" + LLsub_params
            with open(log_filepath, "a+") as f:
                with contextlib.redirect_stdout(f):
                    start_time = time.time()
                    update_time = A.update(directory, return_time=True)
                    results, analyze_time = A.analyze(return_time=True)
                    stop_time = time.time()
                    print(results)
                    print(f"{directory} analyzed in {analyze_time} seconds; "
                          f"total packets processed = {results['n_packets']}; "
                          f"rate of packets per second = {results['n_packets']/(stop_time - start_time)}"
                         )
        else:
            start_time = time.time()
            update_time = A.update(directory, return_time=True)
            results, analyze_time = A.analyze(return_time=True)
            stop_time = time.time()
            print(results)
            print(f"{directory} analyzed in {analyze_time} seconds; "
                      f"total packets processed = {results['n_packets']}; "
                      f"rate of packets per second = {results['n_packets']/(stop_time - start_time)}"
                     )
        
        A.clear()
        for stat_name in stat_names:
            stats[dir_row_label, stat_name] = results[stat_name]
        stats[dir_row_label, "update_time"] = update_time
        stats[dir_row_label, "analyze_time"] = analyze_time
        ac.writecsv(stats, output_stats_path, delimiter="\t")
