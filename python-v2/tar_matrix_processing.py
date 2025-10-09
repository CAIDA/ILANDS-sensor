import tarfile
import os
import graphblas as gb
from typing import List

def get_matrix_from_grb(filename: str) -> gb.Matrix:
    """Extract graphblas matrix from .grb file"""
    with open(filename, "rb") as f:
        file_bytes = f.read()                           # Read in binary stream
    return gb.Matrix.ss.deserialize(file_bytes)         # deserialize the grb blob

def create_empty_matrix() -> gb.Matrix:
    """Create an empty GraphBLAS matrix."""
    return gb.Matrix(dtype=int, nrows=2**32, ncols=2**32)

def sum_grbs(grb_list: List) -> gb.Matrix:
    """
    Sum matrices together. 
        Inputs: 
            grb_list - List containing GraphBLAS matrices in each entry
        Outputs:
            Anetsum -  a GraphBLAS matrix of all the matrices in the list summed together. 
                        outputs an empty matrix if the list is empty 
    """
    Anetsum = create_empty_matrix()      # create empty matrix to hold the summation of the files
    for matrix in grb_list:
        Anetsum += matrix
    return Anetsum

def readTar(tarball_filename: str) -> List:
    """
    Extract tarball of .grb files and make list of matrices
        Inputs:
            tarball_filename - relative or absolute path to *.tar file
        Outputs:
            AsrsRange - list of GraphBLAS matrices extracted from [tarball_filename]
    """
    assert tarfile.is_tarfile(tarball_filename)                                 # Make sure we actually have a tar file

    directory = os.path.dirname(tarball_filename)
    directory = "." if directory == "" else directory                           # get directory where extracted files will be placed

    with tarfile.open(name=tarball_filename, mode="r") as tarball:
        tarball_list = tarball.getmembers()                                     # make list of what is inside the tar file
        AsrsRange = []
        for file_info in tarball_list:                                          # for each member in the tar archive
            next_graphblas_matrix_name = file_info.name                         # get the file name
            if next_graphblas_matrix_name[-4:] == ".grb":                       # make sure it is a .grb file
                tarball.extract(next_graphblas_matrix_name, path=directory)     # extract the grb file into the tar directory
                next_graphblas_matrix = get_matrix_from_grb(
                    directory + "/" + next_graphblas_matrix_name                # deserialize the matrix
                )
                AsrsRange.append(next_graphblas_matrix)                         # Add this matrix onto the end of our list                
                try:                                                            # deleting .grb file made from the extraction
                    os.remove(directory + "/" + next_graphblas_matrix_name)
                except FileNotFoundError:
                    continue
            else:                                                               # skip over non-.grb files
                continue
    return AsrsRange

def process_tarball(tarball_filename: str) -> gb.Matrix:
    """Extract tarball of .grb files and return a grb matrix of the sum of all files"""
    grb_matrices = readTar(tarball_filename)
    return sum_grbs(grb_matrices)

def process_range_tarball(tarball_filename: str) ->  List:
    """
    Extract tarball of .grb files and return a list of GraphBLAS matrices, with the
    end of list containing the sum of all the matrices.
        Inputs:
            tarball_filename - relative or absolute path to *.tar file
        Outputs:
            AsrsRange - list of GraphBLAS matrices extracted from the tarball
    
    Note: 
        * The extracted python-graphblas matrices from [tarball-filenname] are assumed
            to be diagonal matrices intended for use as source and/or destination ranges 
            for range-level analysis of a matrix. 
    """
    AsrsRange = readTar(tarball_filename)
    AsrsRange.append(sum_grbs(AsrsRange))
    return AsrsRange