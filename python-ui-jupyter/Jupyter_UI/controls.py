import ipywidgets as widgets
from ipywidgets import interact
from functools import partial
import Jupyter_UI.utils

from Jupyter_UI.utils import (
    ROW_LABELS,
    COL_LABELS,
    get_subcol,
    reset_output,
    reset_state,
    get_statsrow_idx,
    get_zone_lables,
    get_iColStr,
    get_adjusted_cell_rc,
    get_grb_filename,
    row_to_zone_matrix,
    safe_ratio_matrix,
    list_colindices,
    clean_window_list,
    zscore_zone_matrix,
    get_Aselect_from_colname,
)
from Jupyter_UI.plotting import get_spy_fig_img
from Jupyter_UI.sections import render_error_list, title_box, make_dropdown
from Jupyter_UI.theme import THEME, button_color, anomaly_color
from Jupyter_UI.plotting import get_time_series

def on_property_context_click(i, j, _btn, *, state, outputs):
    """This is the button behavior for the top-level table between 
        network quantities and contexts. It updates the state, and refreshes
        any drop downs or zone matrices as necessary. """
    reset_state(state)
    reset_output(outputs)
    state.property = ROW_LABELS[i]
    state.context = COL_LABELS[j]
    refresh_controls(state, outputs)
    refresh_zone_matrix(state, outputs)


def on_window_selected(window, state, outputs):
    """Button behavior """
    outputs.plot.clear_output()
    state.window = window
    refresh_zone_matrix(state, outputs)


# could probably make this better with more smaller functions
def on_zone_cell_click(row, col, btn, *, state, outputs):
    """
    Button behavoir for clicking on a zone matrix. Makes plots, max window zone matrix, 
    or no output (if its a ratio zone matrix)
    """
    
    outputs.plot.clear_output()
    prop = state.property
    col_name = state.context
    row_zone, col_zone = get_zone_lables(row, col)
    row, col = get_adjusted_cell_rc(row, col)
    iColStr = get_iColStr(prop)
    selected_val = btn.description
    CLEANING_STR = Jupyter_UI.utils.CLEANING_STR

    if prop == "Ratio":
        return

    if col_name in ["Current", "Select Window"]:
        grb_filename = get_grb_filename(row, col)
        window = state.window
        row_name = window + CLEANING_STR
        fig, error_message = get_spy_fig_img(
            grb_filename, row_name, iColStr, selected_val, state
        )
        with outputs.plot:
            display(render_error_list(error_message))
            display(
                title_box(
                    f"{prop}: {row_zone} → {col_zone} for time window {window.replace(CLEANING_STR, '')}",
                    widgets.HTML(),
                )
            )
            fig.show()
    elif col_name == "Max Window":
        try:
            row_name = state.window + CLEANING_STR
            selected_val = float(selected_val)
            grb_filename = get_grb_filename(row, col)
            fig, error_message = get_spy_fig_img(
                grb_filename, row_name, iColStr, selected_val, state
            )
            with outputs.plot:
                display(render_error_list(error_message))
                display(
                    title_box(
                        f"{prop}: {row_zone} → {col_zone} for time window {state.window}",
                        widgets.HTML(),
                    )
                )
                fig.show()
        except:
            outputs.window_matrix.clear_output()
            with outputs.window_matrix:
                state.window = selected_val
                statsrow_idx = get_statsrow_idx(state.window)
                Avar = Jupyter_UI.utils.stats[statsrow_idx, list_colindices(iColStr)]
                AsumTable = row_to_zone_matrix(Avar, iColStr)
                display(
                    title_box(
                        f"{prop} x {col_name} Zone Matrix for window {state.window}",
                        render_zone_matrix(AsumTable, state, outputs),
                    )
                )

    elif col_name in ["Median", "Max Value"]:
        with outputs.plot:
            sub_col = get_subcol(iColStr) + f"{row}_{col}"
            fig = get_time_series(sub_col)
            display(
                title_box(
                    f"Time Series for {prop}: {row_zone} → {col_zone}",
                    widgets.HTML(),
                )
            )
            fig.show()


def render_zone_matrix(matrix, state, outputs):
    """
    Responsible for the HTML and widget rendering of a zone matrix. 
    Returns a Widget Gridbox filled with widgets and labels. 
    """
    if state.zscores:
        func = anomaly_color
    else:
        func = button_color
    NZONES = Jupyter_UI.utils.NZONES
    ZONES = Jupyter_UI.utils.ZONES
    children = []
    width = THEME["col_width"]
    height = THEME["col_height"]
    border = THEME["border"]
    children.append(
        widgets.HTML(
            "<div style='height: 100%; text-align: center'><b>Zone</b>",
            layout=widgets.Layout(width=width, height=height),
        )
    )
    for col_name in ZONES:
        children.append(
            widgets.HTML(
                f"<div style='height: 100%; display: flex; align-items: center; justify-content: center; border-right:{border}'><b>{col_name}</b>",
                layout=widgets.Layout(width=width, height=height),
            )
        )
    for i, row_name in enumerate(Jupyter_UI.utils.DISPLAY_ZONES):
        children.append(
            widgets.HTML(
                f"<div style='height: 100%; display: flex; align-items: center; justify-content: center; border-right:{border}; border-bottom:{border}'><b>{row_name}</b>",
                layout=widgets.Layout(width=width, height=height),
            )
        )
        for j in range(NZONES):
            btn = widgets.Button(
                description=str(matrix[i][j]),
                tooltip=f"{row_name}→{ZONES[j]}",
                layout=widgets.Layout(border=border, height=height, width=width),
                style={"button_color": func(i, j, matrix)},
            )
            btn.on_click(
                partial(on_zone_cell_click, i, j, state=state, outputs=outputs)
            )
            children.append(btn)

    return widgets.GridBox(
        children,
        layout=widgets.Layout(
            grid_template_columns=f"repeat({NZONES+1},{100//(NZONES+1)}%)"
        ),
    )


def build_property_context_grid(state, outputs):
    """
    Responsible for the HTML and table rendering of the top level table. 
    Outputs a widget gridbox filled with clickable buttons. 
    """
    children = []
    width = THEME["col_width"]
    border = THEME["border"]
    children.append(
        widgets.HTML(
            "<b> </b>",
            layout=widgets.Layout(width=width),
        )
    )
    for ctx in COL_LABELS:
        children.append(
            widgets.HTML(
                f"<div style='align-items: center; justify-content: center; display: flex; border-right:{border};'><b>{ctx}</b>",
                layout=widgets.Layout(width=width),
            )
        )
    for i, prop in enumerate(ROW_LABELS):
        children.append(
            widgets.HTML(
                f"<div style='align-items: center; justify-content: center; display: flex; border-right:{border}; border-bottom:{border}'><b>{prop}</b>",
                layout=widgets.Layout(width=width),
            )
        )
        for j, ctx in enumerate(COL_LABELS):

            if (i, j) in [(9, 3), (9, 5)]:
                btn = widgets.Button(
                    description=" ",
                    style={"button_color": THEME["background"]},
                    layout=widgets.Layout(width=width),
                )
            else:
                btn = widgets.Button(
                    description="→",
                    tooltip=f"{prop} / {ctx}",
                    layout=widgets.Layout(border=border, width=width),
                    style={"button_color": THEME["background"]},
                )
                btn.on_click(
                    partial(
                        on_property_context_click, i, j, state=state, outputs=outputs
                    )
                )
            children.append(btn)

    return widgets.GridBox(
        children,
        layout=widgets.Layout(
            grid_template_columns=f"repeat({len(COL_LABELS)+1}, {100//(len(COL_LABELS)+1)}%)"
        ),
    )


def refresh_zone_matrix(state, outputs):
    """
    Refreshs zone matrix based on upper level clicks. 
    """
    reset_output(outputs, keep_dropdown=True, keep_window_matrix=True)
    prop = state.property
    col_name = state.context
    iColStr = get_iColStr(prop)
    with outputs.zone:
        if col_name == "Select Window":
            if state.window is None:
                return
            window = state.window
            statsrow_idx = get_statsrow_idx(window)
            Avar = Jupyter_UI.utils.stats[statsrow_idx, list_colindices(iColStr)]
            AsumTable = row_to_zone_matrix(Avar, iColStr)
            display(
                title_box(
                    f"{prop} x {col_name} Zone Matrix for window {window}",
                    render_zone_matrix(AsumTable, state, outputs),
                )
            )
        elif col_name == "Ratio" or prop == "Ratio":
            return
        else:
            if state.zscores:
                AsumTable = zscore_zone_matrix(iColStr, Jupyter_UI.utils.NTIMEWINDOW)
            else:
                Aselect = get_Aselect_from_colname(col_name)
                Avar = Aselect[
                    :,
                    [c for c in Jupyter_UI.utils.STATSCOLS if c.startswith(get_subcol(iColStr))],
                ]
                AsumTable = row_to_zone_matrix(
                    Avar, iColStr, check_val=(col_name == "Max Window")
                )
                if col_name == "Current":
                    state.window = Jupyter_UI.utils.STATSROWS[Jupyter_UI.utils.NTIMEWINDOW]
            display(
                title_box(
                    f"{prop} x {col_name} Zone Matrix",
                    render_zone_matrix(AsumTable, state, outputs),
                )
            )


def refresh_controls(state, outputs):
    """
    Refreshes the dropdown controls if select window or ratio was clicked. 
    """
    outputs.zone.clear_output()
    with outputs.dropdown:
        outputs.dropdown.clear_output()
        widgets_to_show = []

        if state.context == "Select Window":
            window_data = clean_window_list()
            window_dd = make_dropdown(window_data, "Select Time Window")
            if state.property != "Ratio":
                interact(
                    lambda window: on_window_selected(window, state, outputs),
                    window=window_dd,
                )

        if state.property == "Ratio":
            num_dd = make_dropdown(ROW_LABELS[:-1], "Numerator")
            den_dd = make_dropdown(ROW_LABELS[:-1], "Denominator")
            if state.context == "Select Window":
                interact(
                    lambda numerator, denominator, window: ratio_row_select(
                        numerator, denominator, window, state, outputs
                    ),
                    numerator=num_dd,
                    denominator=den_dd,
                    window=window_dd,
                )
            else:
                interact(
                    lambda numerator, denominator: ratio_row(
                        numerator, denominator, state, outputs
                    ),
                    numerator=num_dd,
                    denominator=den_dd,
                )

        if state.context == "Ratio":
            column_dropdown_num = make_dropdown(COL_LABELS[:-3], "Numerator:")
            column_dropdown_den = make_dropdown(COL_LABELS[:-3], "Denominator:")
            interact(
                lambda numerator, denominator: ratio_column(
                    numerator, denominator, state, outputs
                ),
                numerator=column_dropdown_num,
                denominator=column_dropdown_den,
            )

        display(widgets.VBox(widgets_to_show))

        

def ratio_column(numerator, denominator, state, outputs):
    """
    Responsible for the behavior of the ratio column. Outputs a zone matrix. 
    """
    reset_output(outputs, keep_dropdown=True)
    iColStr = get_iColStr(state.property)
    Aselect_num = get_Aselect_from_colname(numerator)
    Aselect_den = get_Aselect_from_colname(denominator)
    Avar_num = Aselect_num[:, list_colindices(iColStr)]
    Avar_denom = Aselect_den[:, list_colindices(iColStr)]
    Anum = row_to_zone_matrix(Avar_num, iColStr)
    Adenom = row_to_zone_matrix(Avar_denom, iColStr)
    ratio_matrix = safe_ratio_matrix(Anum, Adenom)
    with outputs.zone:
        display(
            title_box(
                f"Ratio of {numerator} / {denominator} Zone Matrix for {state.property}",
                render_zone_matrix(ratio_matrix, state, outputs),
            )
        )


def ratio_row(numerator, denominator, state, outputs):
    """
    Responsible for the behavior in the ratio row, creating a ratio matrix to be displayed. 
    """
    outputs.plot.clear_output()
    outputs.window_matrix.clear_output()
    num_col = get_iColStr(numerator)

    denom_col = get_iColStr(denominator)
    Aselect = get_Aselect_from_colname(state.context)

    Avar_num = Aselect[:, list_colindices(num_col)]
    Avar_denom = Aselect[:, list_colindices(denom_col)]

    Anum = row_to_zone_matrix(Avar_num, num_col)
    Adenom = row_to_zone_matrix(Avar_denom, denom_col)

    ratio_matrix = safe_ratio_matrix(Anum, Adenom)
    with outputs.zone:
        display(
            title_box(
                f"Ratio of {numerator} / {denominator} Zone Matrix for {state.context}",
                render_zone_matrix(ratio_matrix, state, outputs),
            )
        )


def ratio_row_select(numerator, denominator, window, state, outputs):
    """
    Specific behavior for the ratio-select_window button. 
    Returns the corresponding ratio zone matrix. 
    """
    reset_output(outputs, keep_dropdown=True)
    num_col = get_iColStr(numerator)
    denom_col = get_iColStr(denominator)

    Aselect = Jupyter_UI.utils.stats[get_statsrow_idx(window), :]

    Avar_num = Aselect[:, list_colindices(num_col)]
    Avar_denom = Aselect[:, list_colindices(denom_col)]

    Anum = row_to_zone_matrix(Avar_num, num_col)
    Adenom = row_to_zone_matrix(Avar_denom, denom_col)

    ratio_matrix = safe_ratio_matrix(Anum, Adenom)
    with outputs.zone:
        display(
            title_box(
                f"Ratio of {numerator} / {denominator} Zone Matrix for {window}",
                render_zone_matrix(ratio_matrix, state, outputs),
            )
        )
