from dash import Dash, html, dcc
import dash_bootstrap_components as dbc
import time 
from helpers import *
from callbacks import push_main_table, push_ratio_tables, reset_button, push_plots, show_window_matrix

# supress callback exceptions - so not everything goes at startup
app = Dash(external_stylesheets=[dbc.themes.BOOTSTRAP], suppress_callback_exceptions=True)

# Top-most table
start_toplevel_time = time.time()
app.layout = dbc.Container([
    html.H2("Select Analysis"),
    make_top_table(),
    html.Div(id='zone-matrix-container', style={'marginTop': 30}),
    html.Div(id='select-window-zones', style={'marginTop': 30}),
    html.Div(id='ratio-zone-matrix', style={'marginTop': 30}),
    html.Div(id='plot-container', style={'marginTop': 30}),
    html.Div(id="max-window-plot", style={'marginTop':30}),
    html.Button("Reset View", id="reset-button", n_clicks=0, style={'marginTop': 20}),
    dcc.Store(id='selected-metadata', data={})
])
# print(f"Making top-level took {time.time() - start_toplevel_time} seconds.")

if __name__ == "__main__":
    app.run(debug=True)