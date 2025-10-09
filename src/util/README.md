# Miscellaneous GraphBLAS network matrix helper utilities

## csv2grb 
csv2grb - Incomplete, basic code to convert an input CSV file to GraphBLAS network traffic matrices.  

## gbdeserialize
gbdeserialize - Takes an input serialized GraphBLAS matrix file (.grb) and calls GxB_fprint() on it 
to describe its basic structure.  Used for sanity checking and validation.

    ./gbdeserialize /path/to/file.grb

## gbdump
gbdump - Dumps the rows and columns (source and destination IP address pairs) from a GraphBLAS matrix 
as dotted quad for inspection and validation.  Accepts either a .tar file containing only serialized matrices
or an individual .grb file.

    ./gbdump /path/to/file.tar

## iplist2grb
iplist2grb - – takes a list of newline-delimited lists of IP ranges in either CIDR notation (X.X.X.X/YY) 
or expressed as a contiguous range with a start and end (X.X.X.X:Y.Y.Y.Y) and turns it into a .tar of 
GraphBLAS matrices.  Used to create input for analysis scripts.

Optionally, the path to a CryptopANT anonymization key can be provided to perform prefix-preserving
IP address anonymization.  The final 16 bits of the network address are masked in this mode.  If the
file specified does not already exist, a random key will be generated and saved with that name.

    ./iplist2grb -n MySiteName rfc1918.txt bogons.txt external.txt webservers.txt

An input text file could be constructed as follows (rfc1918.txt):

    10.0.0.0/8
    172.16.0.0/12
    192.168.0.0/16

Or:

    10.0.0.0:10.255.255.255
    172.16.0.0:172.31.255.255
    192.168.0.0:192.168.255.255

Each provided input text file will appear as a diagonal GraphBLAS matrix in the output .tar file in the
order provided on the command line.

We have provided two files under the "IPlist" directory for convenience that can be used for this purpose.
They were generated using this utility as follows:

$ iplist2grb -n NonRoutableBogonIPList-LE rfc1918.txt bogons.txt
$ iplist2grb -S -n NonRoutableBogonIPList-BE rfc1918.txt bogons.txt

## makecache
makecache - Generates a precomputed IP address anonymization table.

