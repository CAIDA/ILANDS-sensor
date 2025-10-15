import time
import pandas as pd
import dash
from dash import html, Output, Input, State, callback, dash_table
from helpers import *


@callback(
    Output('zone-matrix-container', 'children', allow_duplicate=True),
    Output('selected-metadata', 'data', allow_duplicate=True),
    Input('top-table', 'active_cell'),
    State('top-table', 'data'),
    prevent_initial_call=True
)
def push_main_table(active_cell, table_data):
    """
    Triggered when:
        A cell in the top-most table is selected
    
    Outputs:
        The necessary zone matrix or table of possible windows to select, 
        or, the appropriate drop downs if Ratio is selected
    
    """
    if active_cell is None:
        return dash.no_update, dash.no_update

    start_maintable = time.time()

    row_idx, col_idx = get_cell_rc(active_cell)

    metric = table_data[row_idx]['Metric']
    col_name = col_labels[col_idx - 1]  # skip "Metric" column

    # get the actual name if its there, otherise just use raw
    iColStr = get_iColStr(metric)
    check_value = False

    metadata = {'metric': metric, 'col_name': col_name}

    if iColStr == "ratio":
        # if bottom row is clicked, make appropriate dropdown
        if col_name == "Select Window":
            return html.Div([
            html.H4(f"Ratio Matrix Options for {col_name}"),
            html.Div([
                make_row_dropdown('ratio-numerator-window', "Select Numerator"),
                make_row_dropdown('ratio-denominator-window', "Select Denominator"), 
                make_window_dropdown('select-window-ratio-metric', "Select Window")
            ], style={
            "display": "flex",
            "flexDirection": "row",
            "justifyContent": "space-between",
            "gap": "10px"})
            ], 
    style={"marginTop": 30}), metadata

        return html.Div([
            html.H4(f"Ratio Matrix Options for {col_name}"),
            html.Div([
                make_row_dropdown('ratio-numerator-regular', "Select Numerator"),
                make_row_dropdown('ratio-denominator-regular', "Select Denominator")
            ])
            ], style={'marginTop': 30}), metadata
    else:
        # first, go through the dif columns and get Aselect for each - ie the stats we're checking out
        Aselect = get_Aselect_from_colname(col_name)
        if col_name == "Max Window":
            check_value = True
        elif col_name == "Select Window":
            # Produce table with a list of all the time windows in the global analysis table.
            # break out of here because it makes a different table than other things
            window_data = [{'Index': str(i), 'Time': str(v.replace(cleaning_string, ''))} for i, v in enumerate(statsrows)]
            table = dash_table.DataTable(
                id='Select Window',
            columns=[{"name": "Index", "id": "Index"}, {"name": "Time", "id": "Time"}],
            data=window_data, style_cell={'textAlign': 'center'}, page_size=20,
            cell_selectable=True,
            )
            print(f"pushing main select window list took {time.time()-start_maintable}")
            return html.Div([
                html.H4(f"Select Window for {metric} "),
                table
            ]), metadata

        elif col_name == "Ratio":            
            return html.Div([
                html.H4(f"Ratio Matrix Options for {metric}"),
                html.Div([
                    make_column_dropdown('ratio-numerator-col', "Select Numerator"), 
                    make_column_dropdown('ratio-denominator-col', "Select Denominator")
                ])
                ], style={'marginTop': 30}), metadata

        Avar = Aselect[:, [c for c in statscols if c.startswith(f"sub_{iColStr}_")]]
        AsumTable = row_to_zone_matrix(Avar, iColStr, Nzones, check_val=check_value)

        df_zone = pd.DataFrame(AsumTable, index=zone_names[::-1], columns=zone_names)

        table = make_zone_matrix('zone-matrix', df_zone)
        print(f"pushing {col_name} {metric} main zone matrix took {time.time() - start_maintable} seconds.")
        return html.Div([
            html.H4(f"Zone Matrix: {metric} x {col_name}"),
            table
        ]), metadata