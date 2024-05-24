# Suricata utilities

json2grb - This program takes an input Suricata EVE 'flow' log in JSON format, provided as either a flat 
file or a UNIXdomain socket, and generates GraphBLAS network traffic matrices as output in a specified 
directory.

Optionally, the path to a CryptopANT anonymization key can be provided to perform prefix-preserving
IP address anonymization.  The final 16 bits of the network address are masked in this mode.  If the
file specified does not already exist, a random key will be generated and saved with that name.

In flat file processing mode, the program terminates on EOF; if provided the path to a UNIX domain socket,
processing is done until the remotely connected Suricata server closes the socket or SIGTERM is received.

    ./json2grb [-a anonymize.key] [-b[i][o]] <-i INPUT_FILE | -s unix-socket-path> -o OUTPUT_DIRECTORY

Example:

    ./json2grb -s /home/suricata/eve.sock -o ./outdir

To create a flat JSON EVE logging target in suricata.yaml:

    - eve-log:
        enabled: true
        filetype: regular
        filename: /path/to/flows.json
        types:
        - flow

To create a UNIX domain socket EVE logging target in suricata.yaml (recommended):

    - eve-log:
        enabled: yes
        filetype: unix_stream
        filename: /path/to/eve.sock
        types:
        - flow