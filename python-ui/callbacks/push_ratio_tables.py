import pandas as pd
import dash
from dash import html, Output, Input, State, callback
from helpers import *


@callback(
    Output('ratio-zone-matrix', 'children', allow_duplicate=True),
    Input('ratio-numerator-regular', 'value'),
    Input('ratio-denominator-regular', 'value'),
    State('selected-metadata', 'data'),
    prevent_initial_call=True
)
def show_ratio_matrix_regularnum_denom(numerator_metric, denominator_metric, metadata):
    """
    Triggered when:
        The dropdowns are selected after picking Ratio x ( Current, Median, or Max Value)
    
    Outputs:
        The ratio matrix of the numerator and denominator metrics
    
    """
    if not numerator_metric or not denominator_metric or not metadata:
        return dash.no_update

    col_name = metadata.get('col_name')    

    num_col = get_iColStr(numerator_metric)
    denom_col = get_iColStr(denominator_metric)

    Aselect = get_Aselect_from_colname(col_name)

    Avar_num = Aselect[:, list_colindices(num_col)]
    Avar_denom = Aselect[:, list_colindices(denom_col)]

    Anum = row_to_zone_matrix(Avar_num, num_col, Nzones)
    Adenom = row_to_zone_matrix(Avar_denom, denom_col, Nzones)

    # Perform element-wise division, avoid divide-by-zero
    ratio_matrix = safe_ratio_matrix(Anum, Adenom)
    df_ratio = pd.DataFrame(ratio_matrix, index=zone_names[::-1], columns=zone_names)

    table = make_zone_matrix('zone-matrix-ratio', df_ratio)

    return html.Div([
        html.H4(f"Zone Matrix Ratio: {numerator_metric} / {denominator_metric}"),
        table
    ])

@callback(
    Output('ratio-zone-matrix', 'children', allow_duplicate=True),
    Input('ratio-numerator-window', 'value'),
    Input('ratio-denominator-window', 'value'),
    Input('select-window-ratio-metric', 'value'),
    State('selected-metadata', 'data'),
    prevent_initial_call=True
)
def show_ratio_matrix_window_column(numerator_metric, denominator_metric, selected_window, metadata):
    """
    Triggered when:
        dropdowns are selected for the row ratio col select window case

    Outputs:
        gets ratio between two stats for the selected window 
    """
    if not numerator_metric or not denominator_metric or not selected_window or not metadata:
        return dash.no_update

    num_col = get_iColStr(numerator_metric)
    denom_col = get_iColStr(denominator_metric)

    # Extract the selected row
    Aselect = stats[np.where(statsrows==selected_window), :]

    Avar_num = Aselect[:, list_colindices(num_col)]
    Avar_denom = Aselect[:, list_colindices(denom_col)]

    Anum = row_to_zone_matrix(Avar_num, num_col, Nzones)
    Adenom = row_to_zone_matrix(Avar_denom, denom_col, Nzones)

    # Perform element-wise division, avoid divide-by-zero
    ratio_matrix = safe_ratio_matrix(Anum, Adenom)
    df_ratio = pd.DataFrame(ratio_matrix, index=zone_names[::-1], columns=zone_names)

    table = make_zone_matrix('slected_window_ratiomatrix', df_ratio)

    return html.Div([
        html.H4(f"Ratio Zone Matrix: {numerator_metric}/{denominator_metric} (Window {selected_window})"),
        table
    ])


@callback(
    Output('ratio-zone-matrix', 'children', allow_duplicate=True),
    Output('select-window-zones', 'children', allow_duplicate=True),
    Input('ratio-numerator-col', 'value'),
    Input('ratio-denominator-col', 'value'),
    State('selected-metadata', 'data'),
    prevent_initial_call=True
)
def show_ratio_matrix(numerator_metric, denominator_metric, metadata):
    """
    Triggered when:
        Dropdowns are selected from the Ratio column.

    Outputs:
        Either makes another layer of dropdowns so that the user can select windows, or
        makes the zone matrix of the ratio of the selected dropdown values
    """
    if not numerator_metric or not denominator_metric or not metadata:
        return dash.no_update, dash.no_update

    metric = metadata.get('metric')

    if numerator_metric == "Select Window" and denominator_metric == "Select Window":
        return dash.no_update, html.Div([
        html.H4(f"Ratio Matrix Options - Select Windows"),
        html.Div([
            make_window_dropdown('select-window-ratio-num', "Select Window Numerator"), 
            make_window_dropdown('select-window-ratio-den', "Select Window Denominator")
        ])
        ], style={'marginTop': 30})
    elif numerator_metric == "Select Window" or denominator_metric == "Select Window":
    # currently does not support picking Select Window in just the Numerator or Denominator 
    # and having a different metric in the other - place for improvement would be to add this 
    # functionality, perhaps use the callback context here 
        raise Exception

    iColStr = get_iColStr(metric)

    Aselect_num = get_Aselect_from_colname(numerator_metric)
    Aselect_den = get_Aselect_from_colname(denominator_metric)
    
    Avar_num = Aselect_num[:, list_colindices(iColStr)]
    Avar_denom = Aselect_den[:, list_colindices(iColStr)]

    Anum = row_to_zone_matrix(Avar_num, iColStr, Nzones)
    Adenom = row_to_zone_matrix(Avar_denom, iColStr, Nzones)

    ratio_matrix = safe_ratio_matrix(Anum, Adenom)
    df_ratio = pd.DataFrame(ratio_matrix, index=zone_names[::-1], columns=zone_names)

    table = make_zone_matrix('zone-matrix-ratio', df_ratio)

    return html.Div([
        html.H4(f"Zone Matrix Ratio: {numerator_metric} / {denominator_metric}"),
        table
    ]), dash.no_update


@callback(
    Output('ratio-zone-matrix', 'children', allow_duplicate=True),
    Input('select-window-ratio-num', 'value'),
    Input('select-window-ratio-den', 'value'),
    State('selected-metadata', 'data'),
    prevent_initial_call=True
)
def show_ratio_matrix_2selectwindow(numerator_window, denominator_window, metadata):
    """
    Triggered when:
        Dropdowns are selected for the two windows you want the ratio of

    Outputs:
        Gets ratio zone matrix between the two selected windows
    """
    # this is when we want ratio of two windows. 
    if not numerator_window or not denominator_window or not metadata:
        return dash.no_update

    metric = metadata.get("metric")
    iColStr = get_iColStr(metric)

    
    Aselect_num = stats[np.where(statsrows == numerator_window), :]
    Aselect_den = stats[np.where(statsrows == denominator_window), :]

    Avar_num = Aselect_num[:, list_colindices(iColStr)]
    Avar_den = Aselect_den[:, list_colindices(iColStr)]

    Anum = row_to_zone_matrix(Avar_num, iColStr, Nzones)
    Adenom = row_to_zone_matrix(Avar_den, iColStr, Nzones)

    ratio_matrix = safe_ratio_matrix(Anum, Adenom)
    df_ratio = pd.DataFrame(ratio_matrix, index=zone_names[::-1], columns=zone_names)

    table = make_zone_matrix('zone-matrix-ratio-selectwindows', df_ratio)

    return html.Div([
        html.H4(f"Zone Matrix Ratio: {numerator_window.replace(cleaning_string, "")} / {denominator_window.replace(cleaning_string, "")}"),
        table
    ])
