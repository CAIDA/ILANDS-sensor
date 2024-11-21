import sys
import analysis
import D4M.assoc as ac
import contextlib
import time
from typing import Tuple, Dict
import os

existing_stats_path = None
output_stats_path = None
print_to_log = False

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


def _process(_A: analysis.TrafficMatrix, filelist: list[str]) -> Tuple[Dict[str, float], float, float, float, float]:
    """Update provided TrafficMatrix with tar'ed grbs in given filelist, returning
        * dictionary of network-theoretic quantities,
        * how long it took to update the TrafficMatrix, and
        * how long it took to analyze the updated TrafficMatrix
    """
    start_time = time.time()
    _update_time = _A.update(filelist, return_time=True)
    _results, _analyze_time = _A.analyze(return_time=True)
    stop_time = time.time()
    print(_results)
    print(f"Files analyzed in {_analyze_time} seconds; "
          f"total packets processed = {_results['n_packets']}; "
          f"rate of packets per second = {_results['n_packets']/(stop_time - start_time)}"
          )
    _update_rate = _results['n_packets']/_update_time
    _analyze_rate = _results['n_packets']/_analyze_time
    return _results, _update_time, _analyze_time, _update_rate, _analyze_rate


def process_filelist(filename):
    filelist = []
    with open(filename, "r") as f:
        for line in f:
            filelist.append(line.split("\n")[0])

    A = analysis.TrafficMatrix()

    if print_to_log:
        log_filepath = "./GraphChallenge_logs/GraphChallenge_analysis_log_" + filename
        with open(log_filepath, "a+") as f:
            with contextlib.redirect_stdout(f):
                _process_results = _process(A, filelist)
    else:
        _process_results = _process(A, filelist)

    results, update_time, analyze_time, update_rate, analyze_rate = _process_results

    stat_names = ["n_links", "n_packets", "n_sources", "n_destinations",
                  "max_packets", "max_source_packets", "max_destination_packets",
                  "max_fan_out", "max_fan_in"
                  ]

    A.clear()
    for stat_name in stat_names:
        stats[filename, stat_name] = results[stat_name]
    stats[filename, "update_time"] = update_time
    stats[filename, "analyze_time"] = analyze_time
    stats[filename, "update_rate"] = update_rate
    stats[filename, "analyze_rate"] = analyze_rate
    ac.writecsv(stats, output_stats_path, delimiter="\t")

    return None


if __name__ == '__main__':
    process_filelist(sys.argv[1])
