import tarfile
from dash import dash_table, dcc
import os
import pandas as pd
import graphblas as gb
import D4M.assoc as ac
import numpy as np
import plotly.graph_objects as go

# ------------------------------
# Constants and Styling
# ------------------------------

# Load initial stats
stats = ac.readcsv("GraphChallenge_analyses_subrange_m_23_concat.tsv", labels=True, convert_values=True, delimiter="\t")
# stats = ac.readcsv("stats_random.tsv", labels=True, convert_values=True, delimiter="\t")
statsrows = stats.row
statscols = stats.col
NtimeWindow = len(statsrows) - 1 # we want the number of time windows, ie rows in this analysis table to get most recent time window
cleaning_string = ".8388608"
dropdown_windowlist = [{'label': v.replace(cleaning_string, ''), 'value': v} for v in statsrows]

col_labels = [
    "Current", "Median", "Max Value", "Max Window",
    "Select Window", "Ratio"
]
row_labels = [
    "Packets", "Links", "Sources", "Destinations",
    "Max Packets", "Max Source", "Max Fan Out", "Max Dest",
    "Max Fan In", "Ratio"
]

# Patch metric names to stats column name keys
metric_col_overrides = {
    "packets": "n_packets",
    "links": "n_links",
    "sources": "n_sources",
    "destinations": "n_destinations",
    "max_source": "max_source_packets",
    "max_dest": "max_destination_packets"
}
zone_names = ["nonroute", "bogon", "assigned", "CAIDA", "other"]
Nzones = len(zone_names)
dark_blue = "#0E4B87"
light_blue =  "#5C97D2"
dark_b_text_color = "#FFFFFF" #white for readability 


# coloring for the zone matrix: 
style_data_conditional = [{'if': {'row_index': 3, 'column_id': zone_names[2]}, 'backgroundColor':light_blue }, 
                          {'if': {'row_index': 4, 'column_id': zone_names[2]}, 'backgroundColor': light_blue }]                              
target_rows = [3,4]
target_cols = zone_names[:2]
for row_idx in target_rows:
    for col_id in target_cols:
        style_data_conditional.append({
            'if': {'row_index': row_idx, 'column_id': col_id},
            'backgroundColor': dark_blue,  # Deep blue
            'color': dark_b_text_color  # white for readability
        })
target_cols.append(zone_names[2])
for col_id in target_cols:
    style_data_conditional.append({
        'if': {'row_index': 2, 'column_id': col_id},
        'backgroundColor': light_blue,  # lighter blue
    })

top_data = [{"Metric": row, **{col: "**>>**" for col in col_labels}} for row in row_labels]
df_top = pd.DataFrame(top_data)
light_gray = "#c9c9c9"
top_data_style = [
    {
        "if": {
            "filter_query": '{Metric} = "Ratio"',
            "column_id": "Max Window"
        },
        "backgroundColor": light_gray,
        "color": "black",
        "pointer-events": "none"
    },
    {
        "if": {
            "filter_query": '{Metric} = "Ratio"',
            "column_id": "Ratio"
        },
        "backgroundColor": light_gray,
        "color": "black",
        "pointer-events": "none"
    }
]

# ------------------------------
# Helper Functions
# ------------------------------
def make_top_table():
    return dash_table.DataTable(
        id='top-table',
        columns=[{"name": " ", "id": "Metric"}] +
                [{"name": col, "id": col, "presentation": "markdown"} for col in col_labels],
        data=df_top.to_dict('records'),
        style_table={'overflowX': 'auto'},
        style_cell={'textAlign': 'center'},
        cell_selectable=True, 
        style_data_conditional=top_data_style
    )

def get_cell_rc(active_cell):
    return active_cell['row'], active_cell['column']

def get_adjusted_cell_rc(active_cell):
    row_idx, col_idx = get_cell_rc(active_cell)
    return 4-row_idx, col_idx -1

def get_iColStr(metric):
    return metric_col_overrides.get(metric.lower().replace(' ', '_'), metric.lower().replace(' ', '_'))

def get_zone_lables(r_i, c_i):
    return zone_names[r_i], zone_names[c_i]

def get_grb_filename(row_i, col_i):
    return str(row_i+1)+"_"+str(col_i+1)+".grb"

def list_colindices(col_string):
    return [c for c in stats.col if c.startswith(f"sub_{col_string}_")]

def get_matrix_from_grb(filename):
    """Extract graphblas matrix from .grb file"""
    with open(filename, "rb") as f:
        file_bytes = f.read()                           # Read in binary stream
    return gb.Matrix.ss.deserialize(file_bytes)         # deserialize the grb blob

def make_column_dropdown(dropdown_id, placeholder_text):
    return dcc.Dropdown(
                    id=dropdown_id,
                    options=[{'label': col, 'value': col} for col in col_labels[:-1]  if col != "Max Window"],
                    placeholder=placeholder_text,
                    style={"width": "45%", "display": "inline-block"}
                )

def make_row_dropdown(dropdown_id, placeholder_text):
    return dcc.Dropdown(
                    id=dropdown_id,
                    options=[{'label': row, 'value': row} for row in row_labels[:-1]],
                    placeholder=placeholder_text,
                    style={"width": "30vw"}
                )

def make_window_dropdown(dropdown_id, placeholder_text):
    return dcc.Dropdown(
                id=dropdown_id,
                options=dropdown_windowlist,
                placeholder=placeholder_text,
                style={"width": "30vw"}
            )

def get_Aselect_from_colname(col_name):
    Aselect = None
    if col_name == "Current":
        Aselect = stats[NtimeWindow, :] 
    elif col_name == "Median":
        medians = np.median(stats.adj.toarray(), 0)
        Aselect = ac.Assoc(1, statscols, medians)
    elif col_name == "Max Value":
        maxes = np.max(stats.adj.toarray(), 0)
        Aselect = ac.Assoc(1,statscols, maxes)
    elif col_name == "Max Window":
        max_idx = np.argmax(stats.adj.toarray(),0)
        new_vals = [statsrows[i].replace(cleaning_string, '') for i in max_idx]
        Aselect = ac.Assoc(1, statscols, new_vals)

    return Aselect

def safe_ratio_matrix(Anum, Adenom):
    """
    Convert mixed-type zone matrices to float arrays, compute elementwise ratio safely.
    - Non-numeric entries are treated as 0.0.
    - Division by zero yields np.nan
    """
    Nz = len(Anum)
    ratio = np.zeros((Nz, Nz), dtype=float)

    for i in range(Nz):
        for j in range(Nz):
            try:
                num = float(Anum[i][j]) if isinstance(Anum[i][j], (int, float, np.float64)) else 0.0
            except:
                num = 0.0
            try:
                denom = float(Adenom[i][j]) if isinstance(Adenom[i][j], (int, float, np.float64)) else 0.0
            except:
                denom = 0.0

            ratio[i][j] = num / denom if denom != 0 else np.nan

    return ratio

def row_to_zone_matrix(Avar, iColStr, Nzone, check_val=False):
    """
    Converts a D4M associative array row into an Nzone x Nzone matrix.
    
    Avar: Assoc row with sub-statistics like sub-max_packets-1_3
    iColStr: string like 'max_packets'
    Nzone: number of zones
    """
    c = Avar.col
    v = Avar.val
    filtered = [s.replace(f"sub_{iColStr}_", "") for s in c]
    string_matrix = [["" for _ in range(Nzone)] for _ in range(Nzone)]

    # use when the values were stored in val and not in the adjacency array
    if check_val:
        # get list of indices
        c1, c2 = zip(*(s.split('_') for s in filtered))
        c1 = list(map(int, c1))
        c2 = list(map(int, c2))
        for i, place in enumerate(Avar.adj.data):
            val_i = int(place-1)
            # if v is just an int that means its the same for everything
            if isinstance(v, (int, float)):
                value = v
            else:   # else get hte correct val from array
                value = v[val_i]
            string_matrix[c1[i]-1][c2[i]-1] = value
    else:
        for i, entry in enumerate(filtered):
            c1, c2 = entry.split('_')
            string_matrix[int(c1)-1][int(c2)-1] = Avar.adj.data[i]

    return string_matrix[::-1][:]

def make_zone_matrix(id_name, df):
    return dash_table.DataTable(
        id=id_name,
        columns=[{"name": "Zone", "id": "Zone"}] + [{"name": z, "id": z} for z in zone_names],
        data=[{"Zone": idx, **row} for idx, row in df.iterrows()],
        style_cell={'textAlign': 'center'},
        cell_selectable=True,
        style_data_conditional=style_data_conditional
    )

def BEuint32toIPv4str(x: int) -> str:
    """
    Convert a big-endian 32-bit integer to an IPv4 string.
    x is unsigned 32-bit integer
    returns a str IPv4 address string in dotted decimal format.
    """
    x1 = (x >> 24) & 0xFF
    x2 = (x >> 16) & 0xFF
    x3 = (x >> 8) & 0xFF
    x4 = x & 0xFF
    return f"{x1}.{x2}.{x3}.{x4}"

def get_ticks_in_range(axis_min, axis_max, indices, labels, max_ticks=10):
    """
    using current visible range of axis, and all possible tick positions/labels
    gets a subset of indices/labels to be visible on the graph so its not crowded
    """
    # get current visible (will be all in the beginnign)
    mask = (indices >= axis_min) & (indices <= axis_max)
    visible_indices = indices[mask]
    visible_labels = np.array(labels)[mask]

    # reduce visible by taking a sample of every step 
    # still causes issues whenever things are too clustered at some parts
    #TODO: have the axes labels recomputed when graph is zoomed in 
    if len(visible_indices) > max_ticks:
        step = max(1, len(visible_indices) // max_ticks)
        visible_indices = visible_indices[::step]
        visible_labels = visible_labels[::step]

    return visible_indices, visible_labels

def get_spy_fig_img(grb_filename, row_name, metric, selected_val):
    range_tarname = "./"+ str(row_name) + ".tar"

    if not os.path.exists(range_tarname):
        # just put in the name of what we have if that file doesn't exist
        range_tarname = "20240507-152247.tar"
    
    assert tarfile.is_tarfile(range_tarname)          # Make sure we actually have a tar file
    directory = os.path.dirname(range_tarname)
    directory = "." if directory == "" else directory    # get directory where extracted files will be placed

    with tarfile.open(name=range_tarname, mode="r") as tarball:
        # if there are errors with extracting/find the tar, try:
        # tarball.extract("./"+grb_filename, path=directory)

        tarball.extract(grb_filename, path=directory)
        grb_matrix = get_matrix_from_grb(
            directory + "/" + grb_filename                # deserialize the matrix
        )
        try:                                                            # deleting .grb file made from the extraction
            os.remove(directory + "/" + grb_filename)
        except FileNotFoundError:
            pass

    print(f"{selected_val=}")

    coo_matrix = gb.io.to_scipy_sparse(grb_matrix, format='coo')

    if metric in ["n_packets", "n_links", "n_sources", "n_destinations"]:
        gbrow, gbcol, gbval = coo_matrix.row, coo_matrix.col, coo_matrix.data

    #TODO: ADD SPECIALIZED BEHAVIOR FOR EACH METRIC
    elif metric == 'max_packets':
        # [i j v] = find(AsumSrcDest .* (AsumSrcDest == selected_Value));
        # Maximum packets on a link
        # max_packets = matrix.reduce_scalar(gb.monoid.max).value
        pass 
    elif metric == 'max_source_packets':
        # [i j v] = find(diag(sum(AsumSrcDest,2) == selected_Value) * AsumSrcDest);
        # Maximum packets from a source
        # nonzero_rows = matrix.reduce_rowwise(gb.monoid.plus)
        # max_source_packets = nonzero_rows.reduce(gb.monoid.max).value
        # max_source_packets = 0 if max_source_packets is None else int(max_source_packets)
        pass 
    elif metric == "max_fan_out":
        # Maximum fan-out
        # nonzero_rows = matrix.reduce_rowwise(gb.agg.count)
        # max_fan_out = nonzero_rows.reduce(gb.monoid.max).value
        # max_fan_out = 0 if max_fan_out is None else int(max_fan_out)
        # [i j v] = find(diag(sum(sign(AsumSrcDest),2) == selected_Value) * AsumSrcDest);
        pass 
    elif metric == "max_destination_packets":
        # Maximum packets to a destination
        # nonzero_columns = matrix.reduce_columnwise(gb.monoid.plus)
        # max_destination_packets = nonzero_columns.reduce(gb.monoid.max).value
        # max_destination_packets = 0 if max_destination_packets is None else int(max_destination_packets)
        # [i j v] = find(AsumSrcDest * diag(sum(AsumSrcDest,1) == selected_Value));
        pass
    elif metric =="max_fan_in":
        # Maximum fan-in
        # nonzero_columns = matrix.reduce_columnwise(gb.agg.count)
        # max_fan_in = nonzero_columns.reduce(gb.monoid.max).value
        # max_fan_in = 0 if max_fan_in is None else int(max_fan_in)
        # [i j v] = find(AsumSrcDest * diag(sum(sign(AsumSrcDest),1) == selected_Value));
        pass
    
    #remove the following line once the if-else above is filled in correctly 
    gbrow, gbcol, gbval = coo_matrix.row, coo_matrix.col, coo_matrix.data


    mask = 2863311530  

    if len(gbval) > 1000000:
        raise Exception("Too Large to Display this Matrix")

    unique_rows = np.unique(gbrow)
    unique_cols = np.unique(gbcol)

    # matching labels
    row_labels = [BEuint32toIPv4str(int(np.uint32(r) ^ np.uint32(mask))) for r in unique_rows]
    col_labels = [BEuint32toIPv4str(int(np.uint32(c) ^ np.uint32(mask))) for c in unique_cols]

    hover_text = [
    f"row={BEuint32toIPv4str(int(np.uint32(r)^np.uint32(mask)))}"
    f"<br>col={BEuint32toIPv4str(int(np.uint32(c)^np.uint32(mask)))}"
    f"<br>val={v}"
    for r, c, v in zip(gbrow, gbcol, gbval)
    ]

    fig = go.Figure()
    fig.add_trace(
        go.Scatter(
            x=gbcol,
            y=gbrow,
            mode="markers",
            marker=dict(size=3),
            text=hover_text,
            hoverinfo="text",
        )
    )
 # Initial ticks
    init_xvals, init_xlabels = get_ticks_in_range(min(gbcol), max(gbcol), unique_cols, col_labels)
    init_yvals, init_ylabels = get_ticks_in_range(min(gbrow), max(gbrow), unique_rows, row_labels)

    fig.update_xaxes(
        tickmode="array",
        tickvals=init_xvals,
        ticktext=init_xlabels,
    )
    fig.update_yaxes(
        tickmode="array",
        tickvals=init_yvals,
        ticktext=init_ylabels,
    )

    fig.update_layout(
        autosize=False,
        width=600,
        height=600,
        yaxis=dict(scaleanchor="x", scaleratio=1),
    )

    return dcc.Graph(
        id="spy-graph",
        figure=fig,
        style={"width": "600px", "height": "600px"},
    )