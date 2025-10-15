import time
import dash
from dash import Output, Input, State, callback, html, dcc
import plotly.express as px
import pandas as pd
from helpers import *


@callback(
        Output('plot-container', 'children'),
        Input('zone-matrix-window', 'active_cell'),
        State('selected-metadata', 'data'),
        State('zone-matrix-window', 'data'),
        prevent_initial_call=True
)
def select_row_plot(active_cell, metadata, zone_matrix_data):
    """
    Triggered when:
        Zone matrix cell of a specific window is clicked
    
    Outputs:
        A spy plot
    
    """
    if active_cell is None or not metadata:
        return dash.no_update

    start_showspy = time.time()

    row_idx, col_idx = get_cell_rc(active_cell)
    # adjust for flipped matrix
    row_idx, col_idx = get_adjusted_cell_rc(active_cell)
    row_zone, col_zone = get_zone_lables(row_idx, col_idx)
    statsrow_idx = metadata.get("curr_row_index")
    row_name = statsrows[statsrow_idx]
    metric = metadata.get('metric')
    iColStr = get_iColStr(metric)

    col_id = active_cell['column_id']
    zone_cell_value = zone_matrix_data[active_cell['row']][col_id]

    grb_filename = get_grb_filename(row_idx, col_idx)

    try: 
        fig_spy_matplotlib = get_spy_fig_img(grb_filename, row_name, iColStr, zone_cell_value)
        print(f"Took {time.time() - start_showspy} seconds to display spy plot.")
        return html.Div([
        html.H4(f"Sparse viewer: {row_zone} → {col_zone} for {metric}, window: {row_name}"),
        fig_spy_matplotlib
        ])
    except Exception as e:
        print(f"Making spy plot failed! This is why: {e}")



@callback(
    Output('plot-container', 'children', allow_duplicate=True),
    Output('selected-metadata', 'data'),
    Input('zone-matrix', 'active_cell'),
    State('selected-metadata', 'data'),
    State('zone-matrix', 'data'),
    prevent_initial_call=True
)
def show_plot(active_cell, metadata, zone_matrix_data):
    """
    Triggered when:
        Zone Matrix cell of main stats (current, median, max value/window) window is clicked.
    
    Outputs:
        Either a spy plot (for current), a time series plot (for median, max value), 
        or a zone matrix of the particular window in the cell of the max window zone matrix
    
    """
    if active_cell is None or not metadata:
        return dash.no_update, dash.no_update

    show_plot_time = time.time()

    row_idx, col_idx = get_adjusted_cell_rc(active_cell)
    col_id = active_cell['column_id']
    row_zone, col_zone = get_zone_lables(row_idx, col_idx)
    metric = metadata.get('metric')
    column = metadata.get('col_name')

    zone_cell_value = zone_matrix_data[active_cell['row']][col_id]

    iColStr = get_iColStr(metric)

    # again, have different kind of plot for the different metrics/ clicked things
    if column == "Current":
        grb_filename = get_grb_filename(row_idx, col_idx)
        try: 
            fig_spy_matplotlib = get_spy_fig_img(grb_filename, statsrows[NtimeWindow], iColStr, zone_cell_value)
            print(f"Time to show spy plot - current: {time.time()-show_plot_time}")
            return html.Div([
            html.H4(f"Sparse viewer: {row_zone} → {col_zone} for {metric}"),
            fig_spy_matplotlib
            ]), metadata
        except Exception as e:
            print(f"Making spy plot failed! This is why: {e}")

    elif column in ["Median", "Max Value"]:
        # here is where we have the time series plot
        sub_col = f"sub_{iColStr}_{row_idx + 1}_{col_idx + 1}"
        subcol_idx = np.where(statscols == sub_col)
        series = []
        Avar = stats[:, subcol_idx]
        series = Avar.adj.data
        Avar_rows = [s.replace(cleaning_string, "") for s in Avar.row]
        fig = px.line(x=Avar_rows, y=series, labels={'x': 'Time Window', 'y': metric},
                      title=f"Time Series for {metric} ({row_zone} → {col_zone})", log_y=True)
        print(f"Time to make time series {time.time()-show_plot_time}")
        return html.Div([
            html.H4(f"Time Series Plot: {row_zone} → {col_zone} for {metric}"),
            dcc.Graph(figure=fig.update_layout(
                    xaxis=dict(tickangle=-45),
                    autosize=True,
                    margin=dict(l=40, r=40, t=40, b=40)
                    ),
                    style={
                'height': '80vh',
                'width': '100%'
                 },
                config={'responsive': True})
            ]), metadata
    elif column == "Max Window":
        col_id = active_cell['column_id']
        statsrow_name = zone_cell_value
        statsrow_idx = np.where(statsrows == statsrow_name+cleaning_string)
        Avar = stats[statsrow_idx, list_colindices(iColStr)]
        # make zone matrix out of the selected window! 
        AsumTable = row_to_zone_matrix(Avar, iColStr, Nzones)
        df_zone = pd.DataFrame(AsumTable, index=zone_names[::-1], columns=zone_names)
        
        table = make_zone_matrix('zone-matrix-window-2', df_zone)
        metadata['curr_row_idx'] = statsrow_idx

        print(f"Time to show max window zone matrix {time.time() - show_plot_time}")
        return html.Div([
            html.H4(f"Zone Matrix: {metric} (Window {statsrow_name})"),
            table
        ]), metadata
    elif column == "Select Window":
        # It should not get to this point. 
        raise Exception 

@callback(
    Output('max-window-plot', 'children'),
    Input('zone-matrix-window-2', 'active_cell'),
    State('selected-metadata', 'data'),
    State('zone-matrix-window-2', 'data'),
    prevent_initial_call=True
)
def show_maxwindow_plot(active_cell, metadata, zone_matrix_data):
    """
    Triggered when:
        Specific max window zone matrix is clicked (the second layer)
    
    Outputs:
        A spy plot for that window
    
    """
    if active_cell is None or not metadata:
        return dash.no_update
    
    start_maxwindow = time.time()

    row_idx, col_idx = get_adjusted_cell_rc(active_cell)
    row_zone, col_zone = get_zone_lables(row_idx, col_idx)
    metric = metadata.get('metric')
    iColStr = get_iColStr(metric)
    grb_filename = str(row_idx+1) + "_" + str(col_idx+1) + ".grb"
    
    col_id = active_cell['column_id']
    zone_cell_value = zone_matrix_data[active_cell['row']][col_id]

    row_name = statsrows[metadata.get('curr_row_idx')]

    try: 
        fig_spy_matplotlib = get_spy_fig_img(grb_filename, row_name, iColStr, zone_cell_value)
        print(f"Time it took to show maxwindow spy plot: {time.time()-start_maxwindow}")
        return html.Div([
        html.H4(f"Sparse viewer: {row_zone} → {col_zone} for {metric}"),
        fig_spy_matplotlib
        ])
    except Exception as e:
        print(f"Making spy plot failed! This is why: {e}")