import Jupyter_UI.utils


WHITE = "#ffffff"
LIGHT_BLUE = "#7cadde"
DARK_BLUE = "#346494"
RED = "#D45D48"
YELLOW = "#FFF99E"
CELL_BORDER = "1px solid #999"
COL_WIDTH_LARGE = "100%"
COL_WIDTH_SMALL = "100px"
CELL_HEIGHT = "40px"

THEME = {
    "primary": DARK_BLUE,
    "secondary": LIGHT_BLUE,
    "background": WHITE,
    "border": CELL_BORDER,
    "col_width": COL_WIDTH_LARGE,
    "small_col_width": COL_WIDTH_SMALL,
    "col_height": CELL_HEIGHT,
    "alert": RED,
    "warning": YELLOW,
}


def button_color(i, j, _):
    """
    Makes the bottom right corner buttons in zone matrix have coloring 
    specified by the THEME dictionary. 
    """
    NZONES = Jupyter_UI.utils.NZONES
    core_rows = {NZONES - 2, NZONES - 1}  # bottom two rows
    core_cols = {0, 1}  # left two columns
    if i in core_rows and j in core_cols:
        return THEME["primary"]
    for r in core_rows:
        for c in core_cols:
            if abs(i - r) <= 1 and abs(j - c) <= 1:
                return THEME["secondary"]
    return THEME["background"]


def anomaly_color(i, j, matrix):
    if matrix[i][j] == "":
        return button_color(i, j, matrix)
    z = float(matrix[i][j])
    if abs(z) >= 3:
        return THEME["alert"]
    elif abs(z) >= 1.5:
        return THEME["warning"]
    else:
        return button_color(i, j, matrix)
