import analytics
import analysis
import graphblas as gb
import D4M.assoc as ac
import os
import sys
import contextlib
import time

my_task_id = int(sys.argv[1])
num_tasks = int(sys.argv[2])

print_to_log = True

stats = ac.readcsv("stats.tsv", labels=True, convert_values=True, delimiter="\t")

file_list = []
prefix = "./../../data/CAIDA_GrB_anon/2022/"
for directory in os.listdir(prefix):
    for subdirectory in os.listdir(prefix + directory):
        file_list.append(prefix + directory + "/" + subdirectory)
        
file_list.sort()

file_sublist = file_list[my_task_id:len(file_list):num_tasks]
print(file_sublist)
        
stat_names = ["n_links", "n_packets", "n_sources", "n_destinations", 
              "max_packets", "max_source_packets", "max_destination_packets",
              "max_fan_out", "max_fan_in"
             ]
already_processed = stats.row
for directory in file_sublist:
    if directory in already_processed:
        continue
    else:
        print(directory)
        filename = directory.split("/")[-1]
        A = analysis.TrafficMatrix()
        if print_to_log:
            with open("GraphChallenge_analysis_log_" + filename, "a+") as f:
                with contextlib.redirect_stdout(f):
                    start_time = time.time()
                    A.update(directory)
                    results = A.analyze()
                    stop_time = time.time()
                    print(results)
                    print(f"{directory} processed in {stop_time - start_time} seconds; "
                          f"total packets processed = {A.n_packets}; "
                          f"rate of packets per second = {A.n_packets/(stop_time - start_time)}"
                         )
        else:
            start_time = time.time()
            A.update(directory)
            results = A.analyze()
            stop_time = time.time()
            print(results)
            print(f"{directory} processed in {stop_time - start_time} seconds; "
                      f"total packets processed = {A.n_packets}; "
                      f"rate of packets per second = {A.n_packets/(stop_time - start_time)}"
                     )
            
        for stat_name in stat_names:
            stats[directory, stat_name] = results[stat_name]
        ac.writecsv(stats, "stats.tsv", delimiter="\t")