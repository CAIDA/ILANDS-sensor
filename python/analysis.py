from analytics import *
import graphblas as gb
import numpy as np
import tarfile
import os
import glob
from typing import Union, Optional, Callable
import time
import math
import shutil


def get_matrix_from_grb(filename):
    with open(filename, "rb") as f:
        file_bytes = f.read()
    return gb.Matrix.ss.deserialize(file_bytes)


def process_tarball(tarball_filename: str,
                    n_rows: int = 2 ** 32,
                    n_cols: int = 2 ** 32,
                    return_grb_list: bool = False,
                    ignored_grbs: Optional[list[str]] = None
                    ) \
        -> Union[gb.Matrix, Tuple[gb.Matrix, list[str]]]:
    assert tarfile.is_tarfile(tarball_filename)
    directory = os.path.dirname(tarball_filename)
    if ignored_grbs is None:
        ignored_grbs = []
    with tarfile.open(name=tarball_filename, mode='r') as tarball:
        tarball_list = tarball.getmembers()
        processed_graphblas_matrix = gb.Matrix(nrows=n_rows, ncols=n_cols)
        grb_list = list()
        for file_info in tarball_list:
            next_graphblas_matrix_name = file_info.name
            if next_graphblas_matrix_name[-4:] == ".grb" and next_graphblas_matrix_name not in ignored_grbs:
                grb_list.append(next_graphblas_matrix_name)
                tarball.extract(next_graphblas_matrix_name, path=directory)
                next_graphblas_matrix = get_matrix_from_grb(directory + "/" + next_graphblas_matrix_name)
                processed_graphblas_matrix += next_graphblas_matrix
                try:
                    os.remove(directory + "/" + next_graphblas_matrix_name)
                except FileNotFoundError:
                    continue
            else:
                continue
    if return_grb_list:
        return processed_graphblas_matrix, grb_list
    else:
        return processed_graphblas_matrix


def _hierarchical_sum(file_list: list,
                      extraction_func: Callable,
                      grbs_per_level: int,
                      n_rows: int = 2 ** 32,
                      n_cols: int = 2 ** 32,
                      print_time_info: bool = False,
                      file_type: Optional[str] = None
                      ) -> gb.Matrix:
    if len(file_list) == 0:
        return gb.Matrix(dtype=int, nrows=2 ** 32, ncols=2 ** 32)

    # pre-allocate gb.Matrices to use while hierarchically summing
    if grbs_per_level == 1:
        max_stack_height = 2
    else:
        max_stack_height = math.ceil(math.log(len(file_list), grbs_per_level)) + 1
    gb_Matrix_list = list()
    for index in range(max_stack_height):
        gb_Matrix_list.append(gb.Matrix(dtype=int, nrows=n_rows, ncols=n_cols))

    # create stack to keep track of what levels stored gb.Matrices are at
    gb_Matrix_stack = list()

    # hierarchically sum up the list of grbs
    for index in range(len(file_list)):
        start_time = time.time()
        file = file_list[index]
        # extract the gb.Matrix and assign to appropriate place in gb_Matrix_list workspace
        current_stack_height = len(gb_Matrix_stack)  # stack height determines where newly extracted gb.Matrix goes
        gb_Matrix_list[current_stack_height] = extraction_func(file)
        gb_Matrix_stack.append(1)  # newly extracted gb.Matrix is at lowest level

        # sum until no consecutive gb.Matrices have same level
        while (current_stack_height > 0) and (
                gb_Matrix_stack[current_stack_height - 1] == gb_Matrix_stack[current_stack_height]):
            gb_Matrix_list[current_stack_height - 1] += gb_Matrix_list[current_stack_height]
            gb_Matrix_stack.pop()
            if grbs_per_level > 1:
                gb_Matrix_stack[current_stack_height - 1] += 1
            else:
                pass
            current_stack_height += -1

        if index == len(file_list) - 1:
            # if number of grbs wasn't a perfect power of grbs_per_level then some summing remains
            # alternate elevating level of final stack item and performing additions of gb.Matrices with same level
            current_stack_height = len(gb_Matrix_stack)
            while current_stack_height > 1:
                gb_Matrix_stack[current_stack_height - 2] += gb_Matrix_list[current_stack_height - 1]
                current_stack_height += -1

        end_time = time.time()

        if print_time_info:
            print(f"Processed {file} in {end_time - start_time} seconds."
                  f" {len(file_list) - index} remaining {file_type} files."
                  )
        else:
            continue

    processed_graphblas_matrix = gb_Matrix_list[0]  # first element of gb_Matrix_list is summand of all extracted grbs
    return processed_graphblas_matrix


def process_tarball_hierarchically(tarball_filename: str,
                                   n_rows: int = 2 ** 32,
                                   n_cols: int = 2 ** 32,
                                   grbs_per_level: int = 2,
                                   return_grb_list: bool = False,
                                   ignored_grbs: Optional[list[str]] = None
                                   ) -> Union[gb.Matrix, Tuple[gb.Matrix, list[str]]]:
    assert tarfile.is_tarfile(tarball_filename)
    subdirectory = tarball_filename + "-workspace"
    try:
        os.mkdir(subdirectory)
    except FileExistsError:
        shutil.rmtree(subdirectory)
        os.mkdir(subdirectory)
    if ignored_grbs is None:
        ignored_grbs = []

    with tarfile.open(name=tarball_filename, mode='r') as tarball:
        # generate the list of all grbs that are being added together and extract them
        tarball_list = tarball.getmembers()
        grb_list = list()
        for file_info in tarball_list:
            next_graphblas_matrix_name = file_info.name
            if next_graphblas_matrix_name[-4:] == ".grb" and next_graphblas_matrix_name not in ignored_grbs:
                grb_list.append(subdirectory + "/" + next_graphblas_matrix_name)
                tarball.extract(next_graphblas_matrix_name, path=subdirectory)
            else:
                continue

    processed_graphblas_matrix = _hierarchical_sum(grb_list,
                                                   get_matrix_from_grb,
                                                   grbs_per_level,
                                                   n_rows=n_rows,
                                                   n_cols=n_cols
                                                   )

    shutil.rmtree(subdirectory)

    if return_grb_list:
        return processed_graphblas_matrix, grb_list
    else:
        return processed_graphblas_matrix


def convert_ipv4_to_int(ipv4_address: str, endianness: str) -> int:
    big_endianness_proxies = ['big', 'big-endian', 'BE']
    little_endianness_proxies = ['little', 'little-endian', 'LE']
    big_endian, little_endian = False, False
    if endianness in big_endianness_proxies:
        big_endian = True
    elif endianness in little_endianness_proxies:
        pass
    else:
        raise ValueError(f'Unexpected endianness parameter; '
                         f'acceptable big-endian parameters are {big_endianness_proxies} while '
                         f'acceptable little-endian parameters are {little_endianness_proxies}'
                         )
    ipv4_split = ipv4_address.split('.')
    assert len(ipv4_split) == 4
    good_octet_str = True
    for octet_str in ipv4_split:
        try:
            octet_int = int(octet_str)
            good_octet_str = good_octet_str and (str(octet_int) == octet_str) and (0 <= octet_int <= 255)
        except TypeError:
            good_octet_str = False
            break
    if good_octet_str:
        pass
    else:
        raise ValueError(f'Given ipv4_address {ipv4_address} not a valid quad-dotted ipv4 address')
    if big_endian:
        ipv4_int = (int(ipv4_split[0]) * (256 ** 3)) \
                   + (int(ipv4_split[1]) * (256 ** 2)) \
                   + (int(ipv4_split[2]) * (256 ** 1)) \
                   + (int(ipv4_split[3]) * (256 ** 0))
    else:
        ipv4_int = (int(ipv4_split[0]) * (256 ** 0)) \
                   + (int(ipv4_split[1]) * (256 ** 1)) \
                   + (int(ipv4_split[2]) * (256 ** 2)) \
                   + (int(ipv4_split[3]) * (256 ** 3))
    return ipv4_int


def convert_ipv4s_to_ints(ipv4_addresses: Union[str, list[str], np.ndarray], endianness: str) -> np.ndarray:
    if isinstance(ipv4_addresses, str):
        ipv4_addresses = [ipv4_addresses]
    else:
        pass
    ipv4_ints = []
    for ipv4_address in ipv4_addresses:
        ipv4_int = convert_ipv4_to_int(ipv4_address, endianness)
        ipv4_ints.append(ipv4_int)
    ipv4_ints = np.array(ipv4_ints)
    return ipv4_ints


class TrafficMatrix:
    """
    Class subsuming a traffic matrix built up from tar'ed *.grb files.
    Attributes:
        matrix: gb.Matrix - whole traffic matrix
        source_ranges: list[gb.Matrix] - distinguished source IP ranges as diagonal matrices;
         assumed nonoverlapping
        destination_ranges: list[gb.Matrix] - distinguished destination IP ranges as diagonal matrices;
         assumed nonoverlapping
        submatrices: dict[tuple[slice, slice], gb.Matrix] - traffic submatrices determined by source_ranges
         and destination_ranges
        size: tuple[int, int] - the dimensions of the traffic matrix (and all traffic submatrices)
        processed_grbs: list[str] - list of filenames of *.grb files processed into the traffic matrix
        processed_tars: list[str] - list of filenames of *.tar files processed into the traffic matrix
    """

    def __init__(self,
                 matrix: Optional[gb.Matrix] = None,
                 source_ranges: Optional[list[gb.Matrix]] = None,
                 destination_ranges: Optional[list[gb.Matrix]] = None
                 ):
        if matrix is None:
            matrix = gb.Matrix(dtype=int, nrows=2 ** 32, ncols=2 ** 32)
        self.matrix = matrix

        if source_ranges is None:
            source_ranges = list()
        if destination_ranges is None:
            destination_ranges = list()
        self.source_ranges = source_ranges
        self.destination_ranges = destination_ranges

        # create dictionary of submatrices based on source_ranges and destination_ranges
        self.submatrices = dict()
        for source_range_index in range(len(source_ranges)):
            source_range = source_ranges[source_range_index]
            for destination_range_index in range(len(destination_ranges)):
                destination_range = destination_ranges[destination_range_index]
                submatrix = source_range.mxm(matrix).mxm(destination_range).new()
                self.submatrices[source_range_index, destination_range_index] = submatrix

        self.size = (matrix.nrows, matrix.ncols)
        self.processed_grbs = list()
        self.processed_tars = list()

    def n_packets(self):
        return valid_packets(self.matrix)

    def n_sources(self):
        return unique_sources(self.matrix)

    def n_destinations(self):
        return unique_destinations(self.matrix)

    def n_links(self):
        return unique_links(self.matrix)

    def sub_n_packets(self):
        n_source_ranges = len(self.source_ranges)
        n_destination_ranges = len(self.destination_ranges)
        sub_n_packets = np.empty(shape=(n_source_ranges + 1, n_destination_ranges + 1), dtype=int)
        for key in self.submatrices.keys():
            submatrix = self.submatrices[key]
            sub_n_packets[key] = valid_packets(submatrix)
        sub_n_packets[n_source_ranges, n_destination_ranges] \
            = self.n_packets() - np.sum(sub_n_packets[:n_source_ranges, :n_destination_ranges])
        return sub_n_packets

    def sub_n_sources(self):
        n_source_ranges = len(self.source_ranges)
        n_destination_ranges = len(self.destination_ranges)
        sub_n_sources = np.empty(shape=(n_source_ranges + 1, n_destination_ranges + 1), dtype=int)
        for key in self.submatrices.keys():
            submatrix = self.submatrices[key]
            sub_n_sources[key] = unique_sources(submatrix)
        sub_n_sources[n_source_ranges, n_destination_ranges] \
            = self.n_sources() - np.sum(sub_n_sources[:n_source_ranges, :n_destination_ranges])
        return sub_n_sources

    def sub_n_destinations(self):
        n_source_ranges = len(self.source_ranges)
        n_destination_ranges = len(self.destination_ranges)
        sub_n_destinations = np.empty(shape=(n_source_ranges + 1, n_destination_ranges + 1), dtype=int)
        for key in self.submatrices.keys():
            submatrix = self.submatrices[key]
            sub_n_destinations[key] = unique_destinations(submatrix)
        sub_n_destinations[n_source_ranges, n_destination_ranges] \
            = self.n_destinations() - np.sum(sub_n_destinations[:n_source_ranges, :n_destination_ranges])
        return sub_n_destinations

    def sub_n_links(self):
        n_source_ranges = len(self.source_ranges)
        n_destination_ranges = len(self.destination_ranges)
        sub_n_links = np.empty(shape=(n_source_ranges + 1, n_destination_ranges + 1), dtype=int)
        for key in self.submatrices.keys():
            submatrix = self.submatrices[key]
            sub_n_links[key] = unique_links(submatrix)
        sub_n_links[n_source_ranges, n_destination_ranges] \
            = self.n_links() - np.sum(sub_n_links[:n_source_ranges, :n_destination_ranges])
        return sub_n_links

    def update(self, filename: Union[str, list[str]],
               recursive: bool = False,
               return_time: bool = False
               ) -> Union[float, None]:
        update_start_time = time.time()
        if isinstance(filename, str):
            filenames = [filename]
        else:
            filenames = filename

        if recursive:
            regex_midfix = "/**/"
        else:
            regex_midfix = "/"

        file_list = set()

        for filename in filenames:
            if os.path.isdir(filename):
                tars = glob.glob("./" + filename + regex_midfix + "*.tar")
                grbs = glob.glob("./" + filename + regex_midfix + "*.grb")
                tars = [tar_file.replace("\\", "/") for tar_file in tars
                        if os.path.basename(tar_file) not in self.processed_tars]
                grbs = [grb_file.replace("\\", "/") for grb_file in grbs
                        if os.path.basename(grb_file) not in self.processed_grbs]
                file_list = file_list.union(set(tars))
                file_list = file_list.union(set(grbs))
            elif os.path.isfile(filename):
                file_ext = os.path.splitext(filename)[1]
                if file_ext in [".tar", ".grb"] \
                        and os.path.basename(filename) not in self.processed_tars \
                        and os.path.basename(filename) not in self.processed_grbs:
                    file_list.add(filename)
                else:
                    print(f"Invalid file: {filename} must either by a *.tar file or a *.grb file; passing...")
            else:
                print(f"Invalid file: {filename} must either by a *.tar file, *.grb file, or a directory; passing...")

        full_grb_list = [file for file in file_list if os.path.splitext(file)[1] == ".grb"]
        full_tar_list = [file for file in file_list if os.path.splitext(file)[1] == ".tar"]
        print(f"*.tar & *.grb files collected. Processing {len(full_grb_list)} *.grb files "
              f"and {len(full_tar_list)} *.tar files")

        grb_index = 0
        tar_index = 0
        for file in file_list:
            name, extension = os.path.splitext(file)
            if (extension == ".tar" and name in self.processed_tars) \
                    or (extension == ".grb" and name in self.processed_grbs):
                pass
            else:
                start_time = time.time()
                if extension == ".grb":
                    next_matrix = get_matrix_from_grb(file)
                    grb_list = [name + extension]
                    tar_list = []
                    num_grbs = 1
                    grb_index += 1
                elif extension == ".tar":
                    next_matrix, tar_grbs = process_tarball(file,
                                                            return_grb_list=True,
                                                            ignored_grbs=self.processed_grbs
                                                            )
                    grb_list = []
                    tar_list = [file]
                    num_grbs = len(tar_grbs)
                    tar_index += 1
                else:
                    continue

                self.matrix += next_matrix
                self.processed_grbs += grb_list
                self.processed_tars += tar_list

                end_time = time.time()
                s = "ces" if len(grb_list) > 1 else "x"
                print(f"Processed {file} ({num_grbs} GraphBLAS matri{s}) in {end_time - start_time} seconds."
                      f" {len(full_grb_list) - grb_index} remaining *.grb files"
                      f" & {len(full_tar_list) - tar_index} remaining *.tar files."
                      )

        for key in self.submatrices.keys():
            source_range_index, destination_range_index = key
            source_range = self.source_ranges[source_range_index]
            destination_range = self.destination_ranges[destination_range_index]
            submatrix = source_range.mxm(self.matrix).mxm(destination_range).new()
            self.submatrices[key] += submatrix

        update_stop_time = time.time()
        print(f"All files processed in {update_stop_time - update_start_time} seconds!")

        if return_time:
            return update_stop_time - update_start_time
        else:
            return None

    def update_hierarchically(self,
                              filename: Union[str, list[str]],
                              grbs_per_level: int = 2,
                              recursive: bool = False,
                              return_time: bool = False
                              ) -> Union[float, None]:
        update_start_time = time.time()
        if isinstance(filename, str):
            filenames = [filename]
        else:
            filenames = filename

        if recursive:
            regex_midfix = "/**/"
        else:
            regex_midfix = "/"

        file_list = set()

        for filename in filenames:
            if os.path.isdir(filename):
                tars = glob.glob("./" + filename + regex_midfix + "*.tar")
                grbs = glob.glob("./" + filename + regex_midfix + "*.grb")
                tars = [tar_file.replace("\\", "/") for tar_file in tars
                        if os.path.basename(tar_file) not in self.processed_tars]
                grbs = [grb_file.replace("\\", "/") for grb_file in grbs
                        if os.path.basename(grb_file) not in self.processed_grbs]
                file_list = file_list.union(set(tars))
                file_list = file_list.union(set(grbs))
            elif os.path.isfile(filename):
                file_ext = os.path.splitext(filename)[1]
                if file_ext in [".tar", ".grb"] \
                        and os.path.basename(filename) not in self.processed_tars \
                        and os.path.basename(filename) not in self.processed_grbs:
                    file_list.add(filename)
                else:
                    print(f"Invalid file: {filename} must either by a *.tar file or a *.grb file; passing...")
            else:
                print(f"Invalid file: {filename} must either by a *.tar file, *.grb file, or a directory; passing...")

        full_grb_list = [file for file in file_list if os.path.splitext(file)[1] == ".grb"]
        full_tar_list = [file for file in file_list if os.path.splitext(file)[1] == ".tar"]
        print(f"*.tar & *.grb files collected. Processing {len(full_grb_list)} *.grb files "
              f"and {len(full_tar_list)} *.tar files")

        grb_list_matrix = _hierarchical_sum(full_grb_list,
                                            get_matrix_from_grb,
                                            grbs_per_level,
                                            print_time_info=True,
                                            file_type="grb"
                                            )
        tar_list_matrix = _hierarchical_sum(full_tar_list,
                                            lambda x: process_tarball_hierarchically(x, grbs_per_level=grbs_per_level),
                                            grbs_per_level,
                                            print_time_info=True,
                                            file_type="tar"
                                            )

        self.matrix = grb_list_matrix + tar_list_matrix
        self.processed_grbs += full_grb_list
        self.processed_tars += full_tar_list
        for key in self.submatrices.keys():
            source_range_index, destination_range_index = key
            source_range = self.source_ranges[source_range_index]
            destination_range = self.destination_ranges[destination_range_index]
            next_submatrix = source_range.mxm(self.matrix).mxm(destination_range).new()
            self.submatrices[key] += next_submatrix

        update_stop_time = time.time()
        print(f"All files processed in {update_stop_time - update_start_time} seconds!")

        if return_time:
            return update_stop_time - update_start_time
        else:
            return None

    def clear(self):
        """Remove all values from matrix and all submatrices. Row/column ranges and size are unchanged."""
        self.matrix.clear()
        for key in self.submatrices.keys():
            self.submatrices[key].clear()
        self.processed_tars = list()
        self.processed_grbs = list()
        return None

    def analyze(self,
                count_multiplicities: bool = False,
                return_time: bool = False
                ):
        start_time = time.time()

        n_source_ranges = len(self.source_ranges)
        n_destination_ranges = len(self.destination_ranges)

        sub_max_packets = np.empty(shape=(n_source_ranges + 1, n_destination_ranges + 1), dtype=int)
        sub_max_packets_links = np.empty(shape=(n_source_ranges + 1, n_destination_ranges + 1))
        sub_max_source_packets = np.empty(shape=(n_source_ranges + 1, n_destination_ranges + 1), dtype=int)
        sub_max_source_packets_sources = np.empty(shape=(n_source_ranges + 1, n_destination_ranges + 1))
        sub_max_destination_packets = np.empty(shape=(n_source_ranges + 1, n_destination_ranges + 1), dtype=int)
        sub_max_destination_packets_destinations \
            = np.empty(shape=(n_source_ranges + 1, n_destination_ranges + 1))
        sub_max_fan_out = np.empty(shape=(n_source_ranges + 1, n_destination_ranges + 1), dtype=int)
        sub_max_fan_out_sources = np.empty(shape=(n_source_ranges + 1, n_destination_ranges + 1))
        sub_max_fan_in = np.empty(shape=(n_source_ranges + 1, n_destination_ranges + 1), dtype=int)
        sub_max_fan_in_destinations = np.empty(shape=(n_source_ranges + 1, n_destination_ranges + 1))

        # calculate network quantities for whole traffic matrix

        max_packets_links = None
        max_source_packets_sources = None
        max_destination_packets_destinations = None
        max_fan_out_sources = None
        max_fan_in_destinations = None

        if count_multiplicities:
            _, max_packets, max_packets_links \
                = valid_packets(self.matrix,
                                return_max_packets=True,
                                return_max_IP_pairs=True
                                )
            max_source_packets, max_source_packets_sources \
                = max_packets_by_sources(self.matrix,
                                         return_max_sources=True
                                         )
            max_destination_packets, max_destination_packets_destinations \
                = max_packets_by_destinations(self.matrix,
                                              return_max_destinations=True
                                              )
            _, max_fan_out, max_fan_out_sources \
                = unique_sources(self.matrix,
                                 return_max_fan_out=True,
                                 return_max_sources=True
                                 )
            _, max_fan_in, max_fan_in_destinations \
                = unique_destinations(self.matrix,
                                      return_max_fan_in=True,
                                      return_max_destinations=True
                                      )
        else:
            _, max_packets = valid_packets(self.matrix, return_max_packets=True)
            max_source_packets = max_packets_by_sources(self.matrix)
            max_destination_packets = max_packets_by_destinations(self.matrix)
            _, max_fan_out = unique_sources(self.matrix, return_max_fan_out=True)
            _, max_fan_in = unique_destinations(self.matrix, return_max_fan_in=True)

        for key in self.submatrices.keys():
            submatrix = self.submatrices[key]
            if count_multiplicities:
                _, max_packets_sub, max_packets_links_sub \
                    = valid_packets(submatrix, return_max_packets=True, return_max_IP_pairs=True)
                max_source_packets_sub, max_source_packets_sources_sub \
                    = max_packets_by_sources(submatrix, return_max_sources=True)
                max_destination_packets_sub, max_destination_packets_destinations_sub \
                    = max_packets_by_destinations(submatrix, return_max_destinations=True)
                _, max_fan_out_sub, max_fan_out_sources_sub \
                    = unique_sources(submatrix, return_max_fan_out=True, return_max_sources=True)
                _, max_fan_in_sub, max_fan_in_destinations_sub \
                    = unique_destinations(submatrix, return_max_fan_in=True, return_max_destinations=True)

                sub_max_packets_links[key] = max_packets_links_sub
                sub_max_source_packets_sources[key] = max_source_packets_sources_sub
                sub_max_destination_packets_destinations[key] = max_destination_packets_destinations_sub
                sub_max_fan_out_sources[key] = max_fan_out_sources_sub
                sub_max_fan_in_destinations[key] = max_fan_in_destinations_sub
            else:
                _, max_packets_sub = valid_packets(submatrix, return_max_packets=True)
                max_source_packets_sub = max_source_packets(submatrix)
                max_destination_packets_sub = max_destination_packets(submatrix)
                _, max_fan_out_sub = unique_sources(submatrix, return_max_fan_out=True)
                _, max_fan_in_sub = unique_destinations(submatrix, return_max_fan_in=True)

            sub_max_packets[key] = max_packets_sub
            sub_max_source_packets[key] = max_source_packets_sub
            sub_max_destination_packets[key] = max_destination_packets_sub
            sub_max_fan_out[key] = max_fan_out_sub
            sub_max_fan_in[key] = max_fan_in_sub

        results = dict()

        if count_multiplicities:
            sub_max_packets_links[n_source_ranges, n_destination_ranges] \
                = max_packets_links - np.sum(sub_max_packets_links[:n_source_ranges, :n_destination_ranges])
            sub_max_source_packets_sources[n_source_ranges, n_destination_ranges] \
                = (max_source_packets_sources
                   - np.sum(sub_max_source_packets_sources[:n_source_ranges, :n_destination_ranges]))
            sub_max_destination_packets_destinations[n_source_ranges, n_destination_ranges] \
                = (max_destination_packets_destinations
                   - np.sum(sub_max_destination_packets_destinations[:n_source_ranges, :n_destination_ranges]))
            sub_max_fan_out_sources[n_source_ranges, n_destination_ranges] \
                = max_fan_out_sources - np.sum(sub_max_fan_out_sources[:n_source_ranges, :n_destination_ranges])
            sub_max_fan_in_destinations[n_source_ranges, n_destination_ranges] \
                = (max_fan_in_destinations
                   - np.sum(sub_max_fan_in_destinations[:n_source_ranges, :n_destination_ranges]))

            results["max_packets_links"] = max_packets_links
            results["max_source_packets_sources"] = max_source_packets_sources
            results["max_destination_packets_destinations"] = max_destination_packets_destinations
            results["max_fan_out_sources"] = max_fan_out_sources
            results["max_fan_in_destinations"] = max_fan_in_destinations
            results["sub_max_packets_links"] = sub_max_packets_links
            results["sub_max_source_packets_sources"] = sub_max_source_packets_sources
            results["sub_max_destination_packets_destinations"] = sub_max_destination_packets_destinations
            results["sub_max_fan_out_sources"] = sub_max_fan_out_sources
            results["sub_max_fan_in_destinations"] = sub_max_fan_in_destinations

        sub_max_packets[n_source_ranges, n_destination_ranges] \
            = max_packets - np.sum(sub_max_packets[:n_source_ranges, :n_destination_ranges])
        sub_max_source_packets[n_source_ranges, n_destination_ranges] \
            = max_source_packets - np.sum(sub_max_source_packets[:n_source_ranges, :n_destination_ranges])
        sub_max_destination_packets[n_source_ranges, n_destination_ranges] \
            = max_destination_packets - np.sum(sub_max_destination_packets[:n_source_ranges, :n_destination_ranges])
        sub_max_fan_out[n_source_ranges, n_destination_ranges] \
            = max_fan_out - np.sum(sub_max_fan_out[:n_source_ranges, :n_destination_ranges])
        sub_max_fan_in[n_source_ranges, n_destination_ranges] \
            = max_fan_in - np.sum(sub_max_fan_in[:n_source_ranges, :n_destination_ranges])

        results["max_packets"] = max_packets
        results["max_source_packets"] = max_source_packets
        results["max_destination_packets"] = max_destination_packets
        results["max_fan_out"] = max_fan_out
        results["max_fan_in"] = max_fan_in
        results["sub_max_packets"] = sub_max_packets
        results["sub_max_source_packets"] = sub_max_source_packets
        results["sub_max_destination_packets"] = sub_max_destination_packets
        results["sub_max_fan_out"] = sub_max_fan_out
        results["sub_max_fan_in"] = sub_max_fan_in

        print(f"Maximum number of packets sent between a source and destination: {max_packets}")
        print(f"Maximum number of packets sent by a source: {max_source_packets}")
        print(f"Maximum number of packets received by a destination: {max_destination_packets}")
        print(f"Maximum source fan-out: {max_fan_out}")
        print(f"Maximum destination fan-in: {max_fan_in}")

        if count_multiplicities:
            print(f"Number of source-destination pairs with max number of packets sent between: {max_packets_links}")
            print(f"Number of sources sending max number of packets: {max_source_packets_sources}")
            print(f"Number of destinations receiving max number of packets: {max_destination_packets_destinations}")
            print(f"Number of sources with max source fan-out: {max_fan_out_sources}")
            print(f"Number of destinations with max destination fan-in: {max_fan_in_destinations}")

        if len(self.source_ranges) > 0 and len(self.destination_ranges) > 0:
            print(f"Array of maximum numbers of packets sent between a source and destination "
                  f"for each traffic submatrix: {sub_max_packets}")
            print(f"Array of maximum numbers of packets sent by a source "
                  f"for each traffic submatrix: {sub_max_source_packets}")
            print(f"Array of maximum numbers of packets received by a destination "
                  f"for each traffic submatrix: {sub_max_destination_packets}")
            print(f"Array of maximum source fan-outs "
                  f"for each traffic submatrix: {sub_max_fan_out}")
            print(f"Array of maximum destination fan-ins "
                  f"for each traffic submatrix: {sub_max_fan_in}")

            if count_multiplicities:
                print(f"Array of numbers of source-destination pairs with max number of packets sent between "
                      f"for each traffic submatrix: {sub_max_packets_links}")
                print(f"Array of numbers of sources sending max number of packets "
                      f"for each traffic submatrix: {sub_max_source_packets_sources}")
                print(f"Array of numbers of destinations receiving max number of packets "
                      f"for each traffic submatrix: {sub_max_destination_packets_destinations}")
                print(f"Array of numbers of sources with max source fan-out "
                      f"for each traffic submatrix: {sub_max_fan_out_sources}")
                print(f"Array of numbers of destinations with max destination fan-in "
                      f"for each traffic submatrix: {sub_max_fan_in_destinations}")
        else:
            pass

        results["n_links"] = self.n_links()
        results["n_sources"] = self.n_sources()
        results["n_destinations"] = self.n_destinations()
        results["n_packets"] = self.n_packets()
        results["sub_n_links"] = self.sub_n_links()
        results["sub_n_sources"] = self.sub_n_sources()
        results["sub_n_destinations"] = self.sub_n_destinations()
        results["sub_n_packets"] = self.sub_n_packets()
        stop_time = time.time()

        if return_time:
            return results, stop_time - start_time
        else:
            return results
