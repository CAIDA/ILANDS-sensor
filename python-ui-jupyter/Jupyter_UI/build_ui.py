from Jupyter_UI.state import UIOutputs, UIState
from Jupyter_UI.sections import section_box
from Jupyter_UI.controls import build_property_context_grid

import ipywidgets as widgets


def build_ui(assoc_array=False, endianess="LE"):
    """
    Builds UIs, each with their own states and outputs, 
    to be displayed in various tabs. 
    Optional:
        assoc_array - For specifying if the subrange data is in associative arrays. 
        endianess - For specifying Big or Little endianess. Valid Strings: ['BE', 'LE']
    """
    outputs = UIOutputs(
        plot=widgets.Output(),
        zone=widgets.Output(),
        window_matrix=widgets.Output(),
        dropdown=widgets.Output(),
    )

    state = UIState()
    state.endianess = endianess
    state.zscores = False
    state.assoc_array = assoc_array

    grid = widgets.GridspecLayout(4, 6)

    top_section = section_box(build_property_context_grid(state, outputs), bg="#d6fbff")
    zone_section = section_box(outputs.zone)
    plot_section = section_box(outputs.plot)
    dropdown_section = section_box(outputs.dropdown)
    window_section = section_box(outputs.window_matrix)

    top_section.layout.max_height = (
        "450px" 
    )
    top_section.layout.min_width = "700px"
    dropdown_section.layout.max_height = "450px"
    window_section.layout.max_height = "350px"
    zone_section.layout.max_height = "350px"
    plot_section.layout.min_height = "500px"
    grid.layout.grid_template_rows = "450px 350px 1fr"
    grid.layout.width = "100%"
    grid[0, 0:4] = top_section
    grid[0, 4:] = dropdown_section
    grid[1, 0:3] = zone_section
    grid[1, 3:] = window_section
    grid[2:, :] = plot_section
    grid[2:, :].layout.height = "auto"

    return grid, state, outputs
