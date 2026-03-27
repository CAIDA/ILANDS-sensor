import graphblas as gb
import numpy as np
import plotly.graph_objects as go
import os
import tarfile
import ipaddress
import Jupyter_UI.utils
from Jupyter_UI.utils import MASK
from Jupyter_UI.theme import THEME


def get_matrix_from_grb(filename):
    """Extract graphblas matrix from .grb file"""
    with open(filename, "rb") as f:
        file_bytes = f.read()  # Read in binary stream
    return gb.Matrix.ss.deserialize(file_bytes)  # deserialize the grb blob


def create_empty_matrix() -> gb.Matrix:
    """Create an empty GraphBLAS matrix."""
    return gb.Matrix(dtype=int, nrows=2**32, ncols=2**32)


def BEuint32toIPv4str(x) -> str:
    """
    Convert a big-endian 32-bit integer to an IPv4 string.
    x is unsigned 32-bit integer
    returns a str IPv4 address string in dotted decimal format.
    """
    # return str(ipaddress.IPv4Address(x))
    x1 = (x >> 24) & 0xFF
    x2 = (x >> 16) & 0xFF
    x3 = (x >> 8) & 0xFF
    x4 = x & 0xFF    
    return f"{x1}.{x2}.{x3}.{x4}"
       
def LEuint32toIPv4str(x) -> str:
    """
    Convert a little-endian 32-bit integer to a IPv4 String.
    """
    x4 = (x >> 24) & 0xFF
    x3 = (x >> 16) & 0xFF
    x2 = (x >> 8) & 0xFF
    x1 = x & 0xFF
    return f"{x1}.{x2}.{x3}.{x4}"


def get_ticks_in_range(axis_min, axis_max, indices, labels, min_pixel_spacing=40):
    """
    Gets the tick marks to use for the spy plot, using a data units per pixel 
    idea to ensure that grid lines and tick marks are not overlapped. 
    Returns the indices and labels of the selected tick marks. 
    """
    if len(indices) == 0:
        return [], []

    # data units per pixel
    min_data_gap = min_pixel_spacing * (axis_max - axis_min) / 480

    selected_indices = [indices[0]]
    selected_labels = [labels[0]]

    last = indices[0]
    for i, lbl in zip(indices[1:], labels[1:]):
        if abs(i - last) >= min_data_gap:
            selected_indices.append(i)
            selected_labels.append(lbl)
            last = i

    return selected_indices, selected_labels


def get_empty_scatterplot_fig():
    """
    Returns an empty scatterplot figure that matches the rest of the UI. 
    """
    fig = go.Figure()
    fig.update_layout(
        autosize=False,
        width=600,
        height=600,
        yaxis_scaleanchor="x",
        yaxis_scaleratio=1,
        xaxis_title="Destinations",
        yaxis_title="Sources",
        plot_bgcolor="white",
    )
    return fig


def analytics_for_grb(grb_matrix, prop, selected_value):
    """
    Returns the triples for the GraphBLAS matrix spy plot, depending 
    on which property is being viewed. 
    """
    endMatrix = grb_matrix
    if prop == "max_packets":
        endMatrix = grb_matrix * gb.select.valueeq(grb_matrix, selected_value).new()
    elif prop == "max_source_packets":
        summed_rows = grb_matrix.reduce_rowwise(gb.monoid.plus)
        endMatrix = (
            (gb.Vector.diag(gb.select.valueeq(summed_rows, selected_value)))
            .mxm(grb_matrix)
            .new()
        )
    elif prop == "max_fan_out":
        nzerorows = grb_matrix.reduce_rowwise(gb.agg.count)
        endMatrix = (
            gb.Vector.diag(gb.select.valueeq(nzerorows, selected_value))
            .mxm(grb_matrix)
            .new()
        )
    elif prop == "max_destination_packets":
        nzerocolumns = grb_matrix.reduce_columnwise(gb.monoid.plus)
        endMatrix = grb_matrix.mxm(
            gb.Vector.diag(gb.select.valueeq(nzerocolumns, selected_value))
        )
    elif prop == "max_fan_in":
        nzerocolumns = grb_matrix.reduce_columnwise(gb.agg.count)
        endMatrix = grb_matrix.mxm(
            gb.Vector.diag(gb.select.valueeq(nzerocolumns, selected_value))
        )
    return endMatrix.to_coo()


def analytics_for_assoc_array(assoc_array, prop, selected_value):
    """
    Returns the triples for the Associative Array spy plot, depending 
    on which property is being viewed. 
    """
    endMatrix = assoc_array 
    if prop == "max_packets":
        endMatrix = assoc_array * (assoc_array == selected_value)
    elif prop == "max_source_packets":
        endMatrix = assoc_array[(assoc_array.sum(1) == selected_value).row, :]
    elif prop == "max_fan_out":
        endMatrix = assoc_array[(assoc_array.logical().sum(1) == selected_value).row,:]
    elif prop == "max_destination_packets":
        endMatrix = assoc_array[:, (assoc_array.sum(0) == selected_value).col]
    elif prop == "max_fan_in":
        endMatrix = assoc_array[:, (assoc_array.logical().sum(0)).col]
    return endMatrix.find()


def get_spy_fig_img(grb_filename, row_name, prop, selected_value, state):
    """
    Returns a figure of a scatter plot to be displayed. 
    Additionally, returns error messages, if any
    """
    # Toggle this to turn off the mask on the IP addresses
    use_mask=True
    error_message = []
    try:
        selected_value = int(float(selected_value))
    except ValueError:
        error_message.append("Please pick a valid value (cannot pick an empty cell)")
        return get_empty_scatterplot_fig(), error_message

    i, j = int(grb_filename[0]), int(grb_filename[2])
    subrange_name = Jupyter_UI.utils.SUBRANGE_DIR + os.sep + str(row_name.replace(Jupyter_UI.utils.CLEANING_STR, ""))

    if not os.path.exists(subrange_name + ".tar") or not os.path.exists(
        subrange_name + ".npy"
    ):
        # just put in the name of what we have if that file doesn't exist
        error_message.append(
            f"tar {subrange_name} does not exist for this time window. Using a placeholder."
        )
        subrange_name = Jupyter_UI.utils.SUBRANGE_DIR+ os.sep + "20240507-152247"

    if state.assoc_array == True:
        table_loaded = np.load(subrange_name + ".npy", allow_pickle=True)
        assoc_array = table_loaded[i - 1][j - 1]
        gbrow, gbcol, gbval = analytics_for_assoc_array(
            assoc_array, prop, selected_value
        )
    else:
        subrange_name = subrange_name + ".tar"
        assert tarfile.is_tarfile(
            subrange_name
        )  # Make sure we actually have a tar file

        with tarfile.open(name=subrange_name, mode="r") as tarball:
            # if there are errors with extracting/find the tar, try:
            # tarball.extract("./"+grb_filename, path=directory)
            tarball.extract(grb_filename, path=Jupyter_UI.utils.SUBRANGE_DIR)

            grb_matrix = get_matrix_from_grb(
                Jupyter_UI.utils.SUBRANGE_DIR + os.sep + grb_filename  # deserialize the matrix
            )
            try:  # deleting .grb file made from the extraction
                os.remove(Jupyter_UI.utils.SUBRANGE_DIR + os.sep + grb_filename)
            except FileNotFoundError:
                pass

            gbrow, gbcol, gbval = analytics_for_grb(grb_matrix, prop, selected_value)

    fig = get_empty_scatterplot_fig()

    if len(gbrow) == 0 or len(gbcol) == 0:
        error_message.append("No values were selected, nothing to plot.")
        return fig, error_message

    if len(gbval) > 1000000:
        error_message.append("Too Large to Display this Matrix")
        return fig, error_message

    unique_rows = np.unique(gbrow)
    unique_cols = np.unique(gbcol)
    if state.endianess == 'LE': func = LEuint32toIPv4str
    else: func = BEuint32toIPv4str
    
    
    # matching labels
    if use_mask:
        row_labels = [
            func(np.uint32(r) ^ np.uint32(MASK)) for r in unique_rows
        ]
        col_labels = [
            func(np.uint32(c) ^ np.uint32(MASK)) for c in unique_cols
        ]

        hover_text = [
            f"Source={func(np.uint32(r)^np.uint32(MASK))}"
            f"<br>Dest.={func(np.uint32(c)^np.uint32(MASK))}"
            f"<br>Val={v}"
            for r, c, v in zip(gbrow, gbcol, gbval)
        ]
    else:
        row_labels = [
            func(int(np.uint32(r))) for r in unique_rows
        ]
        col_labels = [
            func(int(np.uint32(c))) for c in unique_cols
        ]

        hover_text = [
            f"Source={func(int(np.uint32(r)))}"
            f"<br>Dest.={func(int(np.uint32(c)))}"
            f"<br>Val={v}"
            for r, c, v in zip(gbrow, gbcol, gbval)
        ]

    fig.add_trace(
        go.Scatter(
            x=gbcol,
            y=gbrow,
            mode="markers",
            marker_size=3,
            text=hover_text,
            hoverinfo="text",
            line_color=THEME["primary"],
        )
    )
    # Initial ticks
    init_xvals, init_xlabels = get_ticks_in_range(
        min(gbcol), max(gbcol), unique_cols, col_labels
    )
    init_yvals, init_ylabels = get_ticks_in_range(
        min(gbrow), max(gbrow), unique_rows, row_labels
    )

    fig.update_xaxes(
        tickmode="array",
        tickvals=init_xvals,
        ticktext=init_xlabels,
        ticks="outside",
        showline=True,
        linecolor="black",
        gridcolor="lightgrey",
    )
    fig.update_yaxes(
        tickmode="array",
        tickvals=init_yvals,
        ticktext=init_ylabels,
        ticks="outside",
        showline=True,
        linecolor="black",
        gridcolor="lightgrey",
    )

    return fig, error_message


def get_time_series(sub_col):
    """
    Returns a time series plot to be displayed. 
    """
    subcol_idx = np.where(Jupyter_UI.utils.STATSCOLS == sub_col)
    series = []
    Avar = Jupyter_UI.utils.stats[:, subcol_idx]
    series = Avar.adj.data
    Avar_rows = [s.replace(Jupyter_UI.utils.CLEANING_STR, "") for s in Avar.row]
    trace = go.Scatter(x=Avar_rows, y=series, mode="lines", line_color=THEME["primary"])
    fig = go.Figure(
        data=[trace],
    )
    fig.update_layout(
        xaxis_title="Time Windows",
        yaxis_title="Packets",
        yaxis_type="log",
        plot_bgcolor="white",
        margin=dict(l=40, r=40, t=40, b=40),
        # width=1200,
        height=600,
    )
    fig.update_xaxes(
        ticks="outside", showline=True, linecolor="black", gridcolor="lightgrey"
    )
    fig.update_yaxes(
        ticks="outside", showline=True, linecolor="black", gridcolor="lightgrey"
    )

    return fig
