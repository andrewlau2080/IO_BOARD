import argparse
import datetime
import html
import json
import os
import re
from collections import Counter, defaultdict
from statistics import median


def _fmt_hex(value, width=2):
    return "0x%0*X" % (width, value)


def _in_range(value, low, high):
    return low <= value <= high


def _segment_rows(durations, start_level):
    rows = []
    elapsed = 0
    level = start_level
    for index, duration in enumerate(durations):
        rows.append({
            "index": index,
            "level": level,
            "voltage": "3.3V" if level else "0V",
            "start_us": elapsed,
            "duration_us": duration,
        })
        elapsed += duration
        level ^= 1
    return rows


def _decode_nec_at(rows, start_index):
    if start_index + 67 > len(rows):
        return None

    header_mark = rows[start_index]
    header_space = rows[start_index + 1]
    active_low = header_mark["level"] == 0 and header_space["level"] == 1
    active_high = header_mark["level"] == 1 and header_space["level"] == 0
    if not active_low and not active_high:
        return None

    if not _in_range(header_mark["duration_us"], 8000, 10000):
        return None
    if not _in_range(header_space["duration_us"], 3800, 5200):
        return None

    bits = []
    bit_rows = []
    seg_index = start_index + 2
    for bit_index in range(32):
        mark = rows[seg_index]
        space = rows[seg_index + 1]
        if mark["level"] == space["level"]:
            return None
        if not _in_range(mark["duration_us"], 350, 800):
            return None

        if _in_range(space["duration_us"], 350, 900):
            bit = 0
        elif _in_range(space["duration_us"], 1200, 2100):
            bit = 1
        else:
            return None

        byte_index = bit_index // 8
        bit_rows.append({
            "bit_index": bit_index,
            "byte_index": byte_index,
            "bit_in_byte": bit_index % 8,
            "value": bit,
            "mark_seg": seg_index,
            "space_seg": seg_index + 1,
            "mark_us": mark["duration_us"],
            "space_us": space["duration_us"],
            "start_us": mark["start_us"],
        })
        bits.append(bit)
        seg_index += 2

    values = []
    for byte_index in range(4):
        value = 0
        for bit in range(8):
            value |= bits[byte_index * 8 + bit] << bit
        values.append(value)

    address_ok = ((values[0] ^ values[1]) & 0xFF) == 0xFF
    command_ok = ((values[2] ^ values[3]) & 0xFF) == 0xFF
    extended_address = values[0] | (values[1] << 8)

    return {
        "protocol": "NEC / NEC-like pulse-distance",
        "start_index": start_index,
        "end_index": seg_index - 1,
        "active": "active-low demodulated output" if active_low else "active-high/inverted",
        "header_mark_us": header_mark["duration_us"],
        "header_space_us": header_space["duration_us"],
        "bits": bits,
        "bit_rows": bit_rows,
        "bytes": values,
        "address_ok": address_ok,
        "command_ok": command_ok,
        "extended_address": extended_address,
    }


def _find_nec_decode(rows):
    for index in range(0, max(0, len(rows) - 66)):
        decoded = _decode_nec_at(rows, index)
        if decoded:
            return decoded
    return None


def _rebase_rows(rows, start_index):
    if start_index <= 0:
        return rows
    base_us = rows[start_index]["start_us"]
    rebased = []
    for new_index, row in enumerate(rows[start_index:]):
        item = dict(row)
        item["index"] = new_index
        item["start_us"] = row["start_us"] - base_us
        rebased.append(item)
    return rebased


def _find_nec_sync_start(rows):
    for index in range(0, max(0, len(rows) - 1)):
        mark = rows[index]
        space = rows[index + 1]
        if mark["level"] != 0 or space["level"] != 1:
            continue
        if _in_range(mark["duration_us"], 8000, 10000) and _in_range(space["duration_us"], 3800, 5200):
            return index
    return None


def _display_rows_from_sync(rows, decoded):
    if decoded:
        rows = _rebase_rows(rows, decoded["start_index"])
        return rows, _find_nec_decode(rows)

    sync_index = _find_nec_sync_start(rows)
    if sync_index is not None:
        rows = _rebase_rows(rows, sync_index)
        return rows, _find_nec_decode(rows)

    return rows, decoded


def _nec_key(decoded):
    if not decoded:
        return None
    return tuple(decoded["bytes"])


def _nec_normalized_signal(decoded):
    if not decoded:
        return None
    durations = [9000, 4500]
    for bit in decoded["bits"]:
        durations.append(560)
        durations.append(1690 if bit else 560)
    durations.append(560)
    return 0, durations


def _make_c_array(name_prefix, start_level, durations, note):
    values = []
    for i in range(0, len(durations), 12):
        values.append("  " + ", ".join(str(x) for x in durations[i:i + 12]))
    return """/*
 * %s
 * Unit: microseconds. start_level 0 means IR carrier mark first for the current transmitter.
 */
#include <stdint.h>

static const uint8_t %s_start_level = %u;
static const uint16_t %s_count = %u;
static const uint16_t %s_signal_us[%u] = {
%s
};
""" % (
        note,
        name_prefix,
        start_level,
        name_prefix,
        len(durations),
        name_prefix,
        len(durations),
        ",\n".join(values),
    )


def _append_history(out_dir, durations, start_level, frame, edges, timeouts, decoded):
    history_path = os.path.join(out_dir, "ir_capture_history.jsonl")
    record = {
        "captured_at": datetime.datetime.now().isoformat(timespec="seconds"),
        "frame": frame,
        "edges": edges,
        "timeouts": timeouts,
        "start_level": start_level,
        "count": len(durations),
        "total_us": sum(durations),
        "durations": durations,
        "nec_bytes": list(decoded["bytes"]) if decoded else None,
        "nec_address_ok": bool(decoded["address_ok"]) if decoded else None,
        "nec_command_ok": bool(decoded["command_ok"]) if decoded else None,
    }
    with open(history_path, "a", encoding="utf-8") as f:
        f.write(json.dumps(record, ensure_ascii=True) + "\n")
    return history_path


def _load_history(out_dir, limit=80):
    history_path = os.path.join(out_dir, "ir_capture_history.jsonl")
    if not os.path.exists(history_path):
        return []
    records = []
    with open(history_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                records.append(json.loads(line))
            except json.JSONDecodeError:
                continue
    return records[-limit:]


def _raw_median_signal(records):
    usable = [r for r in records if r.get("durations") and r.get("start_level") in (0, 1)]
    if not usable:
        return None
    counts = Counter((r["start_level"], r["count"]) for r in usable)
    (start_level, count), _seen = counts.most_common(1)[0]
    matching = [r for r in usable if r["start_level"] == start_level and r["count"] == count]
    if len(matching) < 2:
        latest = usable[-1]
        return latest["start_level"], latest["durations"], 1, "latest raw frame"
    durations = []
    for i in range(count):
        durations.append(int(round(median(r["durations"][i] for r in matching))))
    return start_level, durations, len(matching), "median raw frame"


def _stats_from_history(records):
    decoded_records = [r for r in records if r.get("nec_bytes")]
    if not decoded_records:
        raw = _raw_median_signal(records)
        return {
            "mode": "raw",
            "records": records,
            "chosen_key": None,
            "chosen_records": [],
            "normalized": raw,
        }

    counts = Counter(tuple(r["nec_bytes"]) for r in decoded_records)
    chosen_key, _seen = counts.most_common(1)[0]
    chosen = [r for r in decoded_records if tuple(r["nec_bytes"]) == chosen_key]
    bits = []
    for byte in chosen_key:
        for bit in range(8):
            bits.append((byte >> bit) & 1)
    normalized = _nec_normalized_signal({"bits": bits})
    return {
        "mode": "nec",
        "records": records,
        "chosen_key": chosen_key,
        "chosen_records": chosen,
        "normalized": (normalized[0], normalized[1], len(chosen), "normalized NEC timing"),
    }


def _write_normalized_archive(out_dir, stats):
    normalized = stats.get("normalized")
    if not normalized:
        return None
    start_level, durations, sample_count, source = normalized
    if stats["mode"] == "nec":
        b = stats["chosen_key"]
        note = (
            "Normalized NEC capture from %u matching frames. "
            "ADDR=%s ~ADDR=%s CMD=%s ~CMD=%s"
            % (sample_count, _fmt_hex(b[0]), _fmt_hex(b[1]), _fmt_hex(b[2]), _fmt_hex(b[3]))
        )
    else:
        note = "Median raw capture from %u matching frames. Use only if protocol decoder is not available." % sample_count
    archive = _make_c_array("archived_ir_normalized", start_level, durations, note)
    path = os.path.join(out_dir, "ir_code_normalized.c")
    with open(path, "w", encoding="utf-8") as f:
        f.write(archive)
    return path


def _write_firmware_source_if_stable(out_dir, stats, min_samples=3):
    normalized = stats.get("normalized")
    if not normalized or stats.get("mode") != "nec":
        return None

    start_level, durations, sample_count, _source = normalized
    if sample_count < min_samples or len(durations) > 256:
        return None

    b = stats["chosen_key"]
    if ((b[0] ^ b[1]) & 0xFF) != 0xFF or ((b[2] ^ b[3]) & 0xFF) != 0xFF:
        return None

    values = []
    for i in range(0, len(durations), 12):
        values.append("  " + ", ".join(str(x) + "U" for x in durations[i:i + 12]))

    source = """#include "ir_learned_code.h"

/*
 * Auto-generated from IR capture statistics.
 * Samples: %u matching NEC frames
 * ADDR=%s ~ADDR=%s CMD=%s ~CMD=%s
 */
const uint8_t g_ir_learned_code_available = 1U;
const uint8_t g_ir_learned_start_level = %uU;
const uint16_t g_ir_learned_count = %uU;
const uint16_t g_ir_learned_signal_us[IR_CAPTURE_MAX_EDGES] = {
%s
};

uint16_t ir_load_learned_signal(ir_raw_signal_t *signal)
{
  uint16_t i;

  if(signal == 0 || g_ir_learned_code_available == 0U) {
    return 0U;
  }

  signal->start_level = g_ir_learned_start_level;
  signal->count = g_ir_learned_count;

  for(i = 0; i < IR_CAPTURE_MAX_EDGES; i++) {
    signal->duration_us[i] = (i < g_ir_learned_count) ? g_ir_learned_signal_us[i] : 0U;
  }

  return signal->count;
}
""" % (
        sample_count,
        _fmt_hex(b[0]),
        _fmt_hex(b[1]),
        _fmt_hex(b[2]),
        _fmt_hex(b[3]),
        start_level,
        len(durations),
        ",\n".join(values),
    )

    project_root = os.path.dirname(out_dir)
    src_dir = os.path.join(project_root, "src")
    if not os.path.isdir(src_dir):
        return None
    path = os.path.join(src_dir, "ir_learned_code.c")
    with open(path, "w", encoding="utf-8") as f:
        f.write(source)
    return path


def _find_sync_candidates(rows):
    candidates = []
    for i in range(len(rows) - 1):
        mark = rows[i]
        space = rows[i + 1]
        if mark["level"] != space["level"] and mark["duration_us"] >= 2500 and space["duration_us"] >= 1000:
            candidates.append({
                "index": i,
                "start_us": mark["start_us"],
                "mark_level": mark["level"],
                "mark_us": mark["duration_us"],
                "space_us": space["duration_us"],
            })
    return candidates[:12]


def _build_annotations(rows, decoded):
    annotations = {}
    classes = {}
    if not decoded:
        for candidate in _find_sync_candidates(rows):
            annotations[candidate["index"]] = "sync candidate"
            annotations[candidate["index"] + 1] = "sync space"
            classes[candidate["index"]] = "sync"
            classes[candidate["index"] + 1] = "sync"
        return annotations, classes

    start = decoded["start_index"]
    annotations[start] = "SYNC mark"
    annotations[start + 1] = "SYNC space"
    classes[start] = "sync"
    classes[start + 1] = "sync"

    names = ["ADDR", "~ADDR", "CMD", "~CMD"]
    for bit in decoded["bit_rows"]:
        group = names[bit["byte_index"]]
        label = "%s b%u=%u" % (group, bit["bit_in_byte"], bit["value"])
        annotations[bit["mark_seg"]] = "%s mark" % label
        annotations[bit["space_seg"]] = label
        classes[bit["mark_seg"]] = "mark"
        classes[bit["space_seg"]] = "one" if bit["value"] else "zero"
    return annotations, classes


def _generate_svg(rows, start_level, frame, edges, timeouts, decoded):
    total_us = sum(row["duration_us"] for row in rows)
    width = 1400
    height = 480
    left = 84
    right = 32
    low_y = 330
    high_y = 160
    plot_width = width - left - right
    total = total_us or 1
    annotations, classes = _build_annotations(rows, decoded)

    path_parts = []
    label_parts = []
    band_parts = []
    first_y = high_y if start_level else low_y
    path_parts.append("M%.2f,%.2f" % (left, first_y))

    for row in rows:
        x1 = left + plot_width * (row["start_us"] / total)
        x2 = left + plot_width * ((row["start_us"] + row["duration_us"]) / total)
        y = high_y if row["level"] else low_y
        path_parts.append("L%.2f,%.2f" % (x2, y))
        next_y = low_y if row["level"] else high_y
        path_parts.append("L%.2f,%.2f" % (x2, next_y))

        css_class = classes.get(row["index"], "raw")
        if css_class != "raw":
            band_parts.append(
                '<rect x="%.2f" y="120" width="%.2f" height="230" class="band %s"/>'
                % (x1, max(1.0, x2 - x1), css_class)
            )

        if x2 - x1 >= 42:
            text = "%uus" % row["duration_us"]
            if row["index"] in annotations:
                text = "%s %s" % (annotations[row["index"]], text)
            label_parts.append(
                '<text x="%.1f" y="%.1f" class="seg" text-anchor="middle">%s</text>'
                % ((x1 + x2) / 2, y - 14 if row["level"] else y + 28, html.escape(text))
            )

    tick_parts = []
    for i in range(11):
        t = total_us * i / 10
        tx = left + plot_width * i / 10
        tick_parts.append('<line x1="%.1f" y1="%u" x2="%.1f" y2="%u" class="axis"/>' % (tx, low_y + 22, tx, low_y + 34))
        tick_parts.append('<text x="%.1f" y="%u" text-anchor="middle" class="tick">%.2fms</text>' % (tx, low_y + 58, t / 1000))

    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    decoded_summary = "decoder: waiting for valid NEC-like header"
    if decoded:
        b = decoded["bytes"]
        decoded_summary = (
            "decoder: %s | address=%s ~address=%s %s | command=%s ~command=%s %s"
            % (
                decoded["protocol"],
                _fmt_hex(b[0]),
                _fmt_hex(b[1]),
                "OK" if decoded["address_ok"] else "MISMATCH",
                _fmt_hex(b[2]),
                _fmt_hex(b[3]),
                "OK" if decoded["command_ok"] else "MISMATCH",
            )
        )

    return '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %u %u" preserveAspectRatio="xMidYMid meet">
  <style>
    .bg { fill: #f7f8fb; }
    .title { font: 700 28px -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; fill: #172033; }
    .meta { font: 16px -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; fill: #344054; }
    .axis { stroke: #98a2b3; stroke-width: 1; }
    .grid { stroke: #d0d5dd; stroke-width: 1; stroke-dasharray: 4 6; }
    .wave { fill: none; stroke: #0057d9; stroke-width: 4; stroke-linejoin: round; stroke-linecap: round; }
    .band { opacity: 0.22; }
    .sync { fill: #f59e0b; }
    .mark { fill: #64748b; }
    .zero { fill: #22c55e; }
    .one { fill: #ef4444; }
    .raw { fill: transparent; }
    .seg { font: 12px ui-monospace, SFMono-Regular, Menlo, monospace; fill: #0f172a; }
    .tick { font: 13px ui-monospace, SFMono-Regular, Menlo, monospace; fill: #475467; }
    .label { font: 15px -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; fill: #172033; }
  </style>
  <rect class="bg" width="%u" height="%u"/>
  <text x="40" y="48" class="title">IR RX PA6 Protocol View</text>
  <text x="40" y="82" class="meta">Updated %s | frame=%s | edges=%s | timeouts=%s | start_level=%u | count=%u | total=%uus (%.3fms)</text>
  <text x="40" y="108" class="meta">%s</text>
  <text x="18" y="%u" class="label">3.3V</text>
  <text x="30" y="%u" class="label">0V</text>
  <line x1="%u" y1="%u" x2="%u" y2="%u" class="grid"/>
  <line x1="%u" y1="%u" x2="%u" y2="%u" class="grid"/>
  %s
  <line x1="%u" y1="%u" x2="%u" y2="%u" class="axis"/>
  %s
  <path class="wave" d="%s"/>
  %s
  <text x="40" y="440" class="meta">Color: orange=sync/header, gray=carrier mark, green=bit 0 space, red=bit 1 space.</text>
</svg>''' % (
        width, height,
        width, height,
        html.escape(now), html.escape(str(frame)), html.escape(str(edges)), html.escape(str(timeouts)),
        start_level, len(rows), total_us, total_us / 1000,
        html.escape(decoded_summary),
        high_y + 5, low_y + 5,
        left, high_y, width - right, high_y,
        left, low_y, width - right, low_y,
        "".join(band_parts),
        left, low_y + 22, width - right, low_y + 22,
        "".join(tick_parts),
        " ".join(path_parts),
        "".join(label_parts),
    )


def _decoder_panel(decoded, rows):
    if not decoded:
        candidates = _find_sync_candidates(rows)
        if not candidates:
            return '<div class="panel warn"><h2>Decoder</h2><p>No NEC-like 9ms/4.5ms sync header found yet. The raw segment table below is still valid.</p></div>'
        parts = []
        for c in candidates:
            parts.append(
                "<tr><td>%u</td><td>%u</td><td>%u</td><td>%u</td><td>%u</td></tr>"
                % (c["index"], c["start_us"], c["mark_level"], c["mark_us"], c["space_us"])
            )
        return '''<div class="panel warn"><h2>Decoder</h2>
<p>No full NEC-like frame was decoded. Possible sync candidates are listed here; check wiring, signal polarity, and whether the transmitter is sending a standard pulse-distance code.</p>
<table><thead><tr><th>segment</th><th>start_us</th><th>mark_level</th><th>mark_us</th><th>space_us</th></tr></thead><tbody>%s</tbody></table></div>''' % "".join(parts)

    b = decoded["bytes"]
    bit_rows = []
    names = ["ADDR", "~ADDR", "CMD", "~CMD"]
    for bit in decoded["bit_rows"]:
        bit_rows.append(
            "<tr><td>%u</td><td>%s</td><td>%u</td><td>%u</td><td>%u</td><td>%u</td><td>%u</td></tr>"
            % (
                bit["bit_index"],
                names[bit["byte_index"]],
                bit["bit_in_byte"],
                bit["value"],
                bit["mark_us"],
                bit["space_us"],
                bit["start_us"],
            )
        )

    summary_rows = [
        ("Sync mark", "%uus" % decoded["header_mark_us"], "Expected about 9000us"),
        ("Sync space", "%uus" % decoded["header_space_us"], "Expected about 4500us"),
        ("Address", _fmt_hex(b[0]), "First data byte"),
        ("Address inverse/check", _fmt_hex(b[1]), "OK" if decoded["address_ok"] else "MISMATCH"),
        ("Command", _fmt_hex(b[2]), "Second data byte"),
        ("Command inverse/check", _fmt_hex(b[3]), "OK" if decoded["command_ok"] else "MISMATCH"),
        ("Extended address view", _fmt_hex(decoded["extended_address"], 4), "Use this if address inverse is not expected"),
    ]
    summary = "".join("<tr><td>%s</td><td>%s</td><td>%s</td></tr>" % tuple(html.escape(str(x)) for x in row) for row in summary_rows)
    return '''<div class="panel ok"><h2>Decoded NEC-like Frame</h2>
<table><thead><tr><th>part</th><th>value</th><th>check</th></tr></thead><tbody>%s</tbody></table>
<h2>Bit Timing</h2>
<div class="tablebox"><table><thead><tr><th>bit</th><th>group</th><th>bit_in_byte</th><th>value</th><th>mark_us</th><th>space_us</th><th>start_us</th></tr></thead><tbody>%s</tbody></table></div>
</div>''' % (summary, "".join(bit_rows))


def _stats_panel(stats):
    records = stats.get("records", [])
    normalized = stats.get("normalized")
    if not records:
        return '<div class="panel warn"><h2>Capture Statistics</h2><p>No history has been collected yet.</p></div>'

    if stats["mode"] == "nec" and stats.get("chosen_key"):
        b = stats["chosen_key"]
        sample_count = normalized[2] if normalized else 0
        if sample_count >= 3:
            firmware_status = "This stable code is eligible to be written into src/ir_learned_code.c."
        else:
            firmware_status = "Send the same key at least %u more time(s) before writing it into firmware." % (3 - sample_count)
        summary = (
            "Chosen stable NEC group from %u matching frames: ADDR=%s, ~ADDR=%s, CMD=%s, ~CMD=%s. %s"
            % (sample_count, _fmt_hex(b[0]), _fmt_hex(b[1]), _fmt_hex(b[2]), _fmt_hex(b[3]), firmware_status)
        )
    elif normalized:
        summary = (
            "No full NEC decode yet. A median raw candidate was generated from %u matching frames."
            % normalized[2]
        )
    else:
        summary = "No stable candidate yet. Send the same IR command several times."

    return '''<div class="panel ok"><h2>只显示一组已选数据</h2>
<p>信号已接收，正在统计/解码并已保存当前选定组。%s 历史样本只在后台用于准确性判断，本页面只显示一组数据；选定数组写入 <code>debug/ir_code_normalized.c</code>。</p>
</div>''' % html.escape(summary)


def _generate_html(svg, rows, decoded, c_array, normalized_c_array, stats, source_note):
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    raw_rows = []
    annotations, _classes = _build_annotations(rows, decoded)
    for row in rows:
        raw_rows.append(
            "<tr><td>%u</td><td>%u</td><td>%s</td><td>%u</td><td>%u</td><td>%s</td></tr>"
            % (
                row["index"],
                row["level"],
                row["voltage"],
                row["start_us"],
                row["duration_us"],
                html.escape(annotations.get(row["index"], "")),
            )
        )

    return '''<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta http-equiv="refresh" content="1">
  <title>IR RX Live Protocol View</title>
  <style>
    body { margin: 0; background: #eef2f7; color: #172033; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; }
    .wrap { padding: 12px; max-width: 100vw; box-sizing: border-box; }
    .panel { background: white; border: 1px solid #d0d5dd; border-radius: 6px; padding: 12px; margin-bottom: 12px; }
    .svgbox { overflow: hidden; }
    .svgbox svg { display: block; width: 100%%; height: auto; max-height: calc(100vh - 96px); }
    .ok { border-color: #86efac; }
    .warn { border-color: #facc15; }
    .notice { border-color: #60a5fa; background: #eff6ff; }
    h2 { font-size: 16px; margin: 0 0 8px; }
    p { margin: 0 0 10px; }
    pre { overflow-x: auto; margin: 0; font: 13px ui-monospace, SFMono-Regular, Menlo, monospace; white-space: pre-wrap; }
    table { border-collapse: collapse; width: 100%%; font: 13px ui-monospace, SFMono-Regular, Menlo, monospace; }
    th, td { border-bottom: 1px solid #e4e7ec; padding: 4px 8px; text-align: left; white-space: nowrap; }
    .tablebox { max-height: 360px; overflow: auto; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="panel notice"><strong>已接收到红外信号，信号处理中并已保存。</strong><br>页面每秒自动刷新；当前只显示最新/选定的一组。Updated by GDB at %s. Source: %s.</div>
    <div class="panel svgbox">%s</div>
    %s
    %s
    <div class="panel"><h2>Raw Segments</h2><div class="tablebox"><table><thead><tr><th>idx</th><th>level</th><th>voltage</th><th>start_us</th><th>duration_us</th><th>decoded_label</th></tr></thead><tbody>%s</tbody></table></div></div>
    <div class="panel"><h2>Selected C Array For Firmware</h2><pre>%s</pre></div>
  </div>
</body>
</html>
''' % (
        html.escape(now),
        html.escape(source_note),
        svg,
        _decoder_panel(decoded, rows),
        _stats_panel(stats),
        "".join(raw_rows),
        html.escape(normalized_c_array if normalized_c_array else c_array),
    )


def render_ir_capture(durations, start_level, frame="n/a", edges="n/a", timeouts="n/a", source_note="live target RAM"):
    rows = _segment_rows(durations, start_level)
    decoded = _find_nec_decode(rows)
    display_rows, display_decoded = _display_rows_from_sync(rows, decoded)
    display_start_level = display_rows[0]["level"] if display_rows else start_level
    svg = _generate_svg(display_rows, display_start_level, frame, edges, timeouts, display_decoded)
    c_array = "static const uint8_t archived_ir_start_level = %u;\nstatic const uint16_t archived_ir_signal_us[%u] = {\n  %s\n};\n" % (
        start_level,
        len(durations),
        ", ".join(str(x) for x in durations),
    )
    stats = {"records": [], "mode": "raw", "normalized": (start_level, durations, 1, "latest raw frame")}
    normalized_c_array = _make_c_array(
        "archived_ir_normalized",
        start_level,
        durations,
        "Latest raw capture, not yet statistically verified.",
    )
    html_doc = _generate_html(svg, display_rows, display_decoded, c_array, normalized_c_array, stats, source_note)
    return svg, html_doc, c_array


def write_ir_capture(out_dir, durations, start_level, frame="n/a", edges="n/a", timeouts="n/a", source_note="live target RAM"):
    os.makedirs(out_dir, exist_ok=True)
    rows = _segment_rows(durations, start_level)
    decoded = _find_nec_decode(rows)
    _append_history(out_dir, durations, start_level, frame, edges, timeouts, decoded)
    history = _load_history(out_dir)
    stats = _stats_from_history(history)
    normalized_path = _write_normalized_archive(out_dir, stats)
    _write_firmware_source_if_stable(out_dir, stats)

    display_rows, display_decoded = _display_rows_from_sync(rows, decoded)
    display_start_level = display_rows[0]["level"] if display_rows else start_level
    svg = _generate_svg(display_rows, display_start_level, frame, edges, timeouts, display_decoded)
    c_array = "static const uint8_t archived_ir_start_level = %u;\nstatic const uint16_t archived_ir_signal_us[%u] = {\n  %s\n};\n" % (
        start_level,
        len(durations),
        ", ".join(str(x) for x in durations),
    )
    normalized_c_array = ""
    if normalized_path and os.path.exists(normalized_path):
        with open(normalized_path, "r", encoding="utf-8") as f:
            normalized_c_array = f.read()
    html_doc = _generate_html(svg, display_rows, display_decoded, c_array, normalized_c_array, stats, source_note)
    with open(os.path.join(out_dir, "ir_waveform_live.svg"), "w", encoding="utf-8") as f:
        f.write(svg)
    with open(os.path.join(out_dir, "ir_waveform_live.html"), "w", encoding="utf-8") as f:
        f.write(html_doc)
    with open(os.path.join(out_dir, "ir_code_latest.c"), "w", encoding="utf-8") as f:
        f.write(c_array)
    return sum(durations)


def _read_c_archive(path):
    text = open(path, "r", encoding="utf-8").read()
    start = int(re.search(r"archived_ir_start_level\s*=\s*(\d+)", text).group(1))
    body = re.search(r"archived_ir_signal_us\[[^\]]+\]\s*=\s*\{(.*?)\};", text, re.S).group(1)
    durations = [int(value) for value in re.findall(r"\d+", body)]
    return start, durations


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--from-c", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--frame", default="archived")
    args = parser.parse_args()
    start_level, durations = _read_c_archive(args.from_c)
    total = write_ir_capture(
        args.out_dir,
        durations,
        start_level,
        frame=args.frame,
        source_note=os.path.basename(args.from_c),
    )
    print("IR view updated: count=%u total_us=%u out_dir=%s" % (len(durations), total, args.out_dir))


if __name__ == "__main__":
    main()
