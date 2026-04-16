# Multi-Output Usage Guide

This document describes how to use the `--multi-output` feature added in this fork of qpdf.

## Overview

`--multi-output` writes multiple page groups from a single input PDF to separate output files in one qpdf invocation. This is significantly more efficient than spawning multiple qpdf processes because the input PDF is parsed only once.

## Basic Syntax

```bash
qpdf input.pdf --multi-output="<ranges>" <output-pattern>
```

Where:

- `<ranges>` is a semicolon-separated list of page ranges. Each range produces one output file.
- `<output-pattern>` is the output file name. Use `%d` to insert the output number, or end with `.pdf` to get automatic `-N` insertion.

## Page Range Syntax

Same as qpdf's `--pages`:

- `1-3` — pages 1 through 3
- `1,3,5` — pages 1, 3, and 5
- `r1-r3` — last 3 pages
- `1-z` — all pages
- `1-3,5,7-9` — combine multiple selectors

## Output Naming

With pattern `out-%d.pdf`:

- `out-1.pdf`, `out-2.pdf`, `out-3.pdf`, ...

With pattern `result.pdf` (ends in .pdf, no `%d`):

- `result-1.pdf`, `result-2.pdf`, ...

With pattern `output` (no extension, no `%d`):

- `output-1`, `output-2`, ...

## Threading

By default, outputs are written sequentially (`--multi-output-threads=1`).

- `--multi-output-threads=0` — auto-detect based on CPU cores
- `--multi-output-threads=N` — explicit N threads

When threading is enabled, stream data is copied into memory during preparation so that each output PDF is fully independent for safe parallel writing.

### Memory trade-off

- **Sequential** (`threads=1`, default): minimal memory — one output at a time.
- **Parallel** (`threads>1`): all outputs exist in memory simultaneously due to `setImmediateCopyFrom(true)`. Use threading for large batches of small outputs or when wall-clock time matters more than memory.

## Compatibility

`--multi-output` is **mutually exclusive** with:

- `--split-pages`
- `--replace-input`
- `--copy-encryption`
- Writing to stdout (`-`)
- `--json` output

`--multi-output` **works with**:

- `--pages` (page selection is applied before multi-output)
- `--encrypt`
- `--compress-streams`, `--decrypt`
- `--rotate`, `--flatten-annotations`
- All other transformations

## Node.js Integration Example

For applications that currently spawn multiple qpdf processes:

### Before (N processes)

```typescript
for (const group of groups) {
  const pageList = group.join(',');
  await spawnProcess('qpdf', [
    inputPath, '--pages', '.', pageList, '--', outputPath(group),
  ]);
}
```

### After (1 process)

```typescript
const ranges = groups.map(g => g.join(',')).join(';');
const pattern = path.join(outputDir, 'extracted-%d.pdf');
await spawnProcess('qpdf', [
  inputPath,
  `--multi-output=${ranges}`,
  '--multi-output-threads=0', // optional: enable parallelism
  pattern,
]);
```

### JSON job alternative

```typescript
import { writeFile } from 'fs/promises';

const job = {
  inputFile: inputPath,
  multiOutput: groups.map(g => g.join(',')).join(';'),
  multiOutputThreads: '0',
  outputFile: path.join(outputDir, 'extracted-%d.pdf'),
};

await writeFile('/tmp/job.json', JSON.stringify(job));
await spawnProcess('qpdf', ['--job-json-file=/tmp/job.json']);
```

## Performance

Expected improvements vs spawning N processes:

| Metric      | N processes               | Multi-output (sequential)       | Multi-output (parallel)     |
| ----------- | ------------------------- | ------------------------------- | --------------------------- |
| Memory peak | N × PDF size              | 1 × PDF size                    | ~N × output size (in RAM)   |
| CPU total   | N × (parse + write)       | 1 × parse + N × write           | 1 × parse + N × write       |
| Wall clock  | Parallel via OS processes | Sequential                      | Parallel via threads        |
| Disk I/O    | N reads of input          | 1 read of input                 | 1 read of input             |

## Examples

### Extract chapters from a book

```bash
qpdf book.pdf --multi-output="1-20;21-45;46-80;81-120" chapter-%d.pdf
# → chapter-1.pdf (pages 1-20), chapter-2.pdf (21-45), etc.
```

### Create one file per page

```bash
qpdf doc.pdf --multi-output="1;2;3;4;5" page-%d.pdf
# Equivalent to --split-pages=1, but more flexible if you want specific pages only.
```

### Mix with encryption

```bash
qpdf input.pdf --multi-output="1-5;6-10" \
  --encrypt user-pw owner-pw 256 -- \
  encrypted-%d.pdf
```

### Use with page rotation

```bash
qpdf input.pdf --rotate=+90:1-5 --multi-output="1-5;6-10" out-%d.pdf
# Pages 1-5 of the input are rotated before multi-output splits.
```

### Combine with --pages to select from multiple inputs first

```bash
qpdf --empty --pages a.pdf 1-3 b.pdf 4-6 -- \
     --multi-output="1-3;4-6" combined-%d.pdf
# First combines 6 pages from a.pdf + b.pdf, then splits into 2 outputs.
```

## Building from this Fork

### Using Docker (recommended)

```bash
git clone https://github.com/IvanMicai/qpdf.git
cd qpdf
git checkout feature/multi-output
docker build -f Dockerfile.dev -t qpdf-dev .
docker run --rm qpdf-dev ./build/qpdf/qpdf --version
```

### Native build

```bash
git clone https://github.com/IvanMicai/qpdf.git
cd qpdf
git checkout feature/multi-output
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
./build/qpdf/qpdf --version
```

## Testing

Run the full test suite to verify the build:

```bash
docker build -f Dockerfile.dev -t qpdf-dev .
docker run --rm qpdf-dev bash -c 'cd build && ctest --output-on-failure -j$(nproc)'
```

Manual smoke test:

```bash
docker run --rm qpdf-dev bash -c '
  ./build/qpdf/qpdf qpdf/qtest/qpdf/11-pages.pdf \
    --multi-output="1-3;4-6;7-11" /tmp/out-%d.pdf
  for f in /tmp/out-*.pdf; do
    pages=$(./build/qpdf/qpdf --show-npages "$f")
    echo "$f: $pages pages"
  done
'
```

Expected output:

```
/tmp/out-1.pdf: 3 pages
/tmp/out-2.pdf: 3 pages
/tmp/out-3.pdf: 5 pages
```

## Implementation Details

The feature uses a two-phase architecture:

1. **Preparation phase (single-threaded):** parse the input PDF once, create all output QPDF objects, copy pages from source to each output using `copyForeignObject`. When threading is enabled, `QPDF::setImmediateCopyFrom(true)` is called on the source so stream data is copied to RAM immediately (rather than lazily), making each output QPDF fully independent.

2. **Write phase (possibly multi-threaded):** each output QPDF is written via `QPDFWriter` on its own. Since the outputs are independent, this is safe to do from multiple threads. A simple atomic-counter-based thread pool distributes work across workers.

Global state that must be set before threads spawn:

- `Pl_Flate::setCompressionLevel` (process-global).
- `copy_encryption` is blocked for multi-output (calls `processFile`, not thread-safe).

See `libqpdf/QPDFJob.cc::doMultiOutput` and `setWriterOptionsThreadSafe` for the implementation.
