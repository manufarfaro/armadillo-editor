# Armadillo Editor

[![host-tests](https://github.com/manufarfaro/armadillo-editor/actions/workflows/host-tests.yml/badge.svg)](https://github.com/manufarfaro/armadillo-editor/actions/workflows/host-tests.yml)
[![lint](https://github.com/manufarfaro/armadillo-editor/actions/workflows/lint.yml/badge.svg)](https://github.com/manufarfaro/armadillo-editor/actions/workflows/lint.yml)
[![release](https://github.com/manufarfaro/armadillo-editor/actions/workflows/release.yml/badge.svg)](https://github.com/manufarfaro/armadillo-editor/actions/workflows/release.yml)

A System 7 markdown editor for 68030-and-up classic Macintosh. Cross-compiled with [Retro68](https://github.com/autc04/Retro68).

Current status: **pre-MVP** — the repo holds the PRD, OpenSpec change, capability specs, and the implementation plan. Source is implemented per `openspec/changes/add-md-editor-mvp/plan-1-host-pipeline.md`.

## Where things live

- `PRD.md` — product requirements
- `openspec/` — change proposals, designs, authoritative capability specs, implementation plans
- `src/` — app shell + small glue
- `src_pane/`, `render/`, `mdparse/`, `scanner/` — top-level module folders (peers of `src/`)
- `third_party/md4c/` — vendored markdown parser at a pinned commit
- `third_party/unity/` — vendored test framework
- `test/` — our own test internals (fakes, recorder)
- `.github/workflows/` — CI workflows (host-tests, lint, release); CodeQL runs via GitHub's Default Setup (see Security tab)

## Build

### Host unit tests (fast loop)

```bash
make -f Makefile.hosttests test       # full suite, < 2 s
make -f Makefile.hosttests clean
```

Run a specific test binary:

```bash
make -f Makefile.hosttests build_hosttests/render_test
./build_hosttests/render_test
```

### Cross-compile for 68k (Quadra 800)

Local — requires Retro68 installed at `~/Documents/Projects/Retro68-build/`:

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/Documents/Projects/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
make
```

Produces `ArmadilloEditor.APPL` and `ArmadilloEditor.dsk` (800K floppy image).

Via Docker (no local Retro68 install needed):

```bash
docker run --rm -v "$PWD":/work -w /work ghcr.io/autc04/retro68:latest bash -lc '
  mkdir -p build && cd build &&
  cmake .. -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake &&
  make
'
```

## CI

Every push and PR runs three workflows:

- **host-tests** (~1 min) — Unity-based unit tests on ubuntu-latest
- **lint** (~30 s) — cppcheck (warnings/style/performance/portability, C89). No formatter check; the codebase uses hand-formatted MPW C era style (column-aligned declarations, one-line OSErr guards) that automated formatters cannot fully preserve.
- **release** (~5 min) — Retro68 cross-compile via `ghcr.io/autc04/retro68`; uploads `.APPL` and `.dsk` as workflow artifacts (14-day retention)

CodeQL static analysis runs via GitHub's [Default Setup for Code Scanning](https://docs.github.com/en/code-security/code-scanning/enabling-code-scanning/configuring-default-setup-for-code-scanning) (configured in repo Settings → Code security → Code scanning). Results appear in the Security tab. No workflow file needed — GitHub manages the queries and build detection.

Tag pushes matching `v*` / `mvp-*` / `tier-*` additionally create a GitHub Release with the `.APPL` and `.dsk` attached.

## Architecture

See `CLAUDE.md` for the load-bearing rules (flat block model, single-parse-two-consumers, Handle-backed arena, three test seams, vendor-free headers). The full design is in `openspec/changes/add-md-editor-mvp/design.md`. For a narrative explanation of the concepts (Handles, arenas, SAX parsing, TextEdit vs WASTE, etc.), see `docs/armadillo-architecture-and-concepts.md`.

## License

TBD.
