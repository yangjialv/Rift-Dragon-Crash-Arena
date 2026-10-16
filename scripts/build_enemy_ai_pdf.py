"""Render the Chinese enemy-AI Markdown guide to a searchable, font-embedded PDF.

Run with reportlab installed; output is a generated document, not source editing.
"""
import argparse
import html
import re
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate, Frame, PageTemplate, Paragraph, Spacer, PageBreak,
    Table, TableStyle, Preformatted, KeepTogether,
)
from reportlab.platypus.tableofcontents import TableOfContents


def markup(value):
    value = html.escape(value)
    value = re.sub(r"\*\*(.*?)\*\*", r"<b>\1</b>", value)
    value = re.sub(r"`(.*?)`", r"\1", value)
    return value


class GuideDocument(BaseDocTemplate):
    def afterFlowable(self, flowable):
        if isinstance(flowable, Paragraph) and flowable.style.name == "Section":
            title = flowable.getPlainText()
            key = "section-{}".format(self.seq.nextf("heading"))
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(title, key, 0)
            self.notify("TOCEntry", (0, title, self.page, key))


def build(source, output, font_path):
    pdfmetrics.registerFont(TTFont("AIChinese", str(font_path)))
    pdfmetrics.registerFontFamily("AIChinese", normal="AIChinese", bold="AIChinese")
    ink = colors.HexColor("#17334b")
    green = colors.HexColor("#007e78")
    styles = getSampleStyleSheet()
    body = ParagraphStyle("BodyCJK", fontName="AIChinese", fontSize=10,
                          leading=16, wordWrap="CJK", spaceAfter=7)
    heading = ParagraphStyle("Section", parent=body, fontSize=16, leading=23,
                             textColor=ink, spaceBefore=15, spaceAfter=10, keepWithNext=True)
    subheading = ParagraphStyle("Subsection", parent=body, fontSize=12, leading=19,
                                textColor=green, spaceBefore=9, keepWithNext=True)
    title = ParagraphStyle("Cover", parent=body, fontSize=27, leading=37,
                           alignment=TA_CENTER, textColor=ink, spaceAfter=20)
    subtitle = ParagraphStyle("Subtitle", parent=body, fontSize=20, leading=29,
                              alignment=TA_CENTER, textColor=green)
    cell = ParagraphStyle("Cell", parent=body, fontSize=8.5, leading=13, spaceAfter=0)
    code = ParagraphStyle("Code", parent=body, fontSize=9, leading=14,
                          backColor=colors.HexColor("#f0f5f7"), borderPadding=8)

    output.parent.mkdir(parents=True, exist_ok=True)
    doc = GuideDocument(str(output), pagesize=A4, leftMargin=43, rightMargin=43,
                        topMargin=48, bottomMargin=43, title="RDCA 敌人 AI 系统详解",
                        author="Rift Dragon Crash Arena 项目技术文档")

    def page_footer(canvas, document):
        canvas.saveState()
        canvas.setFont("AIChinese", 8)
        canvas.setFillColor(colors.HexColor("#657887"))
        canvas.drawString(43, A4[1] - 27, "RDCA / 敌人 AI 系统详解 / 重构前技术说明")
        canvas.drawRightString(A4[0] - 43, 23, "{}".format(document.page))
        canvas.restoreState()

    frame = Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="main")
    doc.addPageTemplates(PageTemplate(id="guide", frames=frame, onPage=page_footer))
    story = [Spacer(1, 100), Paragraph("Rift Dragon<br/>Crash Arena", title),
             Paragraph("敌人 AI 系统详解", subtitle), Spacer(1, 26),
             Paragraph("Unreal Engine 5.3 · GAS · AIController · Behavior Tree", body),
             Paragraph("2026-10-16 ｜ refactor/gas-3c", body), Spacer(1, 18),
             Paragraph("阅读说明：先介绍当前实现，再说明标准重构目标。计划功能不等于已经完成；"
                       "编译与自动测试也不等于完整实机或打包验收。", body), PageBreak(),
             Paragraph("目录", subheading)]
    toc = TableOfContents()
    toc.levelStyles = [ParagraphStyle("TOC", parent=body, leading=20, spaceBefore=3)]
    story.extend([toc, PageBreak()])
    lines = source.read_text(encoding="utf-8").splitlines()
    i = 0
    # Skip Markdown cover; the document cover above supplies the title.
    while i < len(lines):
        line = lines[i].strip()
        if not line or line.startswith("# ") or line == "## 敌人 AI 系统详解":
            i += 1
            continue
        if line.startswith("```"):
            block = []
            i += 1
            while i < len(lines) and not lines[i].startswith("```"):
                block.append(lines[i])
                i += 1
            story.append(Preformatted("\n".join(block), code))
        elif line.startswith("|"):
            rows = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                parts = [v.strip() for v in lines[i].strip().strip("|").split("|")]
                if not all(re.fullmatch(r":?-+:?", p) for p in parts):
                    rows.append([Paragraph(markup(p), cell) for p in parts])
                i += 1
            widths = [doc.width / len(rows[0])] * len(rows[0])
            if len(widths) == 3:
                widths = [doc.width * .24, doc.width * .30, doc.width * .46]
            table = Table(rows, colWidths=widths, repeatRows=1, hAlign="LEFT")
            table.setStyle(TableStyle([
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#dceaf0")),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#f5f8fa")]),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("GRID", (0, 0), (-1, -1), .3, colors.HexColor("#c8d6de")),
                ("LEFTPADDING", (0, 0), (-1, -1), 7),
                ("RIGHTPADDING", (0, 0), (-1, -1), 7),
                ("TOPPADDING", (0, 0), (-1, -1), 6),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
            ]))
            story.extend([table, Spacer(1, 10)])
            continue
        elif line.startswith("### "):
            story.append(Paragraph(markup(line[4:]), subheading))
        elif line.startswith("## "):
            story.append(Paragraph(markup(line[3:]), heading))
        else:
            story.append(Paragraph(markup(line), body))
        i += 1
    doc.multiBuild(story)
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=Path("docs/enemy_ai_project_guide.md"))
    parser.add_argument("--output", type=Path, default=Path("docs/RDCA_敌人AI系统详解.pdf"))
    parser.add_argument("--font", type=Path, default=Path("C:/Windows/Fonts/simhei.ttf"))
    args = parser.parse_args()
    print(build(args.source, args.output, args.font))
