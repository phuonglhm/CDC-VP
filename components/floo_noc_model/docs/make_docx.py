#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Sinh bản .docx từ NOC_MODEL_ARCHITECTURE.vi.md.

Bản markdown là canonical. Script này tồn tại để bản Word tái tạo được bằng một
lệnh thay vì sửa tay — sửa tay là cách chắc chắn nhất để hai bản lệch nhau.

    python3 components/floo_noc_model/docs/make_docx.py

Sơ đồ mermaid được nhúng từ `docs/diagrams/diagram-N.png`, đánh số theo thứ tự
xuất hiện trong markdown. Các PNG đó được commit sẵn nên lệnh trên chạy offline.
Khi sửa một sơ đồ trong markdown thì render lại trước:

    python3 components/floo_noc_model/docs/render_diagrams.py \\
        components/floo_noc_model/docs/NOC_MODEL_ARCHITECTURE.vi.md \\
        components/floo_noc_model/docs/diagrams

Nếu thiếu PNG, script in nguyên mã nguồn mermaid dưới dạng khối monospace kèm
nhãn — nội dung không bao giờ bị mất im lặng, chỉ là kém đẹp.

Chỉ hỗ trợ đúng tập cú pháp markdown mà tài liệu đó dùng. Đây là bộ chuyển đổi
cho một file cụ thể, không phải markdown converter tổng quát.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

try:
    import docx
    from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
    from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK
    from docx.oxml import OxmlElement
    from docx.oxml.ns import qn
    from docx.shared import Inches, Pt, RGBColor
except ImportError:  # pragma: no cover
    sys.exit("cần python-docx: python3 -m pip install --user python-docx")


BODY_FONT = "Calibri"
MONO_FONT = "Consolas"
HEAD_FONT = "Cambria"

INK = RGBColor(0x17, 0x1C, 0x24)
MUTED = RGBColor(0x6B, 0x76, 0x83)
ACCENT = RGBColor(0xA8, 0x4A, 0x18)
CODE_SHADE = "EAEEF3"
HEAD_SHADE = "DFE5EC"


# ---------------------------------------------------------------- low level


def set_font(run, name: str) -> None:
    """Đặt font cho cả ba slot, nếu không Word có thể thay font khác cho
    ký tự có dấu tiếng Việt."""
    run.font.name = name
    rpr = run._element.get_or_add_rPr()
    fonts = rpr.find(qn("w:rFonts"))
    if fonts is None:
        fonts = OxmlElement("w:rFonts")
        rpr.append(fonts)
    for slot in ("w:ascii", "w:hAnsi", "w:cs", "w:eastAsia"):
        fonts.set(qn(slot), name)


def shade(target, fill: str) -> None:
    """Tô nền cho một paragraph hoặc một ô bảng."""
    if hasattr(target, "get_or_add_tcPr"):        # phần tử w:tc
        pr = target.get_or_add_tcPr()
    else:                                          # đối tượng Paragraph
        pr = target._element.get_or_add_pPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), fill)
    pr.append(shd)


def keep_together(paragraph) -> None:
    pr = paragraph._element.get_or_add_pPr()
    for tag in ("w:keepNext", "w:keepLines"):
        el = OxmlElement(tag)
        el.set(qn("w:val"), "true")
        pr.append(el)


def add_field(paragraph, instruction: str, placeholder: str = "") -> None:
    """Chèn field Word (TOC, PAGE...) thay vì hardcode giá trị dễ stale."""
    run = paragraph.add_run()
    begin = OxmlElement("w:fldChar")
    begin.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = instruction
    separate = OxmlElement("w:fldChar")
    separate.set(qn("w:fldCharType"), "separate")
    run._r.extend((begin, instr, separate))
    if placeholder:
        text_run = paragraph.add_run(placeholder)
        set_font(text_run, BODY_FONT)
        text_run.font.size = Pt(9)
        text_run.font.color.rgb = MUTED
    end_run = paragraph.add_run()
    end = OxmlElement("w:fldChar")
    end.set(qn("w:fldCharType"), "end")
    end_run._r.append(end)


def prevent_row_split(row) -> None:
    tr_pr = row._tr.get_or_add_trPr()
    cant_split = OxmlElement("w:cantSplit")
    tr_pr.append(cant_split)


# ---------------------------------------------------------------- inline


INLINE = re.compile(r"(\*\*.+?\*\*|`[^`]+`|\*[^*\s][^*]*?\*)", re.DOTALL)


def add_inline(paragraph, text: str, base_size: float = 10.5,
               base_font: str = BODY_FONT) -> None:
    """Chia một đoạn thành run theo **đậm**, `mã` và *nghiêng*."""
    for piece in INLINE.split(text):
        if not piece:
            continue
        if piece.startswith("**") and piece.endswith("**"):
            run = paragraph.add_run(piece[2:-2])
            run.bold = True
            set_font(run, base_font)
            run.font.size = Pt(base_size)
        elif piece.startswith("`") and piece.endswith("`"):
            run = paragraph.add_run(piece[1:-1])
            set_font(run, MONO_FONT)
            run.font.size = Pt(base_size - 1)
            run.font.color.rgb = ACCENT
        elif piece.startswith("*") and piece.endswith("*"):
            run = paragraph.add_run(piece[1:-1])
            run.italic = True
            set_font(run, base_font)
            run.font.size = Pt(base_size)
        else:
            run = paragraph.add_run(piece)
            set_font(run, base_font)
            run.font.size = Pt(base_size)


# ---------------------------------------------------------------- blocks


class Builder:
    def __init__(self, document, images: pathlib.Path | None):
        self.doc = document
        self.images = images
        self.diagram_index = 0
        self.table_index = 0
        self.embedded = 0
        self.fallback = 0
        self.last_heading = ""
        self.rule_index = 0

    def heading(self, level: int, text: str) -> None:
        # Markdown H1 là document title; H2/H3/H4 map sang Heading 1/2/3 để
        # Navigation Pane, accessibility tree và TOC của Word hoạt động.
        style = "Title" if level == 1 else f"Heading {min(level - 1, 3)}"
        para = self.doc.add_paragraph(style=style)
        para.paragraph_format.space_before = Pt(16 if level > 1 else 22)
        para.paragraph_format.space_after = Pt(5)
        keep_together(para)
        sizes = {1: 20, 2: 15, 3: 12.5, 4: 11}
        display_text = text.replace("`", "")
        run = para.add_run(display_text)
        run.bold = True
        set_font(run, HEAD_FONT)
        run.font.size = Pt(sizes.get(level, 11))
        run.font.color.rgb = ACCENT if level <= 2 else INK
        if level > 1:
            self.last_heading = display_text

    def paragraph(self, text: str) -> None:
        para = self.doc.add_paragraph()
        para.paragraph_format.space_after = Pt(7)
        add_inline(para, text)

    def bullet(self, text: str) -> None:
        para = self.doc.add_paragraph(style="List Bullet")
        para.paragraph_format.space_after = Pt(3)
        add_inline(para, text)

    def numbered(self, text: str) -> None:
        para = self.doc.add_paragraph(style="List Number")
        para.paragraph_format.space_after = Pt(3)
        add_inline(para, text)

    def code(self, lines: list[str], label: str | None = None) -> None:
        if label:
            cap = self.doc.add_paragraph()
            cap.paragraph_format.space_after = Pt(2)
            keep_together(cap)
            run = cap.add_run(label)
            set_font(run, BODY_FONT)
            run.font.size = Pt(8.5)
            run.font.color.rgb = MUTED
            run.italic = True
        para = self.doc.add_paragraph()
        para.paragraph_format.space_before = Pt(4)
        para.paragraph_format.space_after = Pt(10)
        para.paragraph_format.left_indent = Inches(0.16)
        shade(para, CODE_SHADE)
        for index, line in enumerate(lines):
            if index:
                para.add_run().add_break()
            run = para.add_run(line)
            set_font(run, MONO_FONT)
            run.font.size = Pt(8.5)

    def diagram(self, lines: list[str]) -> None:
        self.diagram_index += 1
        picture = None
        if self.images is not None:
            candidate = self.images / f"diagram-{self.diagram_index}.png"
            if candidate.is_file():
                picture = candidate
        if picture is None:
            self.fallback += 1
            self.code(lines, label=f"Sơ đồ {self.diagram_index} — mã nguồn "
                                   "Mermaid (chưa render được thành ảnh)")
            return
        self.embedded += 1
        para = self.doc.add_paragraph()
        para.alignment = WD_ALIGN_PARAGRAPH.CENTER
        para.paragraph_format.space_before = Pt(6)
        para.paragraph_format.space_after = Pt(3)
        keep_together(para)

        # Vừa khổ giấy theo cả hai chiều. Sơ đồ pipeline dựng dọc rất cao và
        # hẹp; ép theo bề rộng thôi sẽ cho ra ảnh cao hơn cả trang.
        max_width, max_height = 6.3, 7.6
        try:
            from PIL import Image
            with Image.open(picture) as image:
                pixel_w, pixel_h = image.size
            ratio = pixel_h / pixel_w
            width = min(max_width, max_height / ratio)
        except Exception:
            width = max_width
        shape = para.add_run().add_picture(str(picture), width=Inches(width))
        caption_text = f"Hình {self.diagram_index} — {self.last_heading}"
        shape._inline.docPr.set("descr", caption_text)
        shape._inline.docPr.set("title", caption_text)

        caption = self.doc.add_paragraph(style="Caption")
        caption.alignment = WD_ALIGN_PARAGRAPH.CENTER
        caption.paragraph_format.space_after = Pt(10)
        run = caption.add_run(caption_text)
        set_font(run, BODY_FONT)
        run.font.size = Pt(9)
        run.font.color.rgb = MUTED

    def table(self, rows: list[list[str]]) -> None:
        header, body = rows[0], rows[1:]
        # Bảng metadata đầu tài liệu cố ý có header trống và không đánh số.
        numbered = any(cell.strip() for cell in header)
        if numbered:
            self.table_index += 1
            caption = self.doc.add_paragraph(style="Caption")
            caption.paragraph_format.space_before = Pt(5)
            caption.paragraph_format.space_after = Pt(3)
            keep_together(caption)
            run = caption.add_run(
                f"Bảng {self.table_index} — {self.last_heading}")
            set_font(run, BODY_FONT)
            run.font.size = Pt(9)
            run.font.color.rgb = MUTED

        table = self.doc.add_table(rows=1, cols=len(header))
        table.style = "Table Grid"
        table.alignment = WD_TABLE_ALIGNMENT.LEFT
        table.autofit = True

        for cell, text in zip(table.rows[0].cells, header):
            cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
            shade(cell._element, HEAD_SHADE)
            para = cell.paragraphs[0]
            para.paragraph_format.space_after = Pt(1)
            run = para.add_run(text)
            run.bold = True
            set_font(run, BODY_FONT)
            run.font.size = Pt(9)
        table.rows[0]._element.get_or_add_trPr().append(
            OxmlElement("w:tblHeader"))
        prevent_row_split(table.rows[0])

        for values in body:
            cells = table.add_row().cells
            for cell, text in zip(cells, values):
                cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.TOP
                para = cell.paragraphs[0]
                para.paragraph_format.space_after = Pt(1)
                add_inline(para, text, base_size=9)
            prevent_row_split(table.rows[-1])

        spacer = self.doc.add_paragraph()
        spacer.paragraph_format.space_after = Pt(9)

    def rule(self) -> None:
        self.rule_index += 1
        page = self.doc.add_paragraph()
        page.add_run().add_break(WD_BREAK.PAGE)

        # Rule đầu tiên nằm giữa executive summary và section 1: đặt TOC ở
        # đây. Word/LibreOffice sẽ update field khi mở tài liệu.
        if self.rule_index == 1:
            heading = self.doc.add_paragraph()
            heading.paragraph_format.space_after = Pt(8)
            run = heading.add_run("Mục lục")
            run.bold = True
            set_font(run, HEAD_FONT)
            run.font.size = Pt(16)
            run.font.color.rgb = ACCENT
            toc = self.doc.add_paragraph()
            add_field(toc, r'TOC \o "1-3" \h \z \u',
                      "Cập nhật field để hiển thị mục lục")
            toc.add_run().add_break(WD_BREAK.PAGE)


# ---------------------------------------------------------------- parser


TABLE_SEP = re.compile(r"^\s*\|[\s:|-]+\|\s*$")


def split_row(line: str) -> list[str]:
    return [cell.strip() for cell in line.strip().strip("|").split("|")]


def convert(source: pathlib.Path, target: pathlib.Path,
            images: pathlib.Path | None) -> None:
    lines = source.read_text(encoding="utf-8").splitlines()

    # Lấy revision mới nhất từ bảng lịch sử thay vì hardcode header Word. Một
    # revision mới trong Markdown vì thế không thể để mọi trang mang version cũ.
    revision = "unversioned"
    revision_row = re.compile(r"^\|\s*(v\d+(?:\.\d+)*)\s*\|")
    for line in lines:
        match = revision_row.match(line)
        if match:
            revision = match.group(1)

    document = docx.Document()
    section = document.sections[0]
    section.page_width = Inches(8.27)
    section.page_height = Inches(11.69)
    for attr in ("left_margin", "right_margin"):
        setattr(section, attr, Inches(0.85))
    for attr in ("top_margin", "bottom_margin"):
        setattr(section, attr, Inches(0.8))

    document.core_properties.title = (
        "FlooNoC SystemC/TLM Model — Tài liệu kiến trúc")
    document.core_properties.subject = (
        "Kiến trúc, RTL mapping, verification boundary và design findings")
    document.core_properties.author = "CDC-VP FlooNoC model team"
    document.core_properties.keywords = (
        "FlooNoC, SystemC, TLM, NoC, RTL, CDC-VP")

    # Yêu cầu ứng dụng Word update TOC/PAGE fields lúc mở file.
    settings = document.settings._element
    update = settings.find(qn("w:updateFields"))
    if update is None:
        update = OxmlElement("w:updateFields")
        settings.append(update)
    update.set(qn("w:val"), "true")

    normal = document.styles["Normal"]
    normal.font.name = BODY_FONT
    normal.font.size = Pt(10.5)
    normal.paragraph_format.space_after = Pt(7)
    normal.paragraph_format.line_spacing = 1.12

    title_style = document.styles["Title"]
    title_style.font.name = HEAD_FONT
    title_style.font.size = Pt(20)
    title_style.font.bold = True
    title_style.font.color.rgb = ACCENT
    for level, size in ((1, 15), (2, 12.5), (3, 11)):
        style = document.styles[f"Heading {level}"]
        style.font.name = HEAD_FONT
        style.font.size = Pt(size)
        style.font.bold = True
        style.font.color.rgb = ACCENT if level == 1 else INK
        style.paragraph_format.keep_with_next = True
        style.paragraph_format.keep_together = True

    header = section.header.paragraphs[0]
    header.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    hrun = header.add_run(
        f"FlooNoC SystemC/TLM Model — Architecture {revision}")
    set_font(hrun, BODY_FONT)
    hrun.font.size = Pt(8)
    hrun.font.color.rgb = MUTED

    footer = section.footer.paragraphs[0]
    footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
    frun = footer.add_run("CDC-VP  •  FlooNoC 9a6972a  •  Trang ")
    set_font(frun, BODY_FONT)
    frun.font.size = Pt(8)
    frun.font.color.rgb = MUTED
    add_field(footer, "PAGE", "1")

    build = Builder(document, images)
    index = 0
    total = len(lines)

    while index < total:
        line = lines[index]
        stripped = line.strip()

        if not stripped:
            index += 1
            continue

        if stripped.startswith("```"):
            language = stripped[3:].strip().lower()
            index += 1
            block: list[str] = []
            while index < total and not lines[index].strip().startswith("```"):
                block.append(lines[index])
                index += 1
            index += 1
            if language == "mermaid":
                build.diagram(block)
            else:
                build.code(block)
            continue

        if stripped.startswith("#"):
            level = len(stripped) - len(stripped.lstrip("#"))
            build.heading(level, stripped[level:].strip())
            index += 1
            continue

        if stripped == "---":
            build.rule()
            index += 1
            continue

        if stripped.startswith("|") and index + 1 < total \
                and TABLE_SEP.match(lines[index + 1]):
            rows = [split_row(stripped)]
            index += 2
            while index < total and lines[index].strip().startswith("|"):
                rows.append(split_row(lines[index]))
                index += 1
            build.table(rows)
            continue

        if stripped.startswith("- "):
            text = stripped[2:]
            index += 1
            # Bullet nối dòng: dòng thụt lề tiếp theo thuộc về bullet này.
            while index < total and lines[index].startswith("  ") \
                    and lines[index].strip() \
                    and not lines[index].strip().startswith("- "):
                text += " " + lines[index].strip()
                index += 1
            build.bullet(text)
            continue

        ordered = re.match(r"^\d+[.)]\s+(.*)$", stripped)
        if ordered:
            text = ordered.group(1)
            index += 1
            while index < total and lines[index].startswith("  ") \
                    and lines[index].strip() \
                    and not re.match(r"^\s*\d+[.)]\s+", lines[index]):
                text += " " + lines[index].strip()
                index += 1
            build.numbered(text)
            continue

        if stripped.startswith("> "):
            build.paragraph(stripped[2:])
            index += 1
            continue

        text = stripped
        index += 1
        while index < total and lines[index].strip() \
                and not lines[index].strip().startswith(("#", "|", "- ", "```",
                                                         ">", "---")) \
                and not re.match(r"^\d+[.)]\s+", lines[index].strip()):
            text += " " + lines[index].strip()
            index += 1
        build.paragraph(text)

    target.parent.mkdir(parents=True, exist_ok=True)
    document.save(target)

    print(f"ghi: {target}")
    print(f"  sơ đồ nhúng ảnh: {build.embedded}")
    print(f"  sơ đồ dạng mã nguồn: {build.fallback}")
    if build.fallback:
        print("  (chạy lại với --images DIR chứa diagram-N.png để nhúng ảnh)")


def main() -> int:
    here = pathlib.Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source",
                        type=pathlib.Path,
                        default=here / "NOC_MODEL_ARCHITECTURE.vi.md")
    parser.add_argument("--output",
                        type=pathlib.Path,
                        default=here / "NOC_MODEL_ARCHITECTURE.vi.docx")
    parser.add_argument("--images", type=pathlib.Path,
                        default=here / "diagrams",
                        help="thư mục chứa diagram-N.png đã render")
    args = parser.parse_args()

    if not args.source.is_file():
        print(f"không thấy nguồn: {args.source}", file=sys.stderr)
        return 1
    convert(args.source, args.output, args.images)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
