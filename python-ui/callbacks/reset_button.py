from dash import Output, Input, callback

@callback(
    Output('zone-matrix-container', 'children', allow_duplicate=True),
    Output('select-window-zones', 'children', allow_duplicate=True),
    Output('plot-container', 'children', allow_duplicate=True),
    Output('max-window-plot', 'children', allow_duplicate=True),
    Output('ratio-zone-matrix', 'children', allow_duplicate=True),
    Input('reset-button', 'n_clicks'),
    prevent_initial_call=True
)
def reset_view(_):
    """
    Reset Button - when clicked, it empties the containers, just leaving the top-most table
    """
    return "", "", "", "", ""