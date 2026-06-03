#!/usr/bin/env python3
import argparse
import datetime
import html
import os
import time


def _history_status(debug_dir):
    history_path = os.path.join(debug_dir, "ir_capture_history.jsonl")
    if not os.path.exists(history_path):
        return 0, None, None

    try:
        mtime = os.path.getmtime(history_path)
        with open(history_path, "r", encoding="utf-8") as f:
            lines = [line for line in f if line.strip()]
        return len(lines), mtime, lines[-1].strip() if lines else None
    except OSError:
        return 0, None, None


def _waiting_html(sample_count, idle_seconds, last_line):
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    last_text = last_line if last_line else "No saved frame yet."
    return """<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta http-equiv="refresh" content="1">
  <title>IR RX Waiting</title>
  <style>
    body {
      margin: 0;
      background: #eef2f7;
      color: #172033;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }
    .wrap { padding: 16px; }
    .panel {
      background: white;
      border: 1px solid #d0d5dd;
      border-radius: 6px;
      padding: 16px;
      margin-bottom: 12px;
    }
    .status {
      display: inline-block;
      width: 12px;
      height: 12px;
      border-radius: 50%%;
      background: #ef4444;
      margin-right: 8px;
      animation: pulse 1s infinite;
      vertical-align: -1px;
    }
    @keyframes pulse {
      0%%, 100%% { opacity: 0.2; transform: scale(0.8); }
      50%% { opacity: 1; transform: scale(1.15); }
    }
    h1 { font-size: 22px; margin: 0 0 10px; }
    p { margin: 6px 0; }
    pre {
      margin: 8px 0 0;
      white-space: pre-wrap;
      word-break: break-word;
      font: 12px ui-monospace, SFMono-Regular, Menlo, monospace;
      color: #475467;
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="panel">
      <h1><span class="status"></span>等待红外信号，请发射一组</h1>
      <p>PA6 已经 %u 秒没有接收到完整红外帧。</p>
      <p>页面已自动清空并闪烁刷新，表示系统仍在等待。请现在发射一组红外信号；接收到后页面会提示“信号已接收，处理中”，随后显示并保存最新一组数据。</p>
      <p>后台已保存样本数：%u</p>
      <p>等待页刷新时间：%s</p>
    </div>
    <div class="panel">
      <p>上一组已保存记录：</p>
      <pre>%s</pre>
    </div>
  </div>
</body>
</html>
""" % (
        idle_seconds,
        sample_count,
        html.escape(now),
        html.escape(last_text),
    )


def _write_waiting_page(debug_dir, stale_seconds):
    html_path = os.path.join(debug_dir, "ir_waveform_live.html")
    sample_count, mtime, last_line = _history_status(debug_dir)
    if mtime is None:
        idle_seconds = stale_seconds
    else:
        idle_seconds = int(time.time() - mtime)

    tmp_path = html_path + ".tmp"
    with open(tmp_path, "w", encoding="utf-8") as f:
        f.write(_waiting_html(sample_count, idle_seconds, last_line))
    os.replace(tmp_path, html_path)


def run(debug_dir, stale_seconds, interval_seconds):
    while True:
        _once(debug_dir, stale_seconds)
        time.sleep(interval_seconds)


def _once(debug_dir, stale_seconds):
    _sample_count, mtime, _last_line = _history_status(debug_dir)
    if mtime is None or (time.time() - mtime) >= stale_seconds:
        _write_waiting_page(debug_dir, stale_seconds)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--debug-dir", default=os.path.dirname(os.path.abspath(__file__)))
    parser.add_argument("--stale-seconds", type=int, default=60)
    parser.add_argument("--interval-seconds", type=int, default=5)
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args()

    if args.once:
        _once(args.debug_dir, args.stale_seconds)
    else:
        run(args.debug_dir, args.stale_seconds, args.interval_seconds)


if __name__ == "__main__":
    main()
