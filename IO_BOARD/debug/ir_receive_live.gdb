set pagination off
set confirm off
set architecture arm
target extended-remote localhost:3333
monitor halt
set breakpoint auto-hw on
hbreak ir_debug_frame_ready

python
import datetime
import html
import os
import struct
import sys
import gdb

def _u(expr):
    return int(gdb.parse_and_eval(expr))

def _generate_svg(durations, start_level, frame, edges, timeouts):
    total_us = sum(durations)
    width = 1700
    height = 430
    left = 80
    right = 40
    top = 120
    low_y = 310
    high_y = 170
    plot_width = width - left - right
    total = total_us or 1

    path = []
    x = left
    level = start_level
    y = high_y if level else low_y
    path.append(f"M{x:.2f},{y:.2f}")
    elapsed = 0

    labels = []
    for i, duration in enumerate(durations):
        x1 = left + plot_width * (elapsed / total)
        x2 = left + plot_width * ((elapsed + duration) / total)
        y = high_y if level else low_y
        path.append(f"L{x2:.2f},{y:.2f}")

        segment_width = x2 - x1
        if segment_width >= 36 and duration >= 150:
            labels.append(
                f'<text x="{(x1 + x2) / 2:.1f}" y="{y - 12 if level else y + 26:.1f}" '
                f'class="seg" text-anchor="middle">{duration}us</text>'
            )

        level ^= 1
        y2 = high_y if level else low_y
        path.append(f"L{x2:.2f},{y2:.2f}")
        elapsed += duration

    tick_parts = []
    for i in range(11):
        t = total_us * i / 10
        tx = left + plot_width * i / 10
        tick_parts.append(f'<line x1="{tx:.1f}" y1="{low_y + 20}" x2="{tx:.1f}" y2="{low_y + 30}" class="axis"/>')
        tick_parts.append(f'<text x="{tx:.1f}" y="{low_y + 52}" text-anchor="middle" class="tick">{t / 1000:.2f}ms</text>')

    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
  <style>
    .bg {{ fill: #f7f8fb; }}
    .title {{ font: 700 28px -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; fill: #172033; }}
    .meta {{ font: 16px -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; fill: #344054; }}
    .axis {{ stroke: #98a2b3; stroke-width: 1; }}
    .grid {{ stroke: #d0d5dd; stroke-width: 1; stroke-dasharray: 4 6; }}
    .wave {{ fill: none; stroke: #0057d9; stroke-width: 4; stroke-linejoin: round; stroke-linecap: round; }}
    .seg {{ font: 12px ui-monospace, SFMono-Regular, Menlo, monospace; fill: #0f172a; }}
    .tick {{ font: 13px ui-monospace, SFMono-Regular, Menlo, monospace; fill: #475467; }}
    .label {{ font: 15px -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; fill: #172033; }}
    .box {{ fill: white; stroke: #d0d5dd; stroke-width: 1; rx: 6; }}
    .tbl {{ font: 12px ui-monospace, SFMono-Regular, Menlo, monospace; fill: #1d2939; }}
  </style>
  <rect class="bg" width="{width}" height="{height}"/>
  <text x="40" y="48" class="title">IR RX PA6 Captured Waveform</text>
  <text x="40" y="82" class="meta">Updated {html.escape(now)} | frame={frame} | edges={edges} | timeouts={timeouts} | start_level={start_level} | count={len(durations)} | total={total_us}us ({total_us / 1000:.3f}ms)</text>
  <text x="18" y="{high_y + 5}" class="label">3.3V</text>
  <text x="30" y="{low_y + 5}" class="label">0V</text>
  <line x1="{left}" y1="{high_y}" x2="{width - right}" y2="{high_y}" class="grid"/>
  <line x1="{left}" y1="{low_y}" x2="{width - right}" y2="{low_y}" class="grid"/>
  <line x1="{left}" y1="{low_y + 20}" x2="{width - right}" y2="{low_y + 20}" class="axis"/>
  {''.join(tick_parts)}
  <path class="wave" d="{' '.join(path)}"/>
  {''.join(labels)}
  <rect x="40" y="382" width="{width - 80}" height="1" class="axis"/>
  <text x="40" y="410" class="meta">Use the timing table below the waveform for exact PA6 levels, voltages, start times, and durations.</text>
</svg>
'''

def _build_rows(durations, start_level):
    rows = []
    elapsed = 0
    level = start_level
    for i, duration in enumerate(durations):
        voltage = "3.3V" if level else "0V"
        rows.append((i, level, voltage, elapsed, duration))
        elapsed += duration
        level ^= 1
    return rows

def _generate_html(svg, c_array, rows):
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    table_rows = []
    for i, level, voltage, start_us, duration in rows:
        table_rows.append(
            f'<tr><td>{i}</td><td>{level}</td><td>{voltage}</td>'
            f'<td>{start_us}</td><td>{duration}</td></tr>'
        )
    return '''<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta http-equiv="refresh" content="1">
  <title>IR RX Live Waveform</title>
  <style>
    body { margin: 0; background: #eef2f7; color: #172033; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; }
    .wrap { padding: 16px; }
    .panel { background: white; border: 1px solid #d0d5dd; border-radius: 6px; padding: 12px; margin-bottom: 12px; }
    .svgbox { overflow-x: auto; }
    pre { overflow-x: auto; margin: 0; font: 13px ui-monospace, SFMono-Regular, Menlo, monospace; white-space: pre-wrap; }
    table { border-collapse: collapse; width: 100%; font: 13px ui-monospace, SFMono-Regular, Menlo, monospace; }
    th, td { border-bottom: 1px solid #e4e7ec; padding: 4px 8px; text-align: left; }
    .tablebox { max-height: 300px; overflow: auto; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="panel">Auto-refreshing every 1 second over HTTP. Updated by GDB at ''' + html.escape(now) + '''.</div>
    <div class="panel svgbox">''' + svg + '''</div>
    <div class="panel tablebox">
      <table>
        <thead><tr><th>idx</th><th>level</th><th>voltage</th><th>start_us</th><th>duration_us</th></tr></thead>
        <tbody>''' + "".join(table_rows) + '''</tbody>
      </table>
    </div>
    <div class="panel"><pre>''' + html.escape(c_array) + '''</pre></div>
  </div>
</body>
</html>
'''

def _write_live_files():
    count = _u("(unsigned short)g_ir_code_count")
    count = max(0, min(count, 256))
    start_level = _u("(unsigned char)g_ir_code_start_level")
    frame = _u("(unsigned int)g_ir_frame_counter")
    edges = _u("(unsigned int)g_ir_rx_edge_counter")
    timeouts = _u("(unsigned int)g_ir_capture_timeout_counter")
    addr = _u("(unsigned int)&g_ir_code_table_us")
    raw = gdb.selected_inferior().read_memory(addr, count * 2)
    durations = list(struct.unpack("<" + "H" * count, raw)) if count else []

    roots = [os.getcwd()]
    mac_shared = "/media/psf/Home/qtprj/ARTERY/IO_BOARD"
    if os.path.isdir(mac_shared):
        roots.append(mac_shared)

    debug_dir = os.path.join(os.getcwd(), "debug")
    if debug_dir not in sys.path:
        sys.path.insert(0, debug_dir)
    from ir_render import write_ir_capture

    written = []
    for root in roots:
        out_dir = os.path.join(root, "debug")
        html_path = os.path.join(out_dir, "ir_waveform_live.html")
        write_ir_capture(
            out_dir,
            durations,
            start_level,
            frame=frame,
            edges=edges,
            timeouts=timeouts,
            source_note="live target RAM",
        )
        written.append(html_path)

    print("IR SVG updated: frame=%u count=%u total_us=%u" % (frame, len(durations), sum(durations)))
    for path in written:
        print("  " + path)

gdb.execute("set $ir_live_ready = 1")
end

commands
silent
python _write_live_files()
continue
end

continue
