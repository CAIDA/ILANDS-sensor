from dataclasses import dataclass
import ipywidgets as widgets


@dataclass
class UIState:
    property = None
    context = None
    window = None
    assoc_array = False
    zscores = False
    endianess = 'BE'


@dataclass
class UIOutputs:
    plot: widgets.Output
    zone: widgets.Output
    window_matrix: widgets.Output
    dropdown: widgets.Output
