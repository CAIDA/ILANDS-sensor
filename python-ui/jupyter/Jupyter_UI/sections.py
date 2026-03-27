# reusuable widgets

import ipywidgets as widgets
from Jupyter_UI.theme import THEME


def section_box(content, bg="#f7f7f7"):
    """
    Creates a widget box to surround sections and delineate them. 
    """
    box = widgets.VBox(
        [content],
        layout=widgets.Layout(
            width="100%",
            padding="12px",
            margin="10px 0",
            border=THEME["border"],
            border_radius="8px",
            background=bg,
        ),
    )

    return box


def title_box(title, widget):
    """
    Styles a title in HTML, placing it in a box widget. 
    """
    return widgets.VBox(
        [
            widgets.HTML(
                f"""
                <div style="
                    text-align: center;
                    font-size: 18px;
                    font-weight: 600;
                    margin-bottom: 10px;
                ">
                    {title}
                </div>
                """
            ),
            widget,
        ]
    )


def make_dropdown(options, description):
    """
    Widget dropdown maker
    """
    dd = widgets.Dropdown(options=options, description=description)
    return dd


def render_error_list(errors):
    """
    Formats a list of text into bullet point HTML messages
    """
    if not errors:
        return widgets.HTML("")
    html = "<ul style='margin: 0; padding-left: 18px;'>"
    for err in errors:
        html += f"<li>{err}</li>"
    html += "</ul>"
    return widgets.HTML(html)
