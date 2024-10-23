import analysis
import D4M.assoc as ac
import os
import contextlib
import time
from typing import Tuple, Dict

# USER DEFINED VARIABLES
# existing_stats_path - filepath to existing TSV with previously processed statistics, 
#   in case doing multiple runs where conflicts might otherwise occur
# output_stats_path - filepath to where the processed statistics should be stored
# tar_filepath_prefix - directory where subdirectories containing the tar'ed grb files can be found
#   assumed directory structure is of the form
#       /data
#           |- /CAIDA_GrB_anon
#           |       |- /2022
#           |       |   |- /directory_1
#           |       |   |   |- /subdirectory_11
#           |       |   |   |   |- grbs_111.tar
#           |       |   |   |   |- grbs_112.tar
#           |       |   |   |- /subdirectory_12
#           |       |   |   |   |- grbs_121.tar
#           |       |   |   |   |- grbs_122.tar
#           |       |   |   |-...
#           |       |   |- /directory_2
#           |       |   |   |- /subdirectory_21
#           |       |   |   |   |- grbs_211.tar
#           |       |   |   |   |- grbs_212.tar
#           |       |   |   |- /subdirectory_22
#           |       |   |   |   |- grbs_221.tar
#           |       |   |   |   |- grbs_222.tar
#           |       |   |   |-...
#           |       |   |-...
#           |       |-...
#       /code
#           |- analysis.py
#           |- analytics.py
#           |- GraphChallenge_analyses_serial.py (this file)
#           |-...
# print_to_log - Boolean indicating if log files should be created tracking grbs processed;
#   useful for tracking fine-tuned timing stats, assumes presence of a ./GraphChallenge_logs subdirectory

existing_stats_path = None
output_stats_path = None
tar_filepath_prefix = None
print_to_log = False

# defaults
if existing_stats_path is None:
    stats = ac.Assoc([], [], [])
else:
    assert isinstance(existing_stats_path, str)
    try:
        stats = ac.readcsv(existing_stats_path, labels=True, convert_values=True, delimiter="\t")
    except (StopIteration, FileNotFoundError) as e:
        stats = ac.Assoc([], [], [])

if output_stats_path is None:
    output_stats_path = "./stats.tsv"

if tar_filepath_prefix is None:
    tar_filepath_prefix = "./../data/CAIDA_GrB_anon/2022/"

# generate file list assuming directory structure of tar_filepath_prefix/directory/subdirectory/grbs.tar
file_list = []
for directory in os.listdir(tar_filepath_prefix):
    for subdirectory in os.listdir(tar_filepath_prefix + directory):
        file_list.append(tar_filepath_prefix + directory + "/" + subdirectory)
        
print(file_list)

stat_names = ["n_links", "n_packets", "n_sources", "n_destinations", 
              "max_packets", "max_source_packets", "max_destination_packets", 
              "max_fan_out", "max_fan_in"
              ]

already_processed = stats.row  # get any files that have already been processed according to the provided stats TSV

A = analysis.TrafficMatrix()


def _process(_A: analysis.TrafficMatrix, _directory: str) -> Tuple[Dict[str, float], float, float]:
    """Update provided TrafficMatrix with tar'ed grbs in given directory, returning
        * dictionary of network-theoretic quantities,
        * how long it took to update the TrafficMatrix, and
        * how long it took to analyze the updated TrafficMatrix
    """
    start_time = time.time()
    _update_time = _A.update(_directory, return_time=True)
    _results, _analyze_time = _A.analyze(return_time=True)
    stop_time = time.time()
    print(_results)
    print(f"{_directory} analyzed in {_analyze_time} seconds; "
          f"total packets processed = {_results['n_packets']}; "
          f"rate of packets per second = {_results['n_packets']/(stop_time - start_time)}"
          )
    return _results, _update_time, _analyze_time


for directory in file_list:
    print(directory)
    filename = directory.split("/")[-1]
    row_label = filename  # labels row in stats.tsv by the directory containing the tar'ed grbs
    if row_label in already_processed:
        continue
    else:
        print(row_label)
        if print_to_log:
            log_filepath = "./GraphChallenge_logs/GraphChallenge_analysis_log_" + filename
            with open(log_filepath, "a+") as f:
                with contextlib.redirect_stdout(f):
                    results, update_time, analyze_time = _process(A, directory)
        else:
            results, update_time, analyze_time = _process(A, directory)
        
        A.clear()
        for stat_name in stat_names:
            stats[row_label, stat_name] = results[stat_name]
        stats[row_label, "update_time"] = update_time
        stats[row_label, "analyze_time"] = analyze_time
        ac.writecsv(stats, output_stats_path, delimiter="\t")
