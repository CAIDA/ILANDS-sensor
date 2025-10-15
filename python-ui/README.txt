This is an implementation of the CyPheR UI in Python using Dash and Plotly. 

*************
Setting up:
*************

Prerequisites:
    Dash
    dash_bootstrap_components
    Plotly
    python-graphblas (and dependencies)
    D4M for Python (https://github.com/Accla/D4M.py)


To run, you will need the following directory structure:
    Python-UI
        |-- callbacks
            +-- push_main_table.py
            +-- push_plots.py
            +-- push_ration_tables.py
            +-- reset_buton.py
            +-- show_window_matrix.py
        |-- helpers.py
        |-- python-ui.py

In this directory, you will also need:
    an input file that contains the statistics computed from the Anonymized Network 
    Sensing GraphChallenge. 
        The default is "GraphChallenge_analyses_subrange_m_23_concat.tsv" (file not included); 
	this default can be changed in Line 15 of helpers.py.
    subrange tar files for each time window, which can also be computed using the reference 
    implementation for the Anonymized Network Sensing GraphChallenge.
        If the subrange tarfile cannot be found, it defaults to "20240507-152247.tar" (file not 
	included); this default can be changed in Line 278 of helpers.py.
        Note that the names of GraphBLAS matrices in the subrange tar file must be of the form 
	i_j.grb where i and j are the 1-indexed row and column of which subrange it contains 
	information of. 

******************
Running the code:
******************

With all the set up done, the dashboard visualization will run locally in your browser. 
In your terminal, first make sure that you are in the Python-UI directory. Then, type
    python python-ui.py

In your browser, go to 
    http://127.0.0.1:8050/
There, you will find the UI. 
