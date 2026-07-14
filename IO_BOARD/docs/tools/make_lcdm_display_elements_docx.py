#!/usr/bin/env python3
import html
import os
import zipfile


OUT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "high_end_tester_lcdm_display_elements.docx")
)


COL_W = 2250
TABLE_W = COL_W * 4

COLORS = {
    "WHITE": "FFFFFF",
    "NAVY": "001060",
    "ROW_BG": "EFF3F5",
    "DARK_GRAY": "444444",
    "BLUE": "004CFF",
    "GREEN": "00A040",
    "RED": "E00000",
    "GRAY": "666666",
    "BLACK": "000000",
}

STATES = [
    {
        "id": "READY",
        "scene": "待机",
        "state": "READY",
        "main": "AUTO TEST",
        "ledm": "AUTO",
        "detail": "PROFILE DB50",
        "sub": "K1 SELF/LEARN  K2 AUTO  K3 RESET  K4 OK",
        "color": "BLUE",
    },
    {
        "id": "SELF",
        "scene": "自检",
        "state": "SELF",
        "main": "SELF TEST",
        "ledm": "A01b01",
        "detail": "OUT001 IN001",
        "sub": "CHECKING...  K3 RESET",
        "color": "BLUE",
    },
    {
        "id": "AUTO",
        "scene": "自动测试",
        "state": "AUTO",
        "main": "AUTO TEST",
        "ledm": "A32b32",
        "detail": "OK 032/092",
        "sub": "RUNNING  K3 RESET",
        "color": "BLUE",
    },
    {
        "id": "LEARN",
        "scene": "学习",
        "state": "LEARN",
        "main": "LEARN MODE",
        "ledm": "LEArn",
        "detail": "OUT048 IN048",
        "sub": "HOLD K1 3S  K4 SAVE",
        "color": "BLUE",
    },
    {
        "id": "PASS",
        "scene": "通过",
        "state": "PASS",
        "main": "PASS",
        "ledm": "PASS",
        "detail": "TOTAL 092 OK",
        "sub": "PRINT READY  REMOVE HARNESS",
        "color": "GREEN",
    },
    {
        "id": "NG",
        "scene": "不良",
        "state": "NG",
        "main": "NG",
        "ledm": "001002",
        "detail": "OUT001 IN002",
        "sub": "SHORT CIRCUIT  K3 RESET",
        "color": "RED",
    },
]


def esc(text):
    return html.escape(str(text), quote=True)


def run(text, size=22, bold=False, color="000000"):
    parts = []
    lines = str(text).split("\n")
    for i, line in enumerate(lines):
        if i:
            parts.append("<w:br/>")
        parts.append(f"<w:t>{esc(line)}</w:t>")
    bold_xml = "<w:b/>" if bold else ""
    return (
        "<w:r><w:rPr>"
        '<w:rFonts w:ascii="Arial" w:hAnsi="Arial" w:eastAsia="PingFang SC"/>'
        f"{bold_xml}<w:color w:val=\"{color}\"/><w:sz w:val=\"{size}\"/><w:szCs w:val=\"{size}\"/>"
        "</w:rPr>"
        + "".join(parts)
        + "</w:r>"
    )


def para(text="", align="left", size=22, bold=False, color="000000", spacing_after=120):
    return (
        "<w:p><w:pPr>"
        f'<w:jc w:val="{align}"/>'
        f'<w:spacing w:after="{spacing_after}"/>'
        "</w:pPr>"
        + (run(text, size=size, bold=bold, color=color) if text else "")
        + "</w:p>"
    )


def heading(text, level=1):
    size = 32 if level == 1 else 26
    return para(text, align="left", size=size, bold=True, color="000000", spacing_after=100)


def cell(text, span=1, fill="FFFFFF", color="000000", align="center", valign="center", size=22, bold=False):
    width = COL_W * span
    grid = f'<w:gridSpan w:val="{span}"/>' if span > 1 else ""
    return (
        "<w:tc><w:tcPr>"
        f'<w:tcW w:w="{width}" w:type="dxa"/>'
        f"{grid}"
        f'<w:shd w:fill="{fill}"/>'
        f'<w:vAlign w:val="{valign}"/>'
        '<w:tcMar><w:top w:w="40" w:type="dxa"/><w:left w:w="70" w:type="dxa"/>'
        '<w:bottom w:w="40" w:type="dxa"/><w:right w:w="70" w:type="dxa"/></w:tcMar>'
        "</w:tcPr>"
        + para(text, align=align, size=size, bold=bold, color=color, spacing_after=0)
        + "</w:tc>"
    )


def row(cells, height):
    return (
        "<w:tr><w:trPr>"
        f'<w:trHeight w:val="{height}" w:hRule="exact"/>'
        "</w:trPr>"
        + "".join(cells)
        + "</w:tr>"
    )


def table(rows_xml, borders=True):
    border_xml = (
        "<w:tblBorders>"
        '<w:top w:val="single" w:sz="8" w:color="777777"/>'
        '<w:left w:val="single" w:sz="8" w:color="777777"/>'
        '<w:bottom w:val="single" w:sz="8" w:color="777777"/>'
        '<w:right w:val="single" w:sz="8" w:color="777777"/>'
        '<w:insideH w:val="single" w:sz="4" w:color="999999"/>'
        '<w:insideV w:val="single" w:sz="4" w:color="999999"/>'
        "</w:tblBorders>"
        if borders
        else ""
    )
    grid = "<w:tblGrid>" + "".join(f'<w:gridCol w:w="{COL_W}"/>' for _ in range(4)) + "</w:tblGrid>"
    return (
        "<w:tbl><w:tblPr>"
        f'<w:tblW w:w="{TABLE_W}" w:type="dxa"/>'
        '<w:tblLayout w:type="fixed"/>'
        f"{border_xml}</w:tblPr>"
        + grid
        + "".join(rows_xml)
        + "</w:tbl>"
    )


def screen_table(state, active_key=None):
    state_color = COLORS[state["color"]]
    rows = []
    rows.append(
        row(
            [
                cell("WIRE TESTER LCDM", span=3, fill=COLORS["NAVY"], color=COLORS["WHITE"], align="left", size=24, bold=True),
                cell("TESTER", span=1, fill=COLORS["NAVY"], color=COLORS["WHITE"], align="right", size=22, bold=True),
            ],
            480,
        )
    )
    rows.append(row([cell("", span=4, fill=COLORS["WHITE"])], 150))
    rows.append(
        row(
            [
                cell(state["state"], span=1, fill=COLORS["ROW_BG"], color=state_color, align="left", size=34, bold=True),
                cell(state["main"], span=3, fill=COLORS["ROW_BG"], color=state_color, align="right", size=34, bold=True),
            ],
            990,
        )
    )
    rows.append(row([cell("", span=4, fill=COLORS["WHITE"])], 60))

    key_cells = []
    keys = [
        ("K1\nSELF/LEARN", "K1"),
        ("K2\nAUTO", "K2"),
        ("K3\nRESET", "K3"),
        ("K4\nOK/SAVE", "K4"),
    ]
    for text, key in keys:
        fill = COLORS["BLUE"] if key == active_key else COLORS["DARK_GRAY"]
        key_cells.append(cell(text, fill=fill, color=COLORS["WHITE"], size=20, bold=True))
    rows.append(row(key_cells, 870))
    rows.append(row([cell("", span=4, fill=COLORS["WHITE"])], 210))
    rows.append(
        row(
            [
                cell(state["ledm"], span=2, fill=COLORS["WHITE"], color=COLORS["BLUE"], size=26, bold=True),
                cell(state["detail"], span=2, fill=COLORS["WHITE"], color=COLORS["BLUE"], size=22, bold=True),
            ],
            510,
        )
    )
    rows.append(row([cell("", span=4, fill=COLORS["WHITE"])], 120))
    rows.append(row([cell(state["sub"], span=4, fill=COLORS["WHITE"], color=COLORS["GRAY"], size=18, bold=False)], 420))
    return table(rows)


def simple_table(headers, data, widths=None):
    rows = []
    rows.append(row([cell(h, fill="D9EAF7", color="000000", align="left", size=20, bold=True) for h in headers], 420))
    for data_row in data:
        rows.append(row([cell(v, fill="FFFFFF", color="000000", align="left", size=18) for v in data_row], 520))
    return table(rows)


def page_break():
    return '<w:p><w:r><w:br w:type="page"/></w:r></w:p>'


def build_document_xml():
    body = []
    body.append(para("高档测试机 LCDM 显示元素表", align="center", size=40, bold=True, spacing_after=160))
    body.append(
        para(
            "本 Word 文件把 LCDM 显示图直接放在前面，后面再列出固定元素、会变化元素、K1-K4 按键元素和状态显示内容。此文件只判断显示内容，不包含第二份坐标表和第三份通讯逻辑表。",
            size=22,
            spacing_after=180,
        )
    )

    body.append(heading("1. LCDM 主界面显示图", 1))
    body.append(para("屏幕规格：480 x 272 横屏。下图为待机 READY 状态的完整界面。", size=21))
    body.append(screen_table(STATES[0]))

    body.append(heading("2. 会变化状态显示图", 1))
    for state in STATES:
        body.append(heading(f"{state['id']} - {state['scene']}", 2))
        body.append(screen_table(state))

    body.append(page_break())
    body.append(heading("3. 固定显示元素", 1))
    body.append(
        simple_table(
            ["元素 ID", "元素名称", "类型", "固定显示内容"],
            [
                ["screen_bg", "全屏背景", "色块", "白色背景，覆盖原内容"],
                ["title_bar", "标题栏背景", "色块", "深蓝色横条"],
                ["title_text", "标题文字", "文本", "WIRE TESTER LCDM"],
                ["role_text", "角色文字", "文本", "TESTER"],
                ["status_panel", "主状态背景", "色块", "浅色状态区"],
                ["key_band", "按键带背景", "色块", "K1-K4 四段按键背景"],
                ["ledm_panel", "LEDM 兼容显示背景", "色块", "白色区域"],
                ["detail_panel", "详情背景", "色块", "白色区域"],
                ["sub_panel", "辅助提示背景", "色块", "白色区域"],
            ],
        )
    )

    body.append(heading("4. 会变化的显示元素", 1))
    body.append(
        simple_table(
            ["元素 ID", "元素名称", "可能显示内容", "变化来源"],
            [
                ["state_text", "状态文字", "READY / SELF / AUTO / LEARN / PASS / NG", "固件状态机"],
                ["main_text", "主显示文字", "AUTO TEST / SELF TEST / LEARN MODE / PASS / NG", "固件状态机"],
                ["ledm_text", "LEDM 兼容显示", "AUTO / A01b01 / PASS / 001002 / LEArn", "LEDM 显示模型"],
                ["detail_text", "详情文字", "PROFILE DB50 / OUT001 IN002 / OK 092/092", "测试流程"],
                ["sub_text", "辅助提示文字", "K1 SELF/LEARN  K2 AUTO  K3 RESET  K4 OK", "当前状态提示"],
                ["touch_debug_text", "触摸调试文字", "T 060,140 -> K1 DOWN", "触摸解析结果，调试阶段可覆盖 sub_text"],
            ],
        )
    )

    body.append(heading("5. K1-K4 按键显示元素", 1))
    body.append(
        simple_table(
            ["元素 ID", "按键", "显示文字", "动作含义"],
            [
                ["key_k1", "K1", "K1 / SELF/LEARN", "短按自检；长按约 3 秒学习"],
                ["key_k2", "K2", "K2 / AUTO", "自动测试"],
                ["key_k3", "K3", "K3 / RESET", "复位到待机"],
                ["key_k4", "K4", "K4 / OK/SAVE", "确认/保存；调试阶段可切 PASS"],
            ],
        )
    )

    body.append(heading("6. 状态内容总表", 1))
    body.append(
        simple_table(
            ["状态", "state_text", "main_text", "ledm_text"],
            [[s["id"], s["state"], s["main"], s["ledm"]] for s in STATES],
        )
    )
    body.append(
        simple_table(
            ["状态", "detail_text", "sub_text", "颜色"],
            [[s["id"], s["detail"], s["sub"], s["color"]] for s in STATES],
        )
    )

    body.append(heading("7. 显示边界", 1))
    body.append(
        simple_table(
            ["项目", "规则", "备注", ""],
            [
                ["显示来源", "所有可见内容都必须来自本文件元素表。", "不依赖旧 HMI 控件。", ""],
                ["动态变化", "动态元素只由状态机、测试流程、LEDM 模型和触摸调试更新。", "不能乱跳页。", ""],
                ["原控件", "高档测试机界面不显示屏内旧项目控件。", "露出旧控件就是错误状态。", ""],
            ],
        )
    )

    sect = (
        "<w:sectPr>"
        '<w:pgSz w:w="11906" w:h="16838"/>'
        '<w:pgMar w:top="720" w:right="720" w:bottom="720" w:left="720" w:header="360" w:footer="360" w:gutter="0"/>'
        "</w:sectPr>"
    )
    return (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">'
        "<w:body>"
        + "".join(body)
        + sect
        + "</w:body></w:document>"
    )


def write_docx(path):
    content_types = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
        '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
        '<Default Extension="xml" ContentType="application/xml"/>'
        '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>'
        "</Types>"
    )
    rels = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>'
        "</Relationships>"
    )
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("[Content_Types].xml", content_types)
        zf.writestr("_rels/.rels", rels)
        zf.writestr("word/document.xml", build_document_xml())


if __name__ == "__main__":
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    write_docx(OUT)
    print(OUT)
