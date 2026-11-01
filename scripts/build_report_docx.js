/**
 * Builds the MCA project report DOCX from scripts/report_content.js.
 *
 * Usage: node scripts/build_report_docx.js
 * Output: Shridhar_Kumar_Project_Report.docx
 *
 * The layout (A4, Times New Roman, roman front matter, arabic body,
 * auto-updating table of contents, running header) follows the format of
 * the sample project report supplied as the reference.
 *
 * Usage: node scripts/build_report_docx.js
 * Output: docs/Shridhar_Kumar_Project_Report_16pages.docx
 */
const fs = require('fs');
const path = require('path');
const {
  Document, Packer, Paragraph, TextRun, HeadingLevel, AlignmentType,
  Table, TableRow, TableCell, WidthType, BorderStyle, PageBreak,
  Footer, Header, PageNumber, TableOfContents, VerticalAlign,
  ShadingType, LevelFormat, NumberFormat,
} = require('docx');

const C = require('./report_content.js');

const FONT = 'Times New Roman';
const MONO = 'Courier New';

const PAGE = { width: 11906, height: 16838 };           // A4 in twips
const MARGIN = { top: 1440, bottom: 1440, left: 1800, right: 1440 };

let numberingInstance = 0;

/* ------------------------------------------------------------------ text */

function run(text, o = {}) {
  return new TextRun({
    text,
    font: o.font || FONT,
    size: o.size ?? 24,
    bold: !!o.bold,
    italics: !!o.italics,
    color: o.color || '000000',
  });
}

function para(text, o = {}) {
  return new Paragraph({
    alignment: o.align || AlignmentType.JUSTIFIED,
    spacing: { before: o.before ?? 0, after: o.after ?? 140, line: o.line ?? 360 },
    indent: o.indent,
    children: Array.isArray(text) ? text : [run(text, o)],
  });
}

function centered(text, o = {}) {
  return para(text, { ...o, align: AlignmentType.CENTER });
}

function h3(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_3,
    spacing: { before: 200, after: 100, line: 360 },
    children: [run(text, { bold: true, size: 24 })],
  });
}

function sectionHeading(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_2,
    keepNext: true,
    spacing: { before: 260, after: 120, line: 360 },
    children: [run(text, { bold: true, size: 26 })],
  });
}

function frontMatterHeading(text, pageBreak = true) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_1,
    pageBreakBefore: pageBreak,
    alignment: AlignmentType.CENTER,
    spacing: { before: 240, after: 320, line: 360 },
    children: [run(text, { bold: true, size: 30 })],
  });
}

function bullets(items) {
  return items.map((t) => new Paragraph({
    bullet: { level: 0 },
    alignment: AlignmentType.JUSTIFIED,
    spacing: { before: 0, after: 80, line: 360 },
    children: [run(t)],
  }));
}

function numbers(items) {
  const instance = ++numberingInstance;
  return items.map((t) => new Paragraph({
    numbering: { reference: 'report-numbering', level: 0, instance },
    alignment: AlignmentType.JUSTIFIED,
    spacing: { before: 0, after: 80, line: 360 },
    children: [run(t)],
  }));
}

/* ----------------------------------------------------------------- table */

function buildTable({ headers, rows, widths }) {
  const n = headers.length;
  // Short tables are kept whole; a long table must be allowed to break
  // between rows, otherwise Word pushes the caption to one page and the
  // table to the next, leaving a blank page behind.
  const keepTogether = rows.length <= 10;
  const pct = widths && widths.length === n
    ? widths
    : headers.map(() => Math.round(100 / n));

  const cell = (text, i, o = {}) => new TableCell({
    width: { size: pct[i], type: WidthType.PERCENTAGE },
    verticalAlign: VerticalAlign.CENTER,
    margins: { top: 55, bottom: 55, left: 85, right: 85 },
    shading: o.fill ? { type: ShadingType.CLEAR, color: 'auto', fill: o.fill } : undefined,
    children: String(text).split('\n').map((line) => new Paragraph({
      alignment: o.align || AlignmentType.LEFT,
      spacing: { before: 0, after: 0, line: 250 },
      // Keeping rows with the next row stops Word from leaving a single
      // row of a short table stranded above a page break.
      keepNext: !!o.keepNext,
      children: [run(line, { size: 20, bold: !!o.bold })],
    })),
  });

  const tableRows = [
    new TableRow({
      tableHeader: true,
      cantSplit: true,
      children: headers.map((h, i) => cell(h, i, {
        bold: true,
        fill: 'DEEAF6',
        align: AlignmentType.CENTER,
        keepNext: keepTogether,
      })),
    }),
    ...rows.map((r, ri) => new TableRow({
      cantSplit: true,
      children: r.map((v, i) => cell(v, i, {
        keepNext: keepTogether && ri < rows.length - 1,
      })),
    })),
  ];

  return new Table({
    width: { size: 100, type: WidthType.PERCENTAGE },
    rows: tableRows,
    borders: {
      top: { style: BorderStyle.SINGLE, size: 4, color: '8EA9C1' },
      bottom: { style: BorderStyle.SINGLE, size: 4, color: '8EA9C1' },
      left: { style: BorderStyle.SINGLE, size: 4, color: '8EA9C1' },
      right: { style: BorderStyle.SINGLE, size: 4, color: '8EA9C1' },
      insideHorizontal: { style: BorderStyle.SINGLE, size: 2, color: 'B4C7DC' },
      insideVertical: { style: BorderStyle.SINGLE, size: 2, color: 'B4C7DC' },
    },
  });
}

function caption(text) {
  return new Paragraph({
    keepNext: true,
    alignment: AlignmentType.CENTER,
    spacing: { before: 200, after: 90, line: 280 },
    children: [run(text, { size: 20, bold: true })],
  });
}

/* --------------------------------------------------------------- diagram */

function buildDiagram({ boxes }) {
  const noneBorders = {
    top: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
    bottom: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
    left: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
    right: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
  };
  const boxBorders = {
    top: { style: BorderStyle.SINGLE, size: 6, color: '2F5597' },
    bottom: { style: BorderStyle.SINGLE, size: 6, color: '2F5597' },
    left: { style: BorderStyle.SINGLE, size: 6, color: '2F5597' },
    right: { style: BorderStyle.SINGLE, size: 6, color: '2F5597' },
  };

  const isBox = (s) => String(s).indexOf('\n') !== -1;

  const row = (children, borders, fill) => new TableRow({
    cantSplit: true,
    children: [new TableCell({
      width: { size: 100, type: WidthType.PERCENTAGE },
      borders,
      shading: fill ? { type: ShadingType.CLEAR, color: 'auto', fill } : undefined,
      margins: { top: fill ? 110 : 30, bottom: fill ? 110 : 30, left: 120, right: 120 },
      children,
    })],
  });

  const boxRow = (box) => row(
    String(box).split('\n').map((line, i) => new Paragraph({
      alignment: AlignmentType.CENTER,
      spacing: { before: 0, after: 0, line: 250 },
      children: [run(line, { size: i === 0 ? 21 : 19, bold: i === 0 })],
    })),
    boxBorders,
    'EAF1FB',
  );

  const arrowRow = (label) => row(
    label
      ? [
        new Paragraph({
          alignment: AlignmentType.CENTER,
          spacing: { before: 0, after: 0, line: 200 },
          children: [run('\u2193', { size: 22, bold: true })],
        }),
        new Paragraph({
          alignment: AlignmentType.CENTER,
          spacing: { before: 0, after: 0, line: 200 },
          children: [run(label, { size: 17, italics: true, color: '44546A' })],
        }),
      ]
      : [new Paragraph({
        alignment: AlignmentType.CENTER,
        spacing: { before: 0, after: 0, line: 220 },
        children: [run('\u2193', { size: 22, bold: true })],
      })],
    noneBorders,
    null,
  );

  const rows = [];
  boxes.forEach((box, idx) => {
    if (isBox(box)) {
      rows.push(boxRow(box));
      const next = boxes[idx + 1];
      if (next && isBox(next)) rows.push(arrowRow(null));
    } else {
      rows.push(arrowRow(String(box)));
    }
  });

  return new Table({
    width: { size: 100, type: WidthType.PERCENTAGE },
    rows,
    borders: {
      top: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
      bottom: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
      left: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
      right: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
      insideHorizontal: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
      insideVertical: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
    },
  });
}

/* ------------------------------------------------------------------ code */

function buildCode({ text }) {
  const lines = String(text).split('\n');
  return lines.map((line, i) => new Paragraph({
    spacing: { before: 0, after: 0, line: 240 },
    indent: { left: 360, right: 240 },
    shading: { type: ShadingType.CLEAR, color: 'auto', fill: 'F5F5F5' },
    border: i === 0
      ? { top: { style: BorderStyle.SINGLE, size: 2, color: 'D0D0D0', space: 4 } }
      : i === lines.length - 1
        ? { bottom: { style: BorderStyle.SINGLE, size: 2, color: 'D0D0D0', space: 4 } }
        : undefined,
    children: [run(line === '' ? ' ' : line, { font: MONO, size: 18 })],
  }));
}

/* ----------------------------------------------------------------- chart */

const TEST_CATEGORIES = ['Auth & access', 'Exam management', 'Exam engine',
  'Results', 'Proctoring', 'Student UX'];
const TEST_COUNTS = [32, 21, 21, 12, 7, 6];

const CHART_SPECS = {
  risk_weights: {
    type: 'column',
    title: 'Risk weight assigned to each proctoring signal',
    categories: ['No face', 'Multiple faces', 'Looking away', 'Tab switch',
      'Camera stopped', 'Verification fail'],
    series: [{ name: 'Points', values: [20, 40, 10, 30, 25, 50], color: '2F5597' }],
    legend: false,
  },
  test_categories: {
    type: 'column',
    title: 'Automated tests by functional area',
    categories: TEST_CATEGORIES,
    series: [{ name: 'Tests', values: TEST_COUNTS, color: 'C55A11' }],
    legend: false,
  },
};

function buildChart({ key }) {
  const spec = CHART_SPECS[key];
  if (!spec) throw new Error(`Unknown chart key: ${key}`);
  return new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before: 40, after: 180 },
    children: [new ChartRun({
      ...spec,
      transformation: { width: 540, height: 300 },
    })],
  });
}

/* --------------------------------------------------------------- renderer */

function renderBlocks(blocks) {
  const out = [];
  for (const b of blocks) {
    switch (b.type) {
      case 'p':
        out.push(para(b.text));
        break;
      case 'lead':
        out.push(para(b.text));
        break;
      case 'bullets':
        out.push(...bullets(b.items));
        out.push(para('', { after: 40 }));
        break;
      case 'numbers':
        out.push(...numbers(b.items));
        out.push(para('', { after: 40 }));
        break;
      case 'h3':
        out.push(h3(b.text));
        break;
      case 'table':
        out.push(caption(b.caption));
        out.push(buildTable(b));
        out.push(para('', { after: 60 }));
        break;
      case 'diagram':
        out.push(caption(b.caption));
        out.push(buildDiagram(b));
        out.push(para('', { after: 60 }));
        break;
      case 'chart':
        out.push(caption(b.caption));
        out.push(buildChart(b));
        break;
      case 'code': {
        if (b.caption) out.push(caption(b.caption));
        out.push(...buildCode(b));
        out.push(para('', { after: 80 }));
        break;
      }
      case 'pagebreak':
        out.push(new Paragraph({ children: [new PageBreak()] }));
        break;
      default:
        throw new Error(`Unknown block type: ${b.type}`);
    }
  }
  return out;
}

/* ------------------------------------------------------------- title page */

function titlePage() {
  const gap = () => para('', { after: 120, line: 240 });
  return [
    gap(), gap(), gap(),
    centered('A PROJECT REPORT', { bold: true, size: 26 }),
    centered('ON', { bold: true, size: 22, after: 260 }),
    centered(C.title, { bold: true, size: 34, after: 260, line: 400 }),
    gap(), gap(),
    centered('Submitted in partial fulfilment of the requirements for the award of the degree of', { size: 22 }),
    centered('MASTER OF COMPUTER APPLICATIONS', { bold: true, size: 26, after: 200 }),
    gap(), gap(),
    centered('Submitted by', { size: 22, after: 60 }),
    centered(C.student.toUpperCase(), { bold: true, size: 26, after: 40 }),
    centered(`UID: ${C.uid}`, { size: 22, after: 120 }),
    gap(),
    centered('Under the guidance of', { size: 22, after: 40 }),
    centered('[Project Guide Name]', { bold: true, size: 22, after: 120 }),
    gap(),
    centered('DEPARTMENT OF COMPUTER APPLICATIONS', { bold: true, size: 24, after: 40 }),
    centered('[Name of Institute]', { bold: true, size: 24, after: 40 }),
    centered('[City]', { size: 22, after: 40 }),
    centered(`Session ${C.session}`, { size: 22 }),
  ];
}

function bodyPage(heading, paragraphs) {
  return [
    frontMatterHeading(heading),
    ...paragraphs.map((t) => para(t)),
  ];
}

function signatureBlock(left, right) {
  const cell = (text) => new TableCell({
    width: { size: 50, type: WidthType.PERCENTAGE },
    borders: {
      top: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
      bottom: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
      left: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
      right: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
    },
    children: String(text).split('\n').map((l) => new Paragraph({
      spacing: { before: 0, after: 0, line: 300 },
      children: [run(l, { size: 22 })],
    })),
  });
  return new Table({
    width: { size: 100, type: WidthType.PERCENTAGE },
    rows: [new TableRow({ children: [cell(left), cell(right)] })],
    borders: {
      top: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
      bottom: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
      left: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
      right: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
      insideHorizontal: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
      insideVertical: { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' },
    },
  });
}

/* ------------------------------------------------------------------- main */

const frontMatter = [
  ...titlePage(),

  frontMatterHeading('CERTIFICATE'),
  ...C.certificate.map((t) => para(t, { after: 180 })),
  para('', { after: 400 }),
  signatureBlock('__________________________\nProject Guide\n[Designation]', '__________________________\nHead of Department\nDepartment of Computer Applications'),
  para('', { after: 300 }),
  signatureBlock('__________________________\nExternal Examiner', '__________________________\nDate'),

  frontMatterHeading('DECLARATION'),
  ...C.declaration.map((t) => para(t, { after: 180 })),
  para('', { after: 600 }),
  signatureBlock('Place: [City]\nDate: __________________', `__________________________\n${C.student}\nUID: ${C.uid}`),

  frontMatterHeading('ACKNOWLEDGEMENT'),
  ...C.acknowledgement.map((t) => para(t, { after: 160 })),

  frontMatterHeading('ABSTRACT'),
  ...C.abstract.map((t) => para(t, { after: 160 })),

  new Paragraph({
    pageBreakBefore: true,
    alignment: AlignmentType.CENTER,
    spacing: { before: 240, after: 320, line: 360 },
    children: [run('TABLE OF CONTENTS', { bold: true, size: 30 })],
  }),
  new TableOfContents('Table of Contents', { hyperlink: true, headingStyleRange: '1-2' }),

  frontMatterHeading('LIST OF TABLES'),
  ...C.listOfTables.map((t) => new Paragraph({
    spacing: { before: 0, after: 80, line: 320 },
    indent: { left: 360, hanging: 360 },
    children: [run(t, { size: 22 })],
  })),

  frontMatterHeading('LIST OF FIGURES', false),
  ...C.listOfFigures.map((t) => new Paragraph({
    spacing: { before: 0, after: 80, line: 320 },
    indent: { left: 360, hanging: 360 },
    children: [run(t, { size: 22 })],
  })),
];

const mainBody = [];

C.chapters.forEach((ch, idx) => {
  mainBody.push(new Paragraph({
    pageBreakBefore: idx > 0,
    alignment: AlignmentType.CENTER,
    spacing: { before: 0, after: 60, line: 360 },
    children: [run(`CHAPTER ${ch.number}`, { bold: true, size: 24, color: '404040' })],
  }));
  mainBody.push(new Paragraph({
    heading: HeadingLevel.HEADING_1,
    alignment: AlignmentType.CENTER,
    spacing: { before: 0, after: 280, line: 400 },
    children: [run(ch.title, { bold: true, size: 32 })],
  }));
  for (const sec of ch.sections) {
    mainBody.push(sectionHeading(sec.heading));
    mainBody.push(...renderBlocks(sec.blocks));
  }
});

mainBody.push(frontMatterHeading('REFERENCES', true));
C.references.forEach((ref, i) => {
  mainBody.push(new Paragraph({
    spacing: { before: 0, after: 120, line: 340 },
    indent: { left: 720, hanging: 720 },
    alignment: AlignmentType.JUSTIFIED,
    children: [run(`[${i + 1}]  `), run(ref)],
  }));
});

for (const app of C.appendices) {
  mainBody.push(frontMatterHeading(app.title, true));
  mainBody.push(...renderBlocks(app.blocks));
}

/* --------------------------------------------------------------- document */

const headerText = new Paragraph({
  alignment: AlignmentType.CENTER,
  spacing: { before: 0, after: 60, line: 240 },
  border: { bottom: { style: BorderStyle.SINGLE, size: 4, color: 'BFBFBF', space: 2 } },
  children: [run('Smart IoT Water Tank Monitoring System with Live Web Dashboard', { size: 18, italics: true, color: '595959' })],
});

const footerText = new Paragraph({
  alignment: AlignmentType.CENTER,
  spacing: { before: 0, after: 0, line: 240 },
  children: [
    run('Page ', { size: 20 }),
    new TextRun({ children: [PageNumber.CURRENT], font: FONT, size: 20 }),
  ],
});

const doc = new Document({
  title: 'Smart IoT Water Tank Monitoring System with Live Web Dashboard',
  subject: 'MCA Project Report',
  creator: C.student,
  description: `Project report submitted by ${C.student} (UID ${C.uid})`,
  features: { updateFields: true },
  numbering: {
    config: [{
      reference: 'report-numbering',
      levels: [{
        level: 0,
        format: LevelFormat.DECIMAL,
        text: '%1.',
        alignment: AlignmentType.START,
        style: { paragraph: { indent: { left: 720, hanging: 360 } } },
      }],
    }],
  },
  styles: {
    default: {
      document: {
        run: { font: FONT, size: 24, color: '000000' },
        paragraph: { spacing: { line: 360, after: 140 } },
      },
      heading1: {
        run: { font: FONT, size: 30, bold: true, color: '000000' },
        paragraph: { spacing: { before: 240, after: 200, line: 360 } },
      },
      heading2: {
        run: { font: FONT, size: 26, bold: true, color: '000000' },
        paragraph: { spacing: { before: 240, after: 120, line: 360 } },
      },
      heading3: {
        run: { font: FONT, size: 24, bold: true, color: '000000' },
        paragraph: { spacing: { before: 200, after: 100, line: 360 } },
      },
    },
  },
  sections: [
    {
      properties: {
        titlePage: true,
        page: {
          size: PAGE,
          margin: MARGIN,
          pageNumbers: { start: 1, formatType: NumberFormat.LOWER_ROMAN },
        },
      },
      footers: {
        default: new Footer({ children: [footerText] }),
        first: new Footer({ children: [new Paragraph({ children: [] })] }),
      },
      children: frontMatter,
    },
    {
      properties: {
        page: { size: PAGE, margin: MARGIN, pageNumbers: { start: 1 } },
      },
      headers: { default: new Header({ children: [headerText] }) },
      footers: { default: new Footer({ children: [footerText] }) },
      children: mainBody,
    },
  ],
});

Packer.toBuffer(doc).then((buffer) => {
  const out = path.join(__dirname, '..', 'docs', 'Shridhar_Kumar_Project_Report_16pages.docx');
  fs.writeFileSync(out, buffer);
  console.log('WROTE', out, buffer.length, 'bytes');
}).catch((e) => {
  console.error('BUILD FAILED:', e);
  process.exit(1);
});
