import time
import dash 
from dash import Output, Input, State, callback, html
import pandas as pd
from helpers import *


@callback(
        Output('select-window-zones', 'children', allow_duplicate=True),
        Output('selected-metadata', 'data', allow_duplicate=True),
        Input('Select Window', 'active_cell'),
        State('selected-metadata', 'data'),
        prevent_initial_call=True
)
def show_window_matrix(active_cell, metadata):
    """
    Triggered when:
        User selects window in long table of possible windows (from Select Window Column)

    Outputs:
        A zone matrix of the selected window for a particular metric (row)        
    
    """
    if active_cell is None or not metadata:
        return dash.no_update, dash.no_update
    
    start_showwindows = time.time()

    row_idx, _ = get_cell_rc(active_cell)
    row_name = statsrows[row_idx].replace(cleaning_string, '') 
    metric = metadata['metric']
    iColStr = get_iColStr(metric)

    # Extract the selected row
    Avar = stats[row_idx, list_colindices(iColStr)]
    # Avar = Aselect[:, ]
    AsumTable = row_to_zone_matrix(Avar, iColStr, Nzones)
    df_zone = pd.DataFrame(AsumTable, index=zone_names[::-1], columns=zone_names)

    table = make_zone_matrix('zone-matrix-window', df_zone)
    metadata['curr_row_index'] = row_idx

    print(f"Took {time.time()-start_showwindows} seconds to show the list of windows.")
    return html.Div([
        html.H4(f"Zone Matrix: {metric} (Window {row_name})"),
        table
    ]), metadata