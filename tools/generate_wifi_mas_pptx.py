#!/usr/bin/env python3
import html
import zipfile
from pathlib import Path

EMU = 914400
SLIDE_W = 13.333333
SLIDE_H = 7.5

NS = {
    "p": "http://schemas.openxmlformats.org/presentationml/2006/main",
    "a": "http://schemas.openxmlformats.org/drawingml/2006/main",
    "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
}


def emu(v):
    return int(v * EMU)


def esc(s):
    return html.escape(str(s), quote=True)


def color(hex_color):
    return hex_color.replace("#", "").upper()


def text_runs(lines, font_size=18, bold=False, fill="1F2937"):
    if isinstance(lines, str):
        lines = [lines]
    out = []
    for i, line in enumerate(lines):
        if i:
            out.append("<a:br/>")
        out.append(
            f'<a:r><a:rPr lang="zh-CN" sz="{font_size * 100}" b="{1 if bold else 0}">'
            f'<a:solidFill><a:srgbClr val="{color(fill)}"/></a:solidFill>'
            f'<a:latin typeface="Arial"/><a:ea typeface="Microsoft YaHei"/></a:rPr>'
            f"<a:t>{esc(line)}</a:t></a:r>"
        )
    return "".join(out)


def shape(idx, x, y, w, h, text="", fill="FFFFFF", line="CBD5E1", radius=True,
          font_size=16, bold=False, align="ctr", text_color="111827"):
    prst = "roundRect" if radius else "rect"
    return f"""
    <p:sp>
      <p:nvSpPr><p:cNvPr id="{idx}" name="shape{idx}"/><p:cNvSpPr/><p:nvPr/></p:nvSpPr>
      <p:spPr>
        <a:xfrm><a:off x="{emu(x)}" y="{emu(y)}"/><a:ext cx="{emu(w)}" cy="{emu(h)}"/></a:xfrm>
        <a:prstGeom prst="{prst}"><a:avLst/></a:prstGeom>
        <a:solidFill><a:srgbClr val="{color(fill)}"/></a:solidFill>
        <a:ln w="12700"><a:solidFill><a:srgbClr val="{color(line)}"/></a:solidFill></a:ln>
      </p:spPr>
      <p:txBody>
        <a:bodyPr wrap="square" anchor="mid"/>
        <a:lstStyle/>
        <a:p><a:pPr algn="{align}"/>{text_runs(text, font_size, bold, text_color)}</a:p>
      </p:txBody>
    </p:sp>
    """


def textbox(idx, x, y, w, h, text, font_size=18, bold=False, color_hex="111827", align="l"):
    return f"""
    <p:sp>
      <p:nvSpPr><p:cNvPr id="{idx}" name="text{idx}"/><p:cNvSpPr txBox="1"/><p:nvPr/></p:nvSpPr>
      <p:spPr>
        <a:xfrm><a:off x="{emu(x)}" y="{emu(y)}"/><a:ext cx="{emu(w)}" cy="{emu(h)}"/></a:xfrm>
        <a:prstGeom prst="rect"><a:avLst/></a:prstGeom>
        <a:noFill/><a:ln><a:noFill/></a:ln>
      </p:spPr>
      <p:txBody>
        <a:bodyPr wrap="square"/>
        <a:lstStyle/>
        <a:p><a:pPr algn="{align}"/>{text_runs(text, font_size, bold, color_hex)}</a:p>
      </p:txBody>
    </p:sp>
    """


def arrow(idx, x1, y1, x2, y2, line="64748B", width=2):
    flip_h = ' flipH="1"' if x2 < x1 else ""
    flip_v = ' flipV="1"' if y2 < y1 else ""
    off_x = min(x1, x2)
    off_y = min(y1, y2)
    ext_x = abs(x2 - x1) or 0.01
    ext_y = abs(y2 - y1) or 0.01
    return f"""
    <p:cxnSp>
      <p:nvCxnSpPr><p:cNvPr id="{idx}" name="arrow{idx}"/><p:cNvCxnSpPr/><p:nvPr/></p:nvCxnSpPr>
      <p:spPr>
        <a:xfrm{flip_h}{flip_v}><a:off x="{emu(off_x)}" y="{emu(off_y)}"/><a:ext cx="{emu(ext_x)}" cy="{emu(ext_y)}"/></a:xfrm>
        <a:prstGeom prst="line"><a:avLst/></a:prstGeom>
        <a:ln w="{int(width * 12700)}">
          <a:solidFill><a:srgbClr val="{color(line)}"/></a:solidFill>
          <a:tailEnd type="triangle"/>
        </a:ln>
      </p:spPr>
      <p:style><a:lnRef idx="1"><a:schemeClr val="accent1"/></a:lnRef><a:fillRef idx="0"><a:schemeClr val="accent1"/></a:fillRef><a:effectRef idx="0"><a:schemeClr val="accent1"/></a:effectRef><a:fontRef idx="minor"><a:schemeClr val="tx1"/></a:fontRef></p:style>
    </p:cxnSp>
    """


def bullet_box(idx, x, y, w, h, title, bullets, fill="F8FAFC", accent="2563EB"):
    items = [f"• {b}" for b in bullets]
    return (
        shape(idx, x, y, w, 0.42, title, fill=accent, line=accent, radius=False,
              font_size=16, bold=True, text_color="FFFFFF")
        + shape(idx + 1, x, y + 0.42, w, h - 0.42, items, fill=fill, line="CBD5E1",
                radius=False, font_size=14, align="l")
    )


def slide_xml(title, subtitle, body):
    return f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:sld xmlns:p="{NS['p']}" xmlns:a="{NS['a']}" xmlns:r="{NS['r']}">
  <p:cSld>
    <p:bg><p:bgPr><a:solidFill><a:srgbClr val="F8FAFC"/></a:solidFill><a:effectLst/></p:bgPr></p:bg>
    <p:spTree>
      <p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>
      <p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr>
      {textbox(2, 0.45, 0.25, 12.4, 0.45, title, 25, True, "0F172A")}
      {textbox(3, 0.48, 0.73, 12.2, 0.32, subtitle, 10, False, "64748B") if subtitle else ""}
      {body}
      {textbox(900, 10.7, 7.08, 2.1, 0.22, "IO BOARD WiFi / MAS", 8, False, "94A3B8", "r")}
    </p:spTree>
  </p:cSld>
  <p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>
</p:sld>"""


def rels_xml():
    return """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"/>"""


def build_slides():
    slides = []

    body = (
        textbox(4, 0.75, 1.7, 7.2, 0.9, "每条拉本地闭环生产，MAS 负责上级管理和统计", 28, True, "0F172A")
        + textbox(5, 0.78, 2.75, 6.7, 1.1, ["先固定组网与互联框架", "每条拉配置微型/工业 AP", "WiFi 模块型号后续再选"], 20, False, "334155")
        + shape(6, 8.2, 1.45, 3.9, 0.75, "车间十几条拉", "DBEAFE", "60A5FA", True, 18, True, text_color="1E3A8A")
        + shape(7, 8.2, 2.45, 3.9, 0.75, "每条拉约 10 台测试机", "DCFCE7", "4ADE80", True, 18, True, text_color="166534")
        + shape(8, 8.2, 3.45, 3.9, 0.75, "每条拉 1 台 AP + 打印主机", "FEF3C7", "FBBF24", True, 18, True, text_color="92400E")
        + shape(9, 8.2, 4.45, 3.9, 0.75, "MAS 断线仍可本线打印", "FCE7F3", "F472B6", True, 18, True, text_color="9D174D")
    )
    slides.append(("WiFi / MAS 组网架构说明", "用于设计评审、演示和后续跟踪", body))

    body = (
        bullet_box(4, 0.6, 1.25, 3.65, 4.7, "为什么需要 MAS", [
            "统一监控所有测试机和打印机",
            "集中保存测试、打印、报警记录",
            "支持电脑、手机、手持机查看",
            "为后续数据分析提供完整资料",
        ], "FFFFFF", "2563EB")
        + bullet_box(8, 4.85, 1.25, 3.65, 4.7, "不能直接互连", [
            "多台测试机抢同一台打印机",
            "跨线统计和追溯困难",
            "换机、换打印机管理复杂",
            "APP 无法稳定同时监控多条线",
        ], "FFFFFF", "DC2626")
        + bullet_box(12, 9.1, 1.25, 3.65, 4.7, "MAS 的定位", [
            "监控中心",
            "统计中心",
            "资料版本中心",
            "历史追溯中心",
        ], "FFFFFF", "059669")
    )
    slides.append(("MAS 在系统中的角色", "MAS 负责上级管理和统计，不是本线打印的前置条件", body))

    body = (
        shape(4, 4.35, 1.15, 4.6, 0.65, "MAS / Monitor / Statistics Server", "E0F2FE", "0284C7", True, 16, True, text_color="075985")
        + shape(5, 4.65, 2.2, 4.0, 0.55, "Factory Ethernet / VLAN / Uplink", "E2E8F0", "64748B", True, 14, True)
        + arrow(6, 6.65, 2.2, 6.65, 1.8, "2563EB")
        + shape(7, 0.75, 3.25, 3.6, 0.55, "Line 01 微型/工业 AP", "DBEAFE", "60A5FA", True, 14, True)
        + shape(8, 4.85, 3.25, 3.6, 0.55, "Line 02 微型/工业 AP", "DBEAFE", "60A5FA", True, 14, True)
        + shape(9, 8.95, 3.25, 3.6, 0.55, "Line NN 微型/工业 AP", "DBEAFE", "60A5FA", True, 14, True)
        + arrow(10, 6.65, 2.75, 2.55, 3.25) + arrow(11, 6.65, 2.75, 6.65, 3.25) + arrow(12, 6.65, 2.75, 10.75, 3.25)
        + shape(13, 0.55, 4.25, 3.95, 1.45, ["ST01 ... ST10 WiFi Station", "AT32 Print Host", "Printer"], "FFFFFF", "CBD5E1", False, 13)
        + shape(14, 4.65, 4.25, 3.95, 1.45, ["ST01 ... ST10 WiFi Station", "AT32 Print Host", "Printer"], "FFFFFF", "CBD5E1", False, 13)
        + shape(15, 8.75, 4.25, 3.95, 1.45, ["ST01 ... ST10 WiFi Station", "AT32 Print Host", "Printer"], "FFFFFF", "CBD5E1", False, 13)
        + textbox(16, 0.7, 6.25, 12.0, 0.38, "正式方案：每条拉 1 个独立微型/工业 AP；AP 只提供网络，生产逻辑由本线 AT32 打印主机完成。", 16, True, "334155", "ctr")
    )
    slides.append(("车间网络拓扑", "每条拉有本地 AP 和打印主机，MAS 断线不影响本线打印", body))

    body = (
        shape(4, 0.55, 1.35, 2.45, 0.85, "设备层\n测试机 + WiFi", "DCFCE7", "22C55E", True, 15, True, text_color="166534")
        + shape(5, 3.25, 1.35, 2.45, 0.85, "线体层\nAP + 打印主机", "FEF3C7", "F59E0B", True, 15, True, text_color="92400E")
        + shape(6, 5.95, 1.35, 2.45, 0.85, "车间层\nVLAN / Uplink", "E2E8F0", "64748B", True, 15, True)
        + shape(7, 8.65, 1.35, 2.45, 0.85, "MAS 层\n监控/统计/DB", "DBEAFE", "2563EB", True, 15, True, text_color="1E3A8A")
        + shape(8, 11.2, 1.35, 1.65, 0.85, "客户端\nAPP/电脑", "FCE7F3", "EC4899", True, 14, True, text_color="9D174D")
        + arrow(9, 3.0, 1.78, 3.25, 1.78) + arrow(10, 5.7, 1.78, 5.95, 1.78) + arrow(11, 8.4, 1.78, 8.65, 1.78) + arrow(12, 11.1, 1.78, 11.2, 1.78)
        + bullet_box(20, 0.55, 3.0, 2.45, 2.6, "测试机", ["本地扫描", "PASS/NG", "故障点", "上传事件"], "FFFFFF", "16A34A")
        + bullet_box(24, 3.25, 3.0, 2.45, 2.6, "AT32 打印主机", ["本线主控", "打印队列", "本地缓存", "继续打印"], "FFFFFF", "D97706")
        + bullet_box(28, 5.95, 3.0, 2.45, 2.6, "网络", ["VLAN 隔离", "MAS 同步", "NTP", "远程维护"], "FFFFFF", "475569")
        + bullet_box(32, 8.65, 3.0, 2.45, 2.6, "MAS", ["全车间监控", "历史记录", "统计报表", "资料管理"], "FFFFFF", "2563EB")
        + bullet_box(36, 11.2, 3.0, 1.65, 2.6, "客户端", ["查看", "报警", "统计", "追溯"], "FFFFFF", "DB2777")
    )
    slides.append(("系统分层", "上层业务协议不绑定 WiFi 模块，模块以后可替换", body))

    steps = [
        ("1", "测试机 -> 本线主机\nheartbeat"),
        ("2", "本地扫描\nPASS/NG"),
        ("3", "结果到打印主机\n缓存/可同步 MAS"),
        ("4", "PB8 触发\n打印请求"),
        ("5", "打印主机\n排队打印"),
        ("6", "打印状态\nDONE/ERROR"),
    ]
    body = ""
    x = 0.65
    for i, (_, txt) in enumerate(steps):
        body += shape(10+i, x + i*2.05, 2.2, 1.55, 0.9, txt, "FFFFFF", "93C5FD", True, 13, True, text_color="1E3A8A")
        if i < len(steps)-1:
            body += arrow(30+i, x + i*2.05 + 1.55, 2.65, x + (i+1)*2.05, 2.65)
    body += bullet_box(50, 0.9, 4.25, 3.4, 1.65, "测试机", ["不直接控制打印机", "发给本线打印主机"], "FFFFFF", "16A34A")
    body += bullet_box(54, 4.95, 4.25, 3.4, 1.65, "打印主机", ["本线队列", "驱动打印机", "缓存结果"], "FFFFFF", "D97706")
    body += bullet_box(58, 9.0, 4.25, 3.4, 1.65, "MAS", ["接收同步", "统计报表", "刷新电脑/APP"], "FFFFFF", "2563EB")
    slides.append(("正常测试与打印流程", "打印关键路径在本线闭环内完成，MAS 只接收同步和做统计", body))

    body = (
        shape(4, 0.65, 1.35, 2.4, 0.75, "测试机 ST01-ST10", "DCFCE7", "22C55E", True, 15, True)
        + arrow(5, 3.05, 1.72, 3.7, 1.72)
        + shape(6, 3.7, 1.35, 2.45, 0.75, "本线 AP", "DBEAFE", "2563EB", True, 15, True)
        + arrow(7, 6.15, 1.72, 6.8, 1.72)
        + shape(8, 6.8, 1.35, 2.65, 0.75, "打印主机\n本地队列/缓存", "FEF3C7", "F59E0B", True, 14, True)
        + arrow(9, 9.45, 1.72, 10.1, 1.72)
        + shape(10, 10.1, 1.35, 2.1, 0.75, "打印机", "FFFFFF", "94A3B8", True, 15, True)
        + shape(11, 6.8, 3.05, 2.65, 0.75, "MAS 离线\n不影响打印", "FEE2E2", "EF4444", True, 14, True)
        + arrow(12, 8.12, 2.1, 8.12, 3.05, "EF4444")
        + bullet_box(20, 0.95, 4.45, 3.2, 1.55, "MAS 断线", ["本线 AP 仍工作", "打印主机继续接收请求"], "FFFFFF", "DC2626")
        + bullet_box(24, 4.95, 4.45, 3.2, 1.55, "缓存位置", ["主要存在 AT32 打印主机", "测试机只做短缓存"], "FFFFFF", "D97706")
        + bullet_box(28, 8.95, 4.45, 3.2, 1.55, "恢复后", ["打印主机按 event_id 补传", "MAS 去重并生成统计"], "FFFFFF", "2563EB")
    )
    slides.append(("MAS 断线时仍可生产打印", "MAS 不是打印前置条件；本线打印主机是 AT32 打印主机", body))

    body = (
        shape(4, 0.75, 1.45, 2.2, 0.8, "K1+K4\n进入对码", "FFFFFF", "22C55E", True, 14, True)
        + arrow(5, 2.95, 1.85, 3.55, 1.85)
        + shape(6, 3.55, 1.45, 2.2, 0.8, "PAIR_REQ\nUID/短码", "FFFFFF", "22C55E", True, 14, True)
        + arrow(7, 5.75, 1.85, 6.35, 1.85)
        + shape(8, 6.35, 1.45, 2.2, 0.8, "LCDM 选择\n绑定/替换", "FFFFFF", "F59E0B", True, 14, True)
        + arrow(9, 8.55, 1.85, 9.15, 1.85)
        + shape(10, 9.15, 1.45, 2.7, 0.8, "PAIR_ASSIGN\nLine/Station/Profile", "FFFFFF", "2563EB", True, 13, True)
        + arrow(11, 10.5, 2.25, 10.5, 3.0)
        + shape(12, 9.15, 3.0, 2.7, 0.8, "PAIR_ACK\n保存成功", "FFFFFF", "22C55E", True, 14, True)
        + bullet_box(20, 0.85, 4.55, 3.4, 1.35, "换机原则", ["工位不变", "新 UID 替换旧 UID", "旧设备 inactive"], "FFFFFF", "0F766E")
        + bullet_box(24, 4.95, 4.55, 3.4, 1.35, "资料恢复", ["产品资料", "学习线序", "打印规则", "无需重新学习"], "FFFFFF", "7C3AED")
        + bullet_box(28, 9.05, 4.55, 3.4, 1.35, "记录连续", ["本线先保存", "binding_event 后同步"], "FFFFFF", "2563EB")
    )
    slides.append(("对码与换机流程", "打印主机本地完成换机；MAS 在线即同步，离线后补传", body))

    body = (
        shape(4, 0.65, 1.35, 2.2, 0.8, "PC 上位机\nCSV 编辑/导入", "FFFFFF", "2563EB", True, 14, True)
        + arrow(5, 2.85, 1.75, 3.45, 1.75)
        + shape(6, 3.45, 1.35, 2.35, 0.8, "本线 AP\n只提供网络", "DBEAFE", "60A5FA", True, 14, True)
        + arrow(7, 5.8, 1.75, 6.4, 1.75)
        + shape(8, 6.4, 1.15, 2.75, 1.2, "AT32 打印主机\n校验/保存/生成 CRC", "FEF3C7", "F59E0B", True, 13, True, text_color="92400E")
        + arrow(9, 9.15, 1.75, 9.75, 1.75)
        + shape(10, 9.75, 1.35, 2.45, 0.8, "ST01-ST10\nprofile_push", "DCFCE7", "22C55E", True, 14, True)
        + shape(11, 6.4, 3.0, 2.75, 0.75, "MAS 在线\n接收 profile_sync", "E0F2FE", "0284C7", True, 13, True, text_color="075985")
        + arrow(12, 7.78, 2.35, 7.78, 3.0, "2563EB")
        + bullet_box(20, 0.85, 4.45, 3.4, 1.45, "CSV 内容", ["OUT/IN 对应关系", "开路/短路/GND/NC", "必测标志和版本"], "FFFFFF", "2563EB")
        + bullet_box(24, 4.95, 4.45, 3.4, 1.45, "主机校验", ["范围检查", "重复/冲突检查", "profile_rev + crc32"], "FFFFFF", "D97706")
        + bullet_box(28, 9.05, 4.45, 3.4, 1.45, "断网处理", ["使用已保存 profile", "继续生产打印", "恢复后补传 MAS"], "FFFFFF", "059669")
    )
    slides.append(("CSV / Profile 下发", "测试规划先到本线 AT32 打印主机，再由打印主机分发给各测试机", body))

    body = (
        shape(4, 0.65, 1.3, 2.4, 0.65, "telemetry", "DBEAFE", "2563EB", True, 15, True)
        + shape(5, 3.25, 1.3, 2.4, 0.65, "event", "DCFCE7", "16A34A", True, 15, True)
        + shape(6, 5.85, 1.3, 2.4, 0.65, "state", "FEF3C7", "F59E0B", True, 15, True)
        + shape(7, 8.45, 1.3, 2.4, 0.65, "cmd", "FCE7F3", "DB2777", True, 15, True)
        + shape(8, 11.05, 1.3, 1.65, 0.65, "ack", "E0E7FF", "6366F1", True, 15, True)
        + textbox(9, 0.65, 2.35, 12.0, 0.5, "Topic: factory/{factory}/workshop/{workshop}/line/{line}/device/{uid}/{suffix}", 16, True, "334155", "ctr")
        + bullet_box(20, 0.75, 3.25, 3.6, 1.85, "设备 -> 打印主机", ["heartbeat", "test_result", "print_request", "alarm"], "FFFFFF", "2563EB")
        + bullet_box(24, 4.85, 3.25, 3.6, 1.85, "打印主机 <-> MAS", ["缓存同步", "profile_sync", "summary", "history"], "FFFFFF", "DB2777")
        + bullet_box(28, 8.95, 3.25, 3.6, 1.85, "API / Dashboard", ["历史查询", "统计报表", "手机 APP", "电脑监控"], "FFFFFF", "059669")
    )
    slides.append(("MQTT / API 数据通道", "本线消息先到打印主机；打印主机再同步 MAS", body))

    body = (
        shape(4, 0.6, 1.45, 1.55, 0.55, "BOOT", "FFFFFF", "94A3B8", True, 13, True)
        + shape(5, 2.45, 1.45, 1.95, 0.55, "WIFI_CONNECT", "FFFFFF", "94A3B8", True, 12, True)
        + shape(6, 4.75, 1.45, 1.85, 0.55, "REGISTER", "FFFFFF", "94A3B8", True, 13, True)
        + shape(7, 6.95, 1.45, 1.95, 0.55, "SYNC_CONFIG", "FFFFFF", "94A3B8", True, 12, True)
        + shape(8, 9.25, 1.45, 1.55, 0.55, "READY", "DCFCE7", "22C55E", True, 13, True)
        + shape(9, 11.1, 1.45, 1.55, 0.55, "TESTING", "FEF3C7", "F59E0B", True, 13, True)
        + arrow(20, 2.15, 1.73, 2.45, 1.73) + arrow(21, 4.4, 1.73, 4.75, 1.73)
        + arrow(22, 6.6, 1.73, 6.95, 1.73) + arrow(23, 8.9, 1.73, 9.25, 1.73) + arrow(24, 10.8, 1.73, 11.1, 1.73)
        + shape(10, 2.0, 3.2, 1.8, 0.6, "LEARNING", "FFFFFF", "A78BFA", True, 13, True)
        + shape(11, 4.25, 3.2, 1.8, 0.6, "PASS_READY", "DCFCE7", "22C55E", True, 13, True)
        + shape(12, 6.55, 3.2, 2.15, 0.6, "PRINT_REQUESTED", "DBEAFE", "2563EB", True, 12, True)
        + shape(13, 9.15, 3.2, 1.8, 0.6, "PRINT_DONE", "DCFCE7", "22C55E", True, 13, True)
        + arrow(25, 11.85, 2.0, 5.15, 3.2) + arrow(26, 6.05, 3.5, 6.55, 3.5) + arrow(27, 8.7, 3.5, 9.15, 3.5)
        + shape(14, 1.0, 5.05, 2.5, 0.6, "PAIRING", "FCE7F3", "DB2777", True, 13, True)
        + shape(15, 4.3, 5.05, 2.5, 0.6, "OFFLINE_CACHE", "FEE2E2", "EF4444", True, 13, True)
        + shape(16, 7.6, 5.05, 2.5, 0.6, "ERROR_LOCK", "FEE2E2", "EF4444", True, 13, True)
        + textbox(17, 0.8, 6.28, 12.0, 0.35, "原则：本地测试不依赖 MAS；测试机连不上打印主机时短缓存，恢复后先补给打印主机。", 15, True, "334155", "ctr")
    )
    slides.append(("测试机状态机", "网络只是上传、同步和打印请求通道，本地测试必须独立运行", body))

    body = (
        shape(4, 0.9, 1.35, 1.65, 0.6, "BOOT", "FFFFFF", "94A3B8", True, 13, True)
        + shape(5, 3.0, 1.35, 2.0, 0.6, "REGISTERING", "FFFFFF", "94A3B8", True, 13, True)
        + shape(6, 5.45, 1.35, 2.25, 0.6, "LOAD_CONFIG", "FFFFFF", "94A3B8", True, 13, True)
        + shape(7, 8.15, 1.35, 1.9, 0.6, "LINE_READY", "DCFCE7", "22C55E", True, 13, True)
        + shape(8, 10.5, 1.35, 1.8, 0.6, "PRINTING", "FEF3C7", "F59E0B", True, 13, True)
        + arrow(20, 2.55, 1.65, 3.0, 1.65) + arrow(21, 5.0, 1.65, 5.45, 1.65) + arrow(22, 7.7, 1.65, 8.15, 1.65) + arrow(23, 10.05, 1.65, 10.5, 1.65)
        + bullet_box(30, 0.85, 3.0, 3.5, 1.95, "本线主机", ["保存 ST01-ST10 绑定", "保存当前产品资料", "保存标签模板"], "FFFFFF", "D97706")
        + bullet_box(34, 4.9, 3.0, 3.5, 1.95, "打印队列", ["接收 print_request", "生成标签", "回传 DONE/ERROR"], "FFFFFF", "2563EB")
        + bullet_box(38, 8.95, 3.0, 3.5, 1.95, "离线能力", ["MAS 离线仍可打印", "本地缓存后补传"], "FFFFFF", "059669")
    )
    slides.append(("打印主机状态机", "每条线保留一台打印主机，负责本线绑定、队列、打印和缓存", body))

    body = (
        bullet_box(4, 0.65, 1.3, 3.0, 4.5, "主表", [
            "devices",
            "line_bindings",
            "products",
            "profiles",
            "test_results",
            "print_jobs",
            "device_events",
            "alarms",
        ], "FFFFFF", "2563EB")
        + bullet_box(8, 4.05, 1.3, 4.0, 4.5, "统计输出", [
            "每条线产量",
            "工位 PASS/NG 率",
            "常见故障点 OUT/IN",
            "打印失败率",
            "设备在线率",
            "单件 serial 追溯",
        ], "FFFFFF", "059669")
        + bullet_box(12, 8.55, 1.3, 3.9, 4.5, "客户端显示", [
            "车间总览",
            "线体状态",
            "单机状态",
            "报警确认",
            "趋势报表",
            "换机辅助",
        ], "FFFFFF", "DB2777")
    )
    slides.append(("数据记录与统计", "测试记录、打印记录、报警记录统一入库，后续数据分析从数据库生成", body))

    body = (
        bullet_box(4, 0.7, 1.25, 3.7, 4.8, "断网场景", [
            "测试机到打印主机断开",
            "打印主机到 MAS 断开",
            "MAS 服务器离线",
            "重复上报",
            "时间未同步",
        ], "FFFFFF", "DC2626")
        + bullet_box(8, 4.85, 1.25, 3.7, 4.8, "处理策略", [
            "本地继续测试",
            "本线继续打印",
            "打印主机为主缓存",
            "event_id 去重",
            "恢复后补传",
        ], "FFFFFF", "2563EB")
        + bullet_box(12, 9.0, 1.25, 3.3, 4.8, "建议容量", [
            "测试机 200-1000 条短缓存",
            "打印主机 5000-50000 条",
            "打印机本体不做业务缓存",
            "优先用 eMMC/SD/外部 Flash/FRAM",
        ], "FFFFFF", "059669")
    )
    slides.append(("断网缓存与可靠性", "MAS 异常不影响本线打印；缓存主要放在 AT32 打印主机", body))

    body = (
        shape(4, 0.85, 1.45, 2.3, 0.75, "业务层\nscan/print/pair", "DCFCE7", "22C55E", True, 14, True)
        + shape(5, 3.65, 1.45, 2.3, 0.75, "mas_protocol\nJSON/ACK", "DBEAFE", "2563EB", True, 14, True)
        + shape(6, 6.45, 1.45, 2.3, 0.75, "net_service\nUART 通道", "FEF3C7", "F59E0B", True, 14, True)
        + shape(7, 9.25, 1.45, 2.8, 0.75, "ESP32-C3-WROOM\nWiFi 协处理器", "FCE7F3", "DB2777", True, 13, True)
        + arrow(8, 3.15, 1.83, 3.65, 1.83) + arrow(9, 5.95, 1.83, 6.45, 1.83) + arrow(10, 8.75, 1.83, 9.25, 1.83)
        + bullet_box(20, 0.9, 3.15, 3.2, 2.2, "第一版优先", ["打印主机: WROOM-02U", "测试机: 02U 或 02", "AT32 仍做业务主控"], "FFFFFF", "2563EB")
        + bullet_box(24, 4.8, 3.15, 3.2, 2.2, "PCB 风险", ["19-25 内部 GND", "必须按规格书回流焊", "天线区域严格禁布"], "FFFFFF", "DC2626")
        + bullet_box(28, 8.7, 3.15, 3.2, 2.2, "维修友好", ["可做 WiFi 子板", "主板只留 J_WIFI", "后续换型不大改板"], "FFFFFF", "059669")
    )
    slides.append(("WiFi 模块边界", "第一版按 ESP32-C3-WROOM-02U/02 设计，AT32 保持业务主控", body))

    body = (
        shape(4, 0.65, 1.25, 2.2, 0.75, "AT32 PC3候选\nWIFI_TX", "FFFFFF", "22C55E", True, 12, True)
        + arrow(5, 2.85, 1.62, 3.55, 1.62)
        + shape(6, 3.55, 1.25, 2.2, 0.75, "ESP32 Pin11\nRXD", "FFFFFF", "DB2777", True, 13, True)
        + shape(7, 0.65, 2.25, 2.2, 0.75, "AT32 PB9候选\nWIFI_RX", "FFFFFF", "22C55E", True, 12, True)
        + arrow(8, 3.55, 2.62, 2.85, 2.62)
        + shape(9, 3.55, 2.25, 2.2, 0.75, "ESP32 Pin12\nTXD", "FFFFFF", "DB2777", True, 13, True)
        + shape(10, 6.45, 1.25, 2.4, 0.75, "EN不占AT32\n上拉+RESET_TP", "FEF3C7", "F59E0B", True, 11, True)
        + shape(11, 9.35, 1.25, 2.4, 0.75, "BOOT不占AT32\n上拉+BOOT_TP", "FEF3C7", "F59E0B", True, 11, True)
        + shape(12, 6.45, 2.25, 2.4, 0.75, "3V3_WIFI\n>=500mA 峰值", "DBEAFE", "2563EB", True, 13, True)
        + shape(13, 9.35, 2.25, 2.4, 0.75, "GND 焊盘\n含内部 19-25", "DBEAFE", "2563EB", True, 13, True)
        + bullet_box(20, 0.85, 4.15, 3.4, 1.7, "原理图", ["PC3->RXD, PB9<-TXD", "PA1/PA3/PD8 禁用", "EN/BOOT 只留测试点"], "FFFFFF", "2563EB")
        + bullet_box(24, 4.95, 4.15, 3.4, 1.7, "电源", ["模块旁 10uF + 0.1uF", "可加 22uF/47uF", "WiFi 分支预留磁珠/0R"], "FFFFFF", "D97706")
        + bullet_box(28, 9.05, 4.15, 3.4, 1.7, "布局", ["02U 外接天线", "02 板载天线禁布", "优先参考刘氏PDF规格书"], "FFFFFF", "059669")
    )
    slides.append(("WiFi PCB 连接", "参考 docs/esp32-c3-wroom-02_datasheet_cn.pdf 和 wifi_module_pcb_connection_plan.md", body))

    body = (
        bullet_box(4, 0.7, 1.15, 3.9, 5.1, "阶段 1: 架构与原型", [
            "设备身份和绑定表",
            "MQTT topic / JSON 消息",
            "CSV/Profile 上传流程",
            "WiFi PCB 连接规格",
        ], "FFFFFF", "2563EB")
        + bullet_box(8, 4.75, 1.15, 3.9, 5.1, "阶段 2: 固件接入", [
            "net_service 抽象",
            "event_queue 断网缓存",
            "print_request WiFi 后端",
            "pair_req / pair_assign",
        ], "FFFFFF", "059669")
        + bullet_box(12, 8.8, 1.15, 3.9, 5.1, "阶段 3: 模块验证", [
            "WROOM-02U 直贴",
            "WiFi 子板",
            "天线和距离测试",
            "可靠性和干扰测试",
        ], "FFFFFF", "D97706")
    )
    slides.append(("下一步实施计划", "先做本线 AP + 打印主机闭环，再接 MAS 同步和模块验证", body))

    return slides


def content_types(n):
    overrides = [
        '<Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/>',
        '<Override PartName="/ppt/slideMasters/slideMaster1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml"/>',
        '<Override PartName="/ppt/slideLayouts/slideLayout1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml"/>',
        '<Override PartName="/ppt/theme/theme1.xml" ContentType="application/vnd.openxmlformats-officedocument.theme+xml"/>',
    ]
    for i in range(1, n + 1):
        overrides.append(f'<Override PartName="/ppt/slides/slide{i}.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>')
    return f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  {''.join(overrides)}
</Types>"""


def root_rels():
    return """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="ppt/presentation.xml"/>
</Relationships>"""


def presentation_xml(n):
    ids = "\n".join([f'<p:sldId id="{255+i}" r:id="rId{i}"/>' for i in range(1, n + 1)])
    return f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:presentation xmlns:p="{NS['p']}" xmlns:a="{NS['a']}" xmlns:r="{NS['r']}" saveSubsetFonts="1">
  <p:sldMasterIdLst><p:sldMasterId id="2147483648" r:id="rId{n+1}"/></p:sldMasterIdLst>
  <p:sldIdLst>{ids}</p:sldIdLst>
  <p:sldSz cx="{emu(SLIDE_W)}" cy="{emu(SLIDE_H)}" type="wide"/>
  <p:notesSz cx="6858000" cy="9144000"/>
  <p:defaultTextStyle/>
</p:presentation>"""


def presentation_rels(n):
    rels = []
    for i in range(1, n + 1):
        rels.append(f'<Relationship Id="rId{i}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide{i}.xml"/>')
    rels.append(f'<Relationship Id="rId{n+1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster" Target="slideMasters/slideMaster1.xml"/>')
    return f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">{''.join(rels)}</Relationships>"""


def simple_master():
    return f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:sldMaster xmlns:p="{NS['p']}" xmlns:a="{NS['a']}" xmlns:r="{NS['r']}">
  <p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr></p:spTree></p:cSld>
  <p:clrMap bg1="lt1" tx1="dk1" bg2="lt2" tx2="dk2" accent1="accent1" accent2="accent2" accent3="accent3" accent4="accent4" accent5="accent5" accent6="accent6" hlink="hlink" folHlink="folHlink"/>
  <p:sldLayoutIdLst><p:sldLayoutId id="2147483649" r:id="rId1"/></p:sldLayoutIdLst>
  <p:txStyles><p:titleStyle/><p:bodyStyle/><p:otherStyle/></p:txStyles>
</p:sldMaster>"""


def simple_layout():
    return f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:sldLayout xmlns:p="{NS['p']}" xmlns:a="{NS['a']}" xmlns:r="{NS['r']}" type="blank" preserve="1">
  <p:cSld name="Blank"><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr></p:spTree></p:cSld>
  <p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>
</p:sldLayout>"""


def simple_theme():
    return """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<a:theme xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" name="IO Board">
  <a:themeElements>
    <a:clrScheme name="IO Board"><a:dk1><a:srgbClr val="111827"/></a:dk1><a:lt1><a:srgbClr val="FFFFFF"/></a:lt1><a:dk2><a:srgbClr val="334155"/></a:dk2><a:lt2><a:srgbClr val="F8FAFC"/></a:lt2><a:accent1><a:srgbClr val="2563EB"/></a:accent1><a:accent2><a:srgbClr val="059669"/></a:accent2><a:accent3><a:srgbClr val="D97706"/></a:accent3><a:accent4><a:srgbClr val="DB2777"/></a:accent4><a:accent5><a:srgbClr val="7C3AED"/></a:accent5><a:accent6><a:srgbClr val="64748B"/></a:accent6><a:hlink><a:srgbClr val="2563EB"/></a:hlink><a:folHlink><a:srgbClr val="7C3AED"/></a:folHlink></a:clrScheme>
    <a:fontScheme name="IO Board"><a:majorFont><a:latin typeface="Arial"/><a:ea typeface="Microsoft YaHei"/></a:majorFont><a:minorFont><a:latin typeface="Arial"/><a:ea typeface="Microsoft YaHei"/></a:minorFont></a:fontScheme>
    <a:fmtScheme name="IO Board"><a:fillStyleLst><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:fillStyleLst><a:lnStyleLst><a:ln w="9525"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln></a:lnStyleLst><a:effectStyleLst><a:effectStyle><a:effectLst/></a:effectStyle></a:effectStyleLst><a:bgFillStyleLst><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:bgFillStyleLst></a:fmtScheme>
  </a:themeElements>
</a:theme>"""


def create_pptx(out_path):
    slides = build_slides()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out_path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", content_types(len(slides)))
        z.writestr("_rels/.rels", root_rels())
        z.writestr("ppt/presentation.xml", presentation_xml(len(slides)))
        z.writestr("ppt/_rels/presentation.xml.rels", presentation_rels(len(slides)))
        z.writestr("ppt/slideMasters/slideMaster1.xml", simple_master())
        z.writestr("ppt/slideMasters/_rels/slideMaster1.xml.rels", """<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout" Target="../slideLayouts/slideLayout1.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme" Target="../theme/theme1.xml"/></Relationships>""")
        z.writestr("ppt/slideLayouts/slideLayout1.xml", simple_layout())
        z.writestr("ppt/slideLayouts/_rels/slideLayout1.xml.rels", """<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster" Target="../slideMasters/slideMaster1.xml"/></Relationships>""")
        z.writestr("ppt/theme/theme1.xml", simple_theme())
        for i, (title, subtitle, body) in enumerate(slides, 1):
            z.writestr(f"ppt/slides/slide{i}.xml", slide_xml(title, subtitle, body))
            z.writestr(f"ppt/slides/_rels/slide{i}.xml.rels", rels_xml())


if __name__ == "__main__":
    create_pptx(Path("IO_BOARD/docs/wifi_mas_network_architecture.pptx"))
