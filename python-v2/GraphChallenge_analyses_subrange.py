import sys
import time
import os
import tarfile
from typing import Tuple, Dict, List, Optional
import graphblas as gb
import D4M.assoc as ac
import tar_matrix_processing as tar_matrix_processing

EXISTING_STATS_PATH = './stats.tsv'         # existing data to read in
OUTPUT_STATS_PATH = "./stats.tsv"           # where to output the stats from this analysis to

STAT_NAMES = [
    "n_links", "n_packets", "n_sources", "n_destinations", "max_packets",
    "max_source_packets", "max_destination_packets", "max_fan_out", "max_fan_in",
]

try:
    stats = ac.readcsv(EXISTING_STATS_PATH, labels=True, convert_values=True, delimiter="\t")
except FileNotFoundError:
    stats = ac.Assoc([], [], [])            # make new Associative array if there is no such file


def process_filelist(filename: str, subrange_tarfile: Optional[str] = None, benchmark_name: Optional[str] = None, save_range: Optional[bool] = False) -> None:
    """
    Process the file list and analyze the traffic matrix.
    Args:
        filename: Path to text file that contains a list of tar files to process. The list of tar 
                files are delineated by new lines. 
        subrange_tarfile: Optional path to a tarball containing subrange matrices. 
                Subrange matrices are GraphBLAS {0,1}-valued diagonal matrices. They are
                used to partition both the rows and columns into subranges, in all combinations 
                of the given subrange matrices. 
        benchmark_name: Optional string that can be set to what the row label should be named 
                in the stats file to ensure different names between runs. Default is the filename
        save_range: Optional boolean for whether to save the GraphBLAS matrices made from 
                applying the subranges onto the the tarred data given by the filename. They are saved 
                in a tarfile as {i}_{j}.grb, with i and j being the indices of the source and 
                destinination subranges, respectively. The naming is 1-indexed. The tarfile 
                gets its name from the filename, and is placed in the working directory. 
    """
    if benchmark_name is None:      
        benchmark_name = filename               
 
    # get the list of tar files from filelist - each tar is on a new line
    filelist = []
    with open(filename, "r") as f:
        for line in f:
            filelist.append(line.split("\n")[0])

    if subrange_tarfile is not None:
        # read in the tar, -> make list of matrices in subrange tar
        # and then at end+1 add empty matrix and sum together the subranged matrices
        AsrcRange = tar_matrix_processing.process_range_tarball(subrange_tarfile)
        AdestRange = AsrcRange

        num_subranges = len(AsrcRange)
        NsrcRange = num_subranges
        NdestRange = num_subranges
    
    start_time = time.time()                             # timing for the whole process       
    a_sum, update_time = update(filelist)                # sum matrices together
    results, analyze_time = analyze_matrix(a_sum)        # compute metrics
    
    if subrange_tarfile is not None:
        start_subrange_analysis_time = time.time()
        AsumSrc = tar_matrix_processing.create_empty_matrix()  # Initialize subrange sum matrix
        AsumSrcDest = tar_matrix_processing.create_empty_matrix()        # Initialize subrange src-dest sum matrix

        for i in range(NsrcRange): 
            # Note that << in this context is not a bit shift operator, but rather
            # to denote that the GraphBLAS object is being updated. 
            AsumSrc << AsrcRange[i].mxm(a_sum)                  # select sources
            if i == NsrcRange - 1:                              # if we're looking at the last matrix that contains the sum
                AsumSrc << gb.select.valuene(a_sum - AsumSrc, 0)        # get 'everything else'

            for j in range(NdestRange):
                AsumSrcDest << AsumSrc.mxm(AdestRange[j])       # select destinations

                if j == NdestRange - 1:                         # if we're at the last, sum, matrix
                    AsumSrcDest << gb.select.valuene(AsumSrc - AsumSrcDest, 0)      # get 'everything else'
                    
                if save_range:
                    # this filename is where the matrices of the AsumSrcDest will be saved to
                    tar_filename = os.path.join(filename.split(os.sep)[-1].split('.')[-2]+".tar")
                    print(f"{tar_filename=}")

                    grb_filename = str(i+1)+"_"+str(j+1)+".grb"         # make the grb file name from indices

                    AsumSrcDestBlob = AsumSrcDest.ss.serialize()           # serialize the matrix into the blob

                    with open(grb_filename, "wb") as g:
                        g.write(AsumSrcDestBlob)                        # write in the blob into the file

                    mode = 'a' if os.path.exists(tar_filename) else 'w'         # append to the tar if it already exists, otherwise make new

                    # Open tar file and add the file
                    with tarfile.open(tar_filename, mode=mode) as tar:
                        tar.add(grb_filename)
                    
                    os.remove(grb_filename)                         # remove the grb afterwards
                        
                # Analyze this subrange src-dest pair of i,j
                results_subrange, _ = analyze_matrix(AsumSrcDest)

                # Add results to stats table, keeping track of i,j in title
                for stat_name in [
                    "sub-" + name + "-" + str(i+1) + "_" + str(j+1) for name in STAT_NAMES
                ]:
                    stats[benchmark_name, stat_name] = results_subrange[stat_name.split("-")[1]]
        
        subrange_time = time.time() - start_subrange_analysis_time          # time it took for subrange analysis
        analyze_time = analyze_time + subrange_time                         # total analysis time

    stop_time = time.time()             # total time for full process
 
    print(results)

    print(
        f"{filename} analyzed in {analyze_time} seconds; "
        f"total packets processed = {results['n_packets']}; "
        f"Total rate of packets per second = {float(results['n_packets'])/(stop_time - start_time)}; "
        f"update time = {update_time} seconds; "
    )

    update_rate = float(results["n_packets"]) / update_time         # find the updating rate
    analyze_rate = float(results["n_packets"]) / analyze_time       # finding analyzing rate

    print(f"Update rate: {update_rate} packets/second")
    print(f"Analyze rate: {analyze_rate} packets/second")
      
    # Saving all stats into the Associative Array, writing results into csv
    for stat_name in STAT_NAMES:
            stats[benchmark_name, stat_name] = results[stat_name]
    stats[benchmark_name, "update_time"] = update_time
    stats[benchmark_name, "analyze_time"] = analyze_time
    stats[benchmark_name, "update_rate"] = update_rate
    stats[benchmark_name, "analyze_rate"] = analyze_rate

    ac.writecsv(stats, OUTPUT_STATS_PATH, delimiter="\t")
    print(f"Results saved to {OUTPUT_STATS_PATH}")

def update(filelist: List) -> Tuple[gb.Matrix, float]:
    """
    Sum together GraphBLAS matrices. 

    Args:
        filelist: List of tar files, each containing GraphBLAS matrices

    Return:
        GraphBLAS matrix of all matrices in all tars summed together, 
        Time it took to sum 
    """
    a_sum = tar_matrix_processing.create_empty_matrix()                    # Initialize sum matrix                
    start_update_time = time.time()                         # start update timer

    # for each tar file, sum matrices and add to our global sum
    for file_name in filelist:
        a_net_sum = tar_matrix_processing.process_tarball(file_name)     # extract and sum together the *.grb's in the tar file
        a_sum += a_net_sum                                  # Update the sum matrix
    update_time = time.time() - start_update_time

    return a_sum, update_time

def analyze_matrix(matrix: gb.Matrix) -> Tuple[Dict, float]:
    """
    Compute network analysis metrics

    Args:
        matrix: GraphBLAS matrix to analyze

    Returns:
        Dictionary of analysis results with timing
        Time taken to do analysis
    """
    start_time = time.time()

    # Maximum packets on a link
    max_packets = matrix.reduce_scalar(gb.monoid.max).value

    # Maximum packets from a source
    nonzero_rows = matrix.reduce_rowwise(gb.monoid.plus)
    max_source_packets = nonzero_rows.reduce(gb.monoid.max).value
    max_source_packets = 0 if max_source_packets is None else int(max_source_packets)

    # Maximum packets to a destination
    nonzero_columns = matrix.reduce_columnwise(gb.monoid.plus)
    max_destination_packets = nonzero_columns.reduce(gb.monoid.max).value
    max_destination_packets = 0 if max_destination_packets is None else int(max_destination_packets)

    # Maximum fan-out
    nonzero_rows = matrix.reduce_rowwise(gb.agg.count)
    max_fan_out = nonzero_rows.reduce(gb.monoid.max).value
    max_fan_out = 0 if max_fan_out is None else int(max_fan_out)

    # Maximum fan-in
    nonzero_columns = matrix.reduce_columnwise(gb.agg.count)
    max_fan_in = nonzero_columns.reduce(gb.monoid.max).value
    max_fan_in = 0 if max_fan_in is None else int(max_fan_in)

    # Total packets
    n_packets = matrix.reduce_scalar(gb.monoid.plus).value
    n_packets = 0 if n_packets is None else int(n_packets)

    # Unique links
    n_links = matrix.reduce_scalar(gb.agg.count).value
    n_links = 0 if n_links is None else int(n_links)

    # Number of unique sources
    n_sources = nonzero_rows.reduce(gb.agg.count).value
    n_sources = 0 if n_sources is None else int(n_sources)

    # Number of unique destinations
    n_destinations = nonzero_columns.reduce(gb.agg.count).value
    n_destinations = 0 if n_destinations is None else int(n_destinations)

    print(f"Max number of packets sent between a source and destination: {max_packets}")
    print(f"Max number of packets sent by a source: {max_source_packets}")
    print(f"Max number of packets received by a destination: {max_destination_packets}")
    print(f"Max source fan-out: {max_fan_out}")
    print(f"Max destination fan-in: {max_fan_in}")

    results = {
        "n_packets": str(n_packets),
        "n_links": str(n_links),
        "n_sources": str(n_sources),
        "n_destinations": str(n_destinations),
        "max_packets": str(max_packets),
        "max_source_packets": str(max_source_packets),
        "max_fan_out": str(max_fan_out),
        "max_destination_packets": str(max_destination_packets),
        "max_fan_in": str(max_fan_in),
    }

    analyze_time = time.time() - start_time

    return results, analyze_time


if __name__ == "__main__":
    try:
        process_filelist(sys.argv[1], subrange_tarfile=sys.argv[2], save_range=True)
    except IndexError as e:
        process_filelist(sys.argv[1])


    
