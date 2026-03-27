import numpy as np
import D4M.assoc as ac

MASK = 2863311530
default_zones = [
        "nonroute",
        "bogon",
        "assigned",
        "CAIDA",
        "other",
    ]
COL_LABELS = ["Current", "Median", "Max Value", "Max Window", "Select Window", "Ratio"]
ROW_LABELS = [
    "Packets",
    "Links",
    "Sources",
    "Destinations",
    "Max Packets",
    "Max Source",
    "Max Fan Out",
    "Max Dest",
    "Max Fan In",
    "Ratio",
]
PROPERTIES = {
    "Packets": "n_packets",
    "Links": "n_links",
    "Sources": "n_sources",
    "Destinations": "n_destinations",
    "Max Packets": "max_packets",
    "Max Source": "max_source_packets",
    "Max Fan Out": "max_fan_out",
    "Max Dest": "max_destination_packets",
    "Max Fan In": "max_fan_in",
}

def load_stats(filename, delim="_", cleaning_string=".8388608", dir="."):
    """
    Loads in a statistics file into an associative array, and updates globals according to this file. 
    """
    global stats, DELIM, CLEANING_STR, STATSCOLS, STATSROWS, SUBRANGE_DIR, NTIMEWINDOW, DROPDOWN_WINDOWLIST
    stats = ac.readcsv(filename, labels=True, convert_values=True, delimiter="\t")
    DELIM = delim
    CLEANING_STR = cleaning_string
    STATSCOLS = stats.col
    STATSROWS = stats.row
    SUBRANGE_DIR = dir
    NTIMEWINDOW = len(STATSROWS) - 1  # most recent time window
    DROPDOWN_WINDOWLIST = [
        {"label": v.replace(CLEANING_STR, ""), "value": v} for v in STATSROWS
    ]

def set_zones(zone_list=default_zones):
    """
    Sets global variables relating to zones. 
    """
    global ZONES, DISPLAY_ZONES, NZONES
    ZONES = zone_list
    DISPLAY_ZONES = list(reversed(ZONES))
    NZONES = len(ZONES)


def clean_window_list():
    return [str(v.replace(CLEANING_STR, "")) for v in STATSROWS]
    

def get_Aselect_from_colname(col_name):
    Aselect = None
    if col_name == "Current":
        Aselect = stats[NTIMEWINDOW, :]
    elif col_name == "Median":
        medians = np.median(stats.adj.toarray(), 0)
        Aselect = ac.Assoc(1, STATSCOLS, medians)
    elif col_name == "Max Value":
        maxes = np.max(stats.adj.toarray(), 0)
        Aselect = ac.Assoc(1, STATSCOLS, maxes)
    elif col_name == "Max Window":
        max_idx = np.argmax(stats.adj.toarray(), 0)
        new_vals = [STATSROWS[i].replace(CLEANING_STR, "") for i in max_idx]
        Aselect = ac.Assoc(1, STATSCOLS, new_vals)
    return Aselect


def get_subcol(iColstr):
    return f"sub{DELIM}{iColstr}{DELIM}"


def list_colindices(col_string):
    return [c for c in STATSCOLS if c.startswith(get_subcol(col_string))]


def get_zone_lables(r_i, c_i):
    return DISPLAY_ZONES[r_i], ZONES[c_i]


def get_grb_filename(r_i, c_i):
    return str(r_i) + "_" + str(c_i) + ".grb"


def get_iColStr(prop):
    return PROPERTIES.get(prop)


def get_adjusted_cell_rc(row_idx, col_idx):
    return 5 - row_idx, col_idx + 1


def row_to_zone_matrix(Avar, iColStr, check_val=False):
    """
    Converts a D4M associative array row into an Nzone x Nzone matrix.

    Avar: Assoc row with sub-statistics like sub-max_packets-1_3
    iColStr: string like 'max_packets'
    Nzone: number of zones
    """
    c = Avar.col
    v = Avar.val
    filtered = [s.replace(get_subcol(iColStr), "") for s in c]
    max_val = max([int(x) for s in filtered for x in s.split("_")])
    assert (
        max_val <= NZONES
    ), f"Please define atleast {max_val} zones. Only {NZONES} are currently defined."
    string_matrix = [["" for _ in range(NZONES)] for _ in range(NZONES)]

    # use when the values were stored in val and not in the adjacency array
    if check_val:
        # get list of indices
        c1, c2 = zip(*(s.split("_") for s in filtered))
        c1 = list(map(int, c1))
        c2 = list(map(int, c2))
        for i, place in enumerate(Avar.adj.data):
            val_i = int(place - 1)
            # if v is just an int that means its the same for everything
            if isinstance(v, (int, float)):
                value = v
            else:  # else get the correct val from array
                value = v[val_i]
            string_matrix[c1[i] - 1][c2[i] - 1] = value
    else:
        for i, entry in enumerate(filtered):
            c1, c2 = entry.split("_")
            string_matrix[int(c1) - 1][int(c2) - 1] = Avar.adj.data[i]

    return string_matrix[::-1][:]


def safe_ratio_matrix(Anum, Adenom):
    """
    Convert mixed-type zone matrices to float arrays, compute elementwise ratio safely.
    - Non-numeric entries are treated as 0.0.
    - Division by zero yields np.nan
    """
    ratio = [["" for _ in range(NZONES)] for _ in range(NZONES)]
    for i in range(NZONES):
        for j in range(NZONES):
            num = (
                float(Anum[i][j])
                if isinstance(Anum[i][j], (int, float, np.float64))
                else 0
            )
            denom = (
                float(Adenom[i][j])
                if isinstance(Adenom[i][j], (int, float, np.float64))
                else 0
            )
            ratio[i][j] = num / denom if denom != 0 else ""
    return ratio


def get_statsrow_idx(window):
    return np.where(STATSROWS == window + CLEANING_STR)


def reset_state(state, *, keep_property=False, keep_context=False):
    if not keep_property:
        state.property = None
    if not keep_context:
        state.context = None

    state.window = None


def reset_output(outputs, *, keep_dropdown=False, keep_window_matrix=False):
    outputs.plot.clear_output()
    outputs.zone.clear_output()
    if not keep_dropdown:
        outputs.dropdown.clear_output()
    if not keep_window_matrix:
        outputs.window_matrix.clear_output()

def get_zonepair_timeseries(iColStr, i, j):
    sub_col = get_subcol(iColStr) + f"{i}_{j}"
    subcol_idx = np.where(STATSCOLS == sub_col)

    Avar = stats[:, subcol_idx]
    series = Avar.adj.data
    # Avar_rows = [s.replace(CLEANING_STR, "") for s in Avar.row]

    return series


def zscore_series(x):
    x = np.asarray(x, dtype=float)
    med = np.median(x)
    mad = np.median(np.abs(x - med))

    if mad == 0:
        return np.zeros_like(x)

    return 0.6745 * (x - med) / mad


def zscore_zone_matrix(iColStr, window_idx):

    Z = [["" for _ in range(NZONES)] for _ in range(NZONES)]

    for i in range(1, NZONES + 1):
        for j in range(1, NZONES + 1):
            ts = get_zonepair_timeseries(iColStr, i, j)
            if len(ts) == 0:
                Z[i - 1][j - 1] = ""
            else:
                z = zscore_series(ts)
                length = len(z) - 1
                Z[i - 1][j - 1] = str(z[length])

    return Z[::-1]
