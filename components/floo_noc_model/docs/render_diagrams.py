#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Render các khối mermaid trong một file markdown thành PNG.

Chỉ cần chạy lại khi sửa sơ đồ trong markdown; PNG kết quả được commit nên
`make_docx.py` chạy được offline.

    npm install mermaid@11            # một lần, ở đâu cũng được
    python3 render_diagrams.py \\
        components/floo_noc_model/docs/NOC_MODEL_ARCHITECTURE.vi.md \\
        components/floo_noc_model/docs/diagrams

Cách làm: mermaid bản ESM được phục vụ qua 127.0.0.1 để module import cùng
origin, google-chrome headless render, rồi chụp đúng kích thước viewBox. Không
dùng rasteriser SVG ngoài: `<style>` mermaid sinh ra là CSS hợp lệ nhưng XML
parser chặt chẽ của rsvg từ chối, trong khi trình duyệt vốn đã render đúng nó.
"""

from __future__ import annotations

import argparse
import functools
import html
import http.server
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import threading

ESM_REL = "node_modules/mermaid/dist/mermaid.esm.min.mjs"

PAGE = """<!doctype html>
<html><head><meta charset="utf-8"><title>pending</title>
<style>html,body{{margin:0;padding:16px;background:#fff}}</style>
</head><body>
<pre id="svgout"></pre>
<script type="module">
  import mermaid from '/{esm}';
  mermaid.initialize({{
    startOnLoad: false,
    theme: 'base',
    fontFamily: 'DejaVu Sans, Arial, sans-serif',
    themeVariables: {{
      background: '#ffffff',
      primaryColor: '#f7efe9',
      primaryBorderColor: '#a84a18',
      primaryTextColor: '#171c24',
      lineColor: '#6b7683',
      secondaryColor: '#e4eff0',
      tertiaryColor: '#f2f4f7',
      clusterBkg: '#f7f9fb',
      clusterBorder: '#c6cfda',
      fontSize: '15px'
    }}
  }});
  try {{
    const res = await mermaid.render('g0', {source});
    document.getElementById('svgout').textContent = res.svg;
    document.title = 'done';
  }} catch (e) {{
    document.getElementById('svgout').textContent = 'ERROR ' + e;
    document.title = 'error';
  }}
</script>
</body></html>
"""


def js_string(text: str) -> str:
    return ('"' + text.replace("\\", "\\\\").replace('"', '\\"')
            .replace("\n", "\\n") + '"')


def extract(markdown: pathlib.Path) -> list[str]:
    blocks: list[str] = []
    inside = False
    buffer: list[str] = []
    for line in markdown.read_text(encoding="utf-8").splitlines():
        token = line.strip()
        if token.startswith("```"):
            if inside:
                blocks.append("\n".join(buffer))
                buffer, inside = [], False
            elif token[3:].strip().lower() == "mermaid":
                inside = True
            continue
        if inside:
            buffer.append(line)
    return blocks


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    """Giữ output renderer ở mức kết quả hình, không in từng request ESM."""

    def log_message(self, _format: str, *args) -> None:
        del args


def find_mermaid_root(explicit: pathlib.Path | None) -> pathlib.Path:
    candidates: list[pathlib.Path] = []
    if explicit:
        candidates.append(explicit)
    if os.environ.get("MERMAID_ROOT"):
        candidates.append(pathlib.Path(os.environ["MERMAID_ROOT"]))
    candidates.append(pathlib.Path.cwd())
    for root in candidates:
        if (root / ESM_REL).is_file():
            return root.resolve()
    sys.exit("không tìm thấy mermaid ESM. Cài bằng:\n"
             "    npm install mermaid@11\n"
             "rồi chạy lại từ thư mục đó, hoặc truyền --mermaid-root DIR")


def find_browser() -> str:
    for name in ("google-chrome", "chromium", "chromium-browser"):
        found = shutil.which(name)
        if found:
            return found
    sys.exit("không tìm thấy google-chrome/chromium")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("markdown", type=pathlib.Path)
    parser.add_argument("outdir", type=pathlib.Path)
    parser.add_argument("--mermaid-root", type=pathlib.Path,
                        help="thư mục chứa node_modules/mermaid")
    args = parser.parse_args()

    root = find_mermaid_root(args.mermaid_root)
    browser = find_browser()
    outdir = args.outdir.resolve()
    outdir.mkdir(parents=True, exist_ok=True)

    blocks = extract(args.markdown)
    print(f"{len(blocks)} khối mermaid, mermaid tại {root}")

    handler = functools.partial(QuietHandler,
                                directory=str(root))
    httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    port = httpd.server_address[1]
    failures = 0

    try:
        with tempfile.TemporaryDirectory(dir=root, prefix=".mmd-") as work:
            stage = pathlib.Path(work)
            slug = stage.name
            for index, source in enumerate(blocks, start=1):
                page = stage / f"d{index}.html"
                page.write_text(
                    PAGE.format(esm=ESM_REL, source=js_string(source)),
                    encoding="utf-8")
                dom = subprocess.run(
                    [browser, "--headless=new", "--disable-gpu",
                     "--no-sandbox", "--hide-scrollbars",
                     "--virtual-time-budget=25000", "--dump-dom",
                     f"http://127.0.0.1:{port}/{slug}/d{index}.html"],
                    check=False, capture_output=True, text=True).stdout

                match = re.search(r'<pre id="svgout">([\s\S]*?)</pre>', dom)
                svg = html.unescape(match.group(1)) if match else ""
                if not svg.lstrip().startswith("<svg"):
                    failures += 1
                    print(f"  {index}: không render được — "
                          f"{svg[:200] or 'trống'}")
                    continue

                # sequenceDiagram phát viewBox gốc âm, flowchart phát "0 0 w h".
                box = re.search(
                    r'viewBox="[-\d.]+ [-\d.]+ ([\d.]+) ([\d.]+)"', svg)
                if not box:
                    failures += 1
                    print(f"  {index}: không đọc được viewBox")
                    continue
                width = int(float(box.group(1)))
                height = int(float(box.group(2)))
                scale = max(1.0, min(2.0, 1500 / width))

                shot = stage / f"s{index}.html"
                shot.write_text(
                    "<!doctype html><meta charset='utf-8'>"
                    "<style>html,body{margin:0;background:#fff}"
                    f"svg{{width:{width}px;height:{height}px;display:block}}"
                    "</style>" + svg, encoding="utf-8")

                png = outdir / f"diagram-{index}.png"
                subprocess.run(
                    [browser, "--headless=new", "--disable-gpu",
                     "--no-sandbox", "--hide-scrollbars",
                     f"--force-device-scale-factor={scale:.2f}",
                     f"--window-size={width},{height}",
                     f"--screenshot={png}",
                     f"http://127.0.0.1:{port}/{slug}/s{index}.html"],
                    check=False, capture_output=True)
                if not png.is_file():
                    failures += 1
                    print(f"  {index}: chrome không chụp được")
                    continue
                print(f"  diagram-{index}.png  {width}x{height} "
                      f"@{scale:.2f}x  {png.stat().st_size:,} bytes")
    finally:
        httpd.shutdown()

    if failures:
        print(f"{failures} sơ đồ lỗi", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
