
# Instructions for PCAP data to visualization pipeline

 <!-- Steps to go from PCAP data to a statistics.tsv file and corresponding subrange matrices in tar files, to then using the jupyter notebook to visualize the data  -->

1. First, read the full README in https://github.com/CAIDA/ILANDS-sensor/tree/main. Follow the directions there to build the code from this directory onto your own machine. 
2. Starting with PCAP data, we will first convert this into GraphBLAS matrices for analysis.
	a. Collections of N<sub>V</sub>= 2<sup>17</sup> consecutive packets can each be anonymized with CryptoPAN directly or using a CryptoPAN generated anonymization table. The
resulting anonymized source and destination IPs are then used to construct a 2^32^x2^32^ hypersparse GraphBLAS matrix.  
	b. Under `src/pcap` is the `pcap2grb` program which is used to convert a standard .pcap file into GrB network traffic matrices. Typically, we’re operating on large pcap files that span an hour or more, and thus many GrB files are generated from a single pcap file, but they should all be of the correct size (2<sup>17</sup> x 64). 64 consecutive hypersparse GraphBLAS matrices are each serialized in compressed sparse rows (CSR) format with ZSTD compression and saved to a UNIX TAR file.
3. Obtain subrange matrices for subrange analysis. They are created from lists of CIDR network ranges with the `iplist2grb` utility under `src`.
5. Now, with .grb matrices and subrange matrices, you may now use the scripts under the `matlab`, `python_v1`, or `python_v2` directories to read, sum, and analyze the GraphBLAS matrices, depending on which language you are most comfortable with. Use the script that additionally does subrange analysis, as this is necessary for visualization. 
	a. Follow the directory-specific instructions to complete the analysis. You will likely need a text file that has your tarred GraphBLAS matrix data file names in a long list for processing.     
	b. The output should be a .tsv file that contains the network quantities obtained from the analysis. Additionally, there should be subrange matrices created for each time window, also collected in a .tar file. 
6. Finally, you can do the data visualization. Using the Jupyter UI, you will need the filename of the statistics .tsv file, the directory where the subrange matrices are held, and names for the subrange zones. After starting up the notebook, and verifying all the dependencies are installed, use the load_stats function with the file name of your statistics file, specify the directory in which the generated subrange matrices are stored, and set the name of the zones of each subrange. Not setting the correct amount of labels will result in an error. 



