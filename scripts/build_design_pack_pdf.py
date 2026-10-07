#!/usr/bin/env python3
"""Render the SHOGUN design pack markdown into one PDF.

The PDF is the six documents, in order. It is not a summary.
"""

from __future__ import annotations

import html
import re
import sys
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.platypus import (
    CondPageBreak,
    HRFlowable,
    ListFlowable,
    ListItem,
    Paragraph,
    Preformatted,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [
    "README.md",
    "METHODOLOGY.md",
    "SCHEMATICS.md",
    "BUILD_GUIDE.md",
    "TESTPLAN.md",
    "REPO_SETUP.md",
]
OUT = ROOT / "docs" / "SHOGUN_Design_Pack.pdf"

BANNED = ("\u2014", "\u2013", "What it is not", "What this is not", "What it is NOT")


def check_sources(texts: list[tuple[str, str]]) -> None:
    bad = False
    for name, text in texts:
        for i, line in enumerate(text.splitlines(), 1):
            for token in BANNED:
                if token in line:
                    print(f"{name}:{i}: banned token {token!r}", file=sys.stderr)
                    bad = True
    if bad:
        raise SystemExit(1)


def markdown_links(text: str) -> str:
    def linksub(match: re.Match) -> str:
        label, url = match.group(1), match.group(2)
        if url.startswith("http"):
            if label.strip() == url.strip() or label in url:
                return url
            return f"{label} ({url})"
        return label

    return re.sub(r"\[([^\]]+)\]\(([^)]+)\)", linksub, text)


def inline(text: str) -> str:
    """Markdown inline marks. Code spans are taken out before italics.

    A multiplication star inside backticks must not open an italic run.
    """
    text = markdown_links(text)
    codes: list[str] = []

    def hold_code(match: re.Match) -> str:
        codes.append(match.group(1))
        return f"\x00C{len(codes) - 1}\x00"

    text = re.sub(r"`([^`]+)`", hold_code, text)
    escaped = html.escape(text, quote=False)
    escaped = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", escaped)
    # Italics only when the stars sit against the words. A spaced star is multiplication.
    escaped = re.sub(r"(?<!\*)\*(?!\s)(.+?)(?<!\s)\*(?!\*)", r"<i>\1</i>", escaped)

    def restore(match: re.Match) -> str:
        raw = html.escape(codes[int(match.group(1))], quote=False)
        return f'<font face="Courier" size="8">{raw}</font>'

    return re.sub(r"\x00C(\d+)\x00", restore, escaped)


def make_styles():
    base = getSampleStyleSheet()
    styles = {
        "h1": ParagraphStyle(
            "H1",
            parent=base["Heading1"],
            fontName="Times-Bold",
            fontSize=16,
            leading=20,
            spaceBefore=12,
            spaceAfter=8,
            textColor=colors.black,
            keepWithNext=True,
        ),
        "h2": ParagraphStyle(
            "H2",
            parent=base["Heading2"],
            fontName="Times-Bold",
            fontSize=13,
            leading=16,
            spaceBefore=12,
            spaceAfter=6,
            textColor=colors.black,
            keepWithNext=True,
        ),
        "h3": ParagraphStyle(
            "H3",
            parent=base["Heading3"],
            fontName="Times-Bold",
            fontSize=11,
            leading=14,
            spaceBefore=10,
            spaceAfter=4,
            textColor=colors.black,
            keepWithNext=True,
        ),
        "body": ParagraphStyle(
            "Body",
            parent=base["BodyText"],
            fontName="Times-Roman",
            fontSize=10,
            leading=13,
            alignment=TA_LEFT,
            spaceAfter=6,
        ),
        "bullet": ParagraphStyle(
            "BulletBody",
            parent=base["BodyText"],
            fontName="Times-Roman",
            fontSize=10,
            leading=13,
            leftIndent=0,
            spaceAfter=1,
        ),
        "cell": ParagraphStyle(
            "Cell",
            parent=base["BodyText"],
            fontName="Times-Roman",
            fontSize=8,
            leading=10,
        ),
        "cellhead": ParagraphStyle(
            "CellHead",
            parent=base["BodyText"],
            fontName="Times-Bold",
            fontSize=8,
            leading=10,
        ),
        "code": ParagraphStyle(
            "Code",
            fontName="Courier",
            fontSize=8,
            leading=10,
            leftIndent=6,
            rightIndent=6,
            spaceBefore=2,
            spaceAfter=8,
        ),
    }
    return styles


def parse_table(lines: list[str], styles) -> Table:
    rows = []
    for line in lines:
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        rows.append(cells)
    cleaned = []
    for r in rows:
        if r and all(re.fullmatch(r":?-{3,}:?", c.replace(" ", "")) for c in r):
            continue
        cleaned.append(r)
    if not cleaned:
        raise SystemExit("empty table")
    width = 7.1 * inch
    ncol = max(len(r) for r in cleaned)
    # Give the last column the leftover when the first columns are short labels.
    if ncol == 2:
        col_w = [1.7 * inch, width - 1.7 * inch]
    elif ncol == 3:
        col_w = [1.5 * inch, 2.2 * inch, width - 3.7 * inch]
    else:
        col_w = [width / ncol] * ncol
    flow = []
    for ri, row in enumerate(cleaned):
        style = styles["cellhead"] if ri == 0 else styles["cell"]
        padded = row + [""] * (ncol - len(row))
        flow.append([Paragraph(inline(c), style) for c in padded])
    table = Table(flow, colWidths=col_w, repeatRows=1)
    table.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.Color(0.93, 0.93, 0.93)),
        ("GRID", (0, 0), (-1, -1), 0.3, colors.Color(0.6, 0.6, 0.6)),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 3),
        ("RIGHTPADDING", (0, 0), (-1, -1), 3),
        ("TOPPADDING", (0, 0), (-1, -1), 2),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 2),
    ]))
    return table


def blocks(text: str, styles) -> list:
    lines = text.splitlines()
    out = []
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        if line.strip() == "":
            i += 1
            continue
        if line.startswith("```"):
            i += 1
            buf = []
            while i < n and not lines[i].startswith("```"):
                buf.append(lines[i])
                i += 1
            i += 1  # closing fence
            out.append(Preformatted("\n".join(buf), styles["code"]))
            continue
        if line.startswith("|"):
            buf = []
            while i < n and lines[i].startswith("|"):
                buf.append(lines[i])
                i += 1
            out.append(parse_table(buf, styles))
            out.append(Spacer(1, 8))
            continue
        if line.startswith("#"):
            level = len(line) - len(line.lstrip("#"))
            title = line[level:].strip()
            key = {1: "h1", 2: "h2", 3: "h3"}.get(level, "h3")
            out.append(Paragraph(inline(title), styles[key]))
            i += 1
            continue
        if line.startswith("- "):
            items = []
            while i < n and lines[i].startswith("- "):
                items.append(ListItem(Paragraph(inline(lines[i][2:].strip()), styles["bullet"])))
                i += 1
            out.append(ListFlowable(
                items,
                bulletType="bullet",
                leftIndent=16,
                bulletFontName="Times-Roman",
                bulletFontSize=10,
                spaceAfter=6,
            ))
            continue
        if re.match(r"\d+\. ", line.strip()):
            out.append(Paragraph(inline(line.strip()), styles["body"]))
            i += 1
            continue
        buf = [line.strip()]
        i += 1
        while i < n and lines[i].strip() and not lines[i].startswith(("#", "-", "|", "```")) and not re.match(r"\d+\. ", lines[i].strip()):
            buf.append(lines[i].strip())
            i += 1
        out.append(Paragraph(inline(" ".join(buf)), styles["body"]))
    return out


def footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("Times-Roman", 8)
    canvas.setFillColor(colors.Color(0.25, 0.25, 0.25))
    canvas.drawString(0.7 * inch, 0.42 * inch, "SHOGUN design pack, revision 2026-10-06")
    canvas.drawRightString(letter[0] - 0.7 * inch, 0.42 * inch, str(doc.page))
    canvas.restoreState()


def main() -> None:
    texts = []
    for name in SOURCES:
        path = ROOT / name
        texts.append((name, path.read_text()))
    check_sources(texts)
    styles = make_styles()
    story = []
    for idx, (name, text) in enumerate(texts):
        if idx:
            story.append(CondPageBreak(1.2 * inch))
            story.append(HRFlowable(width="100%", thickness=0.4, color=colors.Color(0.7, 0.7, 0.7), spaceAfter=8))
        story.extend(blocks(text, styles))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    doc = SimpleDocTemplate(
        str(OUT),
        pagesize=letter,
        leftMargin=0.7 * inch,
        rightMargin=0.7 * inch,
        topMargin=0.7 * inch,
        bottomMargin=0.65 * inch,
        title="SHOGUN design pack",
        author="Martial Systems LLC",
        subject="Stand-in design for the SHOGUN fan instrument, revision 2026-10-06",
    )
    doc.build(story, onFirstPage=footer, onLaterPages=footer)
    print(OUT)


if __name__ == "__main__":
    main()
