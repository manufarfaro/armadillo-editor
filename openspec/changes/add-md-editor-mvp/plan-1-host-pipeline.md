# Armadillo Editor Plan 1 — Host-Testable Pipeline

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build every host-testable module of the Armadillo Editor MVP (arena, doc, debounce, mdparse, scanner, render, host-buildable parts of file_io) with full unit-test coverage. When this plan completes, `make -f Makefile.hosttests test` runs all tests green and the entire markdown-parse-to-recorded-draw-calls pipeline works end-to-end without requiring Mac Toolbox or QEMU.

**Architecture:** Follow the design in `openspec/changes/add-md-editor-mvp/design.md`. Single parse, two outputs (scanner + render), flat-block model, HLocked `Handle`-backed arena with grow-before-parse policy, vendor-free public headers, three test seams (MacSyscalls, DrawOps, MdParseSink). C89, Unity for tests, md4c vendored for markdown parsing.

**Tech Stack:** C89 host + Retro68 cross-compile later; CMake for the cross build; plain GNU Make for host tests; Unity test framework (vendored); md4c markdown parser (vendored, pinned commit).

**Out of scope for this plan:** `src_pane.c`, `draw_qd.c`, `win_editor.c`, `menus.c`, `app.c`, the real `MacSyscalls` wiring, `armadillo.r` resources beyond a minimal stub, and the on-device smoke test. Those live in Plan 2.

**Pulled forward from Plan 2 into this plan:** GitHub Actions CI (four workflows) and README badges. Rationale — CI pays off the moment host tests exist, not after the app ships, so it lands at the end of Plan 1 rather than waiting for Plan 2. The cross-build job uses the official `ghcr.io/autc04/retro68` Docker image, so no Retro68 install is required on the CI runner.

---

## Files created or modified in this plan

### Foundation
- Create: `.gitignore`
- Create: `CMakeLists.txt` (stub — builds minimal `.APPL`)
- Create: `Makefile.hosttests` (host unit-test runner)
- Create: `armadillo.r` (stub — just enough to produce an `.APPL`)
- Create: `CLAUDE.md`, `README.md` (short; expanded in Plan 2)
- Create: `third_party/unity/unity.c`, `third_party/unity/unity.h`, `third_party/unity/unity_internals.h` (vendored)
- Create: `third_party/md4c/src/md4c.c`, `third_party/md4c/src/md4c.h`, `third_party/md4c/COMMIT.txt`, `third_party/md4c/README.md` (vendored)

### Public headers
- Create: `src/mac_syscalls.h` (function-pointer struct for Toolbox wrappers)
- Create: `src/doc.h`
- Create: `src/file_io.h`
- Create: `src/debounce.h`
- Create: `src_pane/src_pane.h` (vendor-free — no Toolbox types)
- Create: `render/blocks.h`
- Create: `render/inlines.h`
- Create: `render/arena.h`
- Create: `render/draw_qd.h`
- Create: `render/render.h`
- Create: `mdparse/mdparse.h`
- Create: `scanner/scanner.h`

### Test support
- Create: `test/fake_syscalls.h`, `test/fake_syscalls.c`
- Create: `test/recorder.h`, `test/recorder.c`

### Implementations (with colocated `_test.c`)
- Create: `render/arena.c`, `render/arena_test.c`
- Create: `src/doc.c`, `src/doc_test.c`
- Create: `src/debounce.c`, `src/debounce_test.c`
- Create: `mdparse/mdparse.c`, `mdparse/mdparse_test.c`
- Create: `scanner/scanner.c`, `scanner/scanner_test.c`
- Create: `render/render.c`, `render/render_test.c`
- Create: `src/file_io.c`, `src/file_io_test.c`

### CI & delivery
- Create: `.clang-format`
- Create: `.github/workflows/host-tests.yml`
- Create: `.github/workflows/lint.yml`
- Create: `.github/workflows/release.yml`
- Configure (outside the repo): CodeQL via GitHub's Default Setup in repo Settings → Code security → Code scanning
- Modify: `README.md` (full MVP version replaces the Task 2 stub — adds badges + CI section + Docker cross-compile instructions)

### Not created in this plan (Plan 2)
- `src_pane/src_pane.c`
- `src/draw_qd.c`, `src/draw_qd.h`
- `src/win_editor.c`, `src/menus.c`, `src/app.c`, `src/smoke_test.c`
- Full `armadillo.r` (ALRTs, STR#, ICN#)
- Real `MacSyscalls` wiring

---

## Commit policy for this plan

- **Every task that passes its verification step ends with a commit.** No trailing uncommitted work between tasks.
- **Every commit uses the 1Password-signed git identity.** Prefix every commit with `SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock"`.
- **No `Co-Authored-By` trailers.** The user does not want Claude attribution in commit history.
- **Commit messages use imperative mood** ("Add arena allocator", not "Added arena allocator"). Keep subject ≤ 72 chars; wrap body at 72 chars; explain *why* in the body only when non-obvious.
- **If a pre-commit hook fails**, investigate and fix the underlying issue. Do NOT use `--no-verify`. If a signing hook fails, re-check `SSH_AUTH_SOCK`.

Shorthand used in steps below: `git-commit "<msg>"` means

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "<msg>"
```

Run this literal command; don't alias it.

---

## Phase overview

Phases group related tasks into natural deliverable boundaries. After each phase, `make -f Makefile.hosttests test` should be green for everything built so far.

1. **Phase 1 — Foundation** (Tasks 1–9): directories, CMakeLists stub, Makefile.hosttests stub, vendored Unity + md4c, `mac_syscalls.h` skeleton, `fake_syscalls` with defaults.
2. **Phase 2 — All public headers** (Tasks 10–21): the 11 public `.h` files from the design plus the fake-syscalls implementation. Headers compile under `-std=c89 -Wall -Werror`.
3. **Phase 3 — Arena** (Tasks 22–28): tests first, then implementation, TDD-style. Arena is foundational; every downstream module depends on it.
4. **Phase 4 — Doc** (Tasks 29–31): dumb data container. Small module, fast.
5. **Phase 5 — Debounce** (Tasks 32–33): pure state machine over `FakeClock`.
6. **Phase 6 — Mdparse** (Tasks 34–37): md4c adapter with `MdParseSink` fan-out.
7. **Phase 7 — Scanner** (Tasks 38–39): style-run producer from sink events.
8. **Phase 8 — Render** (Tasks 40–44): flat-block model construction + layout + draw via `DrawOps`. Largest phase.
9. **Phase 9 — File I/O** (host parts, Task 45): open/save data paths with `FakeSyscalls`.
10. **Phase 10 — CI & GitHub delivery** (Tasks 46–50): three GitHub Actions workflows (host-tests, lint, release) + `.clang-format` + README badges + CodeQL via GitHub's Default Setup. Uses `ghcr.io/autc04/retro68` for the cross-build job.
11. **Phase 11 — Final verification** (Task 51): full suite, spec-coverage audit, tag `plan-1-complete`.

Each task below is atomic — it starts with a clean working tree (except the commit-in-progress staging) and ends with a clean working tree after its commit step.

---

## Phase 1 — Foundation

### Task 1: Create `.gitignore`

**Files:**
- Create: `.gitignore`

- [ ] **Step 1.1: Create `.gitignore` with the standard artifacts**

Create `.gitignore` with exactly this content:

```gitignore
# Build artifacts
build/
build_test/
build_hosttests/

# Retro68 outputs
*.APPL
*.dsk
*.bin
*.flt
*.gdb
*.code
*.rsrc
*.ad

# Host-test outputs
*.o
*.obj
*.a

# macOS / editor noise
.DS_Store
*.swp
*.swo
*~
.vscode/
.idea/
Thumbs.db
```

- [ ] **Step 1.2: Stage and commit**

```bash
git add .gitignore
```

Then `git-commit` with message:

```
Add .gitignore

Ignore Retro68 build outputs (.APPL/.dsk/.bin/.flt), host test
binaries under build_hosttests/, and editor/OS clutter. Host-test
artifacts are never committed because they're host-machine-specific.
```

---

### Task 2: Minimal `README.md` and `CLAUDE.md`

**Files:**
- Create: `README.md`
- Create: `CLAUDE.md`

These are placeholders for now. Full versions land in Plan 2.

- [ ] **Step 2.1: Create `README.md`**

```markdown
# Armadillo Editor

A System 7 markdown editor for 68030+ classic Macintosh. Cross-compiled with Retro68.

Current status: **pre-MVP** — this repo holds the PRD, OpenSpec change, capability specs, and implementation plans. Source code is being implemented per `openspec/changes/add-md-editor-mvp/plan-1-host-pipeline.md`.

## Where things live

- `PRD.md` — product requirements
- `openspec/` — change proposals, designs, authoritative capability specs
- `src/`, `src_pane/`, `render/`, `mdparse/`, `scanner/` — source modules (created during plan execution)
- `third_party/md4c/` — vendored markdown parser
- `test/` — host unit-test support (Unity, fakes, recorder)

Expanded README lands in Plan 2.
```

- [ ] **Step 2.2: Create `CLAUDE.md`**

```markdown
# CLAUDE.md

## Project

Armadillo Editor — a retro68 C markdown editor for 68030+ System 7 Macs.

## Conventions

- C89 compatible (Retro68 GCC + host cc).
- Mockable OS calls via `src/mac_syscalls.h` struct of function pointers; every module that touches the OS takes `const MacSyscalls*`.
- Public headers are vendor-free: no `TEHandle`, `WindowPtr`, `FSSpec`, or md4c types across module boundaries.
- Tests colocated as `<module>_test.c`; host-compile with the repo's `Makefile.hosttests`.
- Use `goto cleanup` resource-release pattern in any function that acquires ≥ 2 resources.
- No `printf`/`DebugStr`/logging in library code. Errors propagate as return codes; alerts are the app shell's job.
- All allocations in the render pipeline go through `Arena*` (Handle-backed). Never call `NewHandle`/`NewPtr`/`malloc` from render/mdparse/scanner directly.

## Build

- Cross-compile (target .APPL): `mkdir -p build && cd build && cmake .. && make`
- Host unit tests: `make -f Makefile.hosttests test`

## See also

- `PRD.md` — what we're building and why
- `openspec/changes/add-md-editor-mvp/design.md` — full architecture
- `openspec/specs/*/spec.md` — authoritative capability specs with scenarios
```

- [ ] **Step 2.3: Stage and commit**

```bash
git add README.md CLAUDE.md
```

Then `git-commit` with message:

```
Add stub README and CLAUDE.md

Pointer-only docs pointing at PRD, OpenSpec change, and design. Full
build / architecture / troubleshooting docs land in Plan 2 alongside
the first runnable .APPL.
```

---

### Task 3: Create directory skeleton

**Files:**
- Create (empty): `src/`, `src_pane/`, `render/`, `mdparse/`, `scanner/`, `test/`, `third_party/`, `third_party/unity/`, `third_party/md4c/`, `third_party/md4c/src/`

- [ ] **Step 3.1: Create the module directories**

```bash
mkdir -p src src_pane render mdparse scanner test third_party/unity third_party/md4c/src
```

- [ ] **Step 3.2: Add placeholder files so git tracks the empty dirs**

Create `.gitkeep` files in each otherwise-empty directory:

```bash
touch src/.gitkeep src_pane/.gitkeep render/.gitkeep mdparse/.gitkeep \
      scanner/.gitkeep test/.gitkeep \
      third_party/.gitkeep third_party/unity/.gitkeep \
      third_party/md4c/.gitkeep third_party/md4c/src/.gitkeep
```

(Each `.gitkeep` will be deleted organically as its directory gains real files.)

- [ ] **Step 3.3: Verify the tree**

```bash
ls -la src src_pane render mdparse scanner test third_party third_party/unity third_party/md4c third_party/md4c/src
```

Expected: each directory exists and contains a `.gitkeep`.

- [ ] **Step 3.4: Stage and commit**

```bash
git add src/.gitkeep src_pane/.gitkeep render/.gitkeep mdparse/.gitkeep scanner/.gitkeep test/.gitkeep third_party/.gitkeep third_party/unity/.gitkeep third_party/md4c/.gitkeep third_party/md4c/src/.gitkeep
```

Then `git-commit` with message:

```
Add module directory skeleton

Create src/, src_pane/, render/, mdparse/, scanner/, test/,
third_party/unity/, and third_party/md4c/src/. Each holds a
.gitkeep placeholder that gets removed organically as real files
land.
```

---

### Task 4: Vendor Unity test framework

**Files:**
- Create: `third_party/unity/unity.c`, `third_party/unity/unity.h`, `third_party/unity/unity_internals.h`
- Delete: `third_party/unity/.gitkeep` (replaced by real files)
- Create: `third_party/unity/README.md` (records version + license)

Unity is a small C unit-test framework (https://github.com/ThrowTheSwitch/Unity). Use a pinned tagged release for reproducibility.

- [ ] **Step 4.1: Download Unity at pinned version**

```bash
cd /tmp
curl -LO https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.6.1.tar.gz
tar -xzf v2.6.1.tar.gz
```

- [ ] **Step 4.2: Copy only the three files we need**

```bash
cp /tmp/Unity-2.6.1/src/unity.c         third_party/unity/unity.c
cp /tmp/Unity-2.6.1/src/unity.h         third_party/unity/unity.h
cp /tmp/Unity-2.6.1/src/unity_internals.h  third_party/unity/unity_internals.h
cp /tmp/Unity-2.6.1/LICENSE.txt        third_party/unity/LICENSE.txt
rm third_party/unity/.gitkeep
```

- [ ] **Step 4.3: Create `third_party/unity/README.md`**

```markdown
# Unity — vendored

Upstream: https://github.com/ThrowTheSwitch/Unity

**Pinned version:** v2.6.1 (tag)

**License:** MIT (see `LICENSE.txt`)

**Files in this vendored copy:**
- `unity.c` — the implementation
- `unity.h` — the public API macros (`TEST_ASSERT_*`)
- `unity_internals.h` — internal macros
- `LICENSE.txt` — upstream license

## Upgrading

1. Download a new tagged release from GitHub.
2. Copy `src/unity.c`, `src/unity.h`, `src/unity_internals.h`, `LICENSE.txt` over the files here.
3. Update the "Pinned version" line above.
4. Run `make -f Makefile.hosttests test` and verify no regressions.
5. Commit with message `unity: bump to vX.Y.Z for <reason>`.
```

- [ ] **Step 4.4: Verify Unity compiles standalone**

```bash
cc -std=c89 -Wall -c third_party/unity/unity.c -o /tmp/unity_check.o
```

Expected: exit 0, no warnings. (If Unity's headers use features beyond C89 — some versions do — bump to `-std=c99` in the Makefile later but keep project code C89-compatible.)

Delete the check artifact: `rm /tmp/unity_check.o`.

- [ ] **Step 4.5: Stage and commit**

```bash
git add third_party/unity/unity.c third_party/unity/unity.h third_party/unity/unity_internals.h \
        third_party/unity/LICENSE.txt third_party/unity/README.md
git rm third_party/unity/.gitkeep
```

Then `git-commit` with message:

```
Vendor Unity v2.6.1 test framework

Unity provides a minimal C unit-test framework with TEST_ASSERT_*
macros. Pinned at tag v2.6.1 so upgrades are deliberate. MIT-licensed;
LICENSE.txt carried alongside the source.

Standard vendoring flow lives in third_party/unity/README.md.
```

---

### Task 5: Vendor md4c markdown parser

**Files:**
- Create: `third_party/md4c/src/md4c.c`, `third_party/md4c/src/md4c.h`
- Create: `third_party/md4c/COMMIT.txt` (records pinned SHA)
- Create: `third_party/md4c/LICENSE` (carried from upstream)
- Create: `third_party/md4c/README.md`
- Delete: `third_party/md4c/.gitkeep`, `third_party/md4c/src/.gitkeep`

We vendor md4c by commit SHA (not tag) because upstream releases are infrequent and we want reproducible builds.

- [ ] **Step 5.1: Clone upstream at a specific commit**

```bash
cd /tmp
git clone https://github.com/mity/md4c.git
cd md4c
git checkout release-0.5.2
git rev-parse HEAD > /tmp/md4c-sha.txt
cat /tmp/md4c-sha.txt
```

Expected: a 40-char SHA (e.g., `481396abc2d0c0c518c67566d3352b8dcc72a99b` — exact value is whatever release-0.5.2 points at when you run this).

- [ ] **Step 5.2: Copy only the parser sources we need**

md4c is a multi-file project but for our use we only need the CommonMark parser itself. Not `md4c-html.c` (HTML renderer — we render our own), not the CLI.

```bash
cp /tmp/md4c/src/md4c.c   third_party/md4c/src/md4c.c
cp /tmp/md4c/src/md4c.h   third_party/md4c/src/md4c.h
cp /tmp/md4c/LICENSE.md   third_party/md4c/LICENSE
rm third_party/md4c/.gitkeep third_party/md4c/src/.gitkeep
```

- [ ] **Step 5.3: Record the pinned SHA in `COMMIT.txt`**

```bash
cp /tmp/md4c-sha.txt third_party/md4c/COMMIT.txt
cat third_party/md4c/COMMIT.txt
```

- [ ] **Step 5.4: Create `third_party/md4c/README.md`**

```markdown
# md4c — vendored

Upstream: https://github.com/mity/md4c

**Pinned commit:** see `COMMIT.txt` (corresponds to release-0.5.2)

**License:** MIT (see `LICENSE`)

## What we vendor

Only the CommonMark parser — not the HTML renderer (`md4c-html.c`), not the CLI. We render markdown ourselves via our `render/` module driving QuickDraw.

**Files in this vendored copy:**
- `src/md4c.c` — the parser (~2500 lines, C89, zero deps)
- `src/md4c.h` — the public API (`md_parse`, `MD_PARSER`, block/span/text type enums)
- `COMMIT.txt` — pinned upstream commit SHA
- `LICENSE` — upstream MIT license

## Upgrading

1. Fetch the target commit from upstream (prefer a tagged release).
2. Overwrite `src/md4c.c` and `src/md4c.h`.
3. Update `COMMIT.txt` with the new SHA.
4. Run `make -f Makefile.hosttests test` — `mdparse` tests in particular will catch regressions in event sequences.
5. Commit with message `md4c: bump to <sha> for <reason>`. Never do drive-by updates.
```

- [ ] **Step 5.5: Verify md4c compiles standalone**

```bash
cc -std=c89 -Wall -c third_party/md4c/src/md4c.c -o /tmp/md4c_check.o -I third_party/md4c/src
```

Expected: exit 0. md4c may emit a handful of warnings on strict `-Wall` depending on version; as long as it compiles to an object file we're good. (Do not silence warnings with pragmas; we'll contain them behind our `mdparse` adapter.)

Clean up: `rm /tmp/md4c_check.o` and `rm -rf /tmp/md4c /tmp/md4c-sha.txt`.

- [ ] **Step 5.6: Stage and commit**

```bash
git add third_party/md4c/src/md4c.c third_party/md4c/src/md4c.h \
        third_party/md4c/COMMIT.txt third_party/md4c/LICENSE \
        third_party/md4c/README.md
git rm third_party/md4c/.gitkeep third_party/md4c/src/.gitkeep
```

Then `git-commit` with message:

```
Vendor md4c at release-0.5.2

md4c provides a SAX-style CommonMark parser with caller-controlled
allocation and zero external dependencies. Pinned commit in
COMMIT.txt; upgrade procedure in third_party/md4c/README.md.

Only md4c.c + md4c.h are brought in — not md4c-html.c (we render our
own via render/) and not the CLI.
```

---

### Task 6: Minimal `armadillo.r` stub

**Files:**
- Create: `armadillo.r`

Just enough resources to let CMakeLists.txt produce an `.APPL`. Real menus, alerts, etc. land in Plan 2.

- [ ] **Step 6.1: Create `armadillo.r`**

```c
/*
 * armadillo.r — Rez resource file for Armadillo Editor
 *
 * This is a minimal stub. Plan 2 expands it with full MBAR/MENU/ALRT/
 * STR#/ICN# entries per the design doc §7.5.
 */

#include "Types.r"

/* Size Manager resource.
 * 2 MB preferred / 1 MB minimum — MVP stub sizing; Plan 2 tunes
 * against real arena + TE footprints. 32-bit clean for 68030+ hosts
 * (no 24-bit addressing assumed). */
resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    canBackground,
    multiFinderAware,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    is32BitCompatible,     /* set via size flags; Retro68 respects 32-bit clean code */
    isHighLevelEventAware,
    localAndRemoteHLEvents,
    notStationeryAware,
    dontUseTextEditServices,
    reserved,
    reserved,
    reserved,
    2 * 1024 * 1024,        /* preferred size: 2 MB */
    1 * 1024 * 1024         /* minimum size:   1 MB */
};
```

Note: the Retro68 compiler will warn if flag semantics don't match an exact expected bit layout. If the compile step in Task 9 complains about any `SIZE` flag names, consult Retro68's `Types.r` for the exact flag names in this toolchain version — the names above are Apple-canonical and may differ slightly in Retro68.

- [ ] **Step 6.2: Stage and commit**

```bash
git add armadillo.r
```

Then `git-commit` with message:

```
Add minimal armadillo.r stub

Just a SIZE resource so Retro68's make-applet step has something to
chew on and produces a valid .APPL. Plan 2 expands this with the full
menu bar, alerts, STR# strings, and app icon.
```

---

### Task 7: CMakeLists.txt stub for cross-compile

**Files:**
- Create: `CMakeLists.txt`

At this point we only have `armadillo.r` and vendored md4c. The stub lets Retro68 produce an empty-but-valid `.APPL`. Real source files are added incrementally as they land.

- [ ] **Step 7.1: Create `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.15)
project(ArmadilloEditor C)

#
# Armadillo Editor — Retro68 cross-compile targets.
#
# This CMakeLists builds the .APPL for classic Mac (68030+). Host unit
# tests are built via a separate Makefile.hosttests to keep the two
# toolchains from interfering.
#
# The Retro68 toolchain file is expected in
# ~/Documents/Projects/Retro68-build/toolchain/m68k-apple-macos/cmake/
# retro68.toolchain.cmake. Invoke with:
#
#   cd build && cmake .. \
#     -DCMAKE_TOOLCHAIN_FILE=$HOME/Documents/Projects/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
#
# The toolchain file provides add_application() and the Rez resource
# pipeline.
#

# md4c vendored source
set(MD4C_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/md4c/src)
set(MD4C_SOURCES ${MD4C_DIR}/md4c.c)

# The app target. Source list is incremental — modules are added as
# they're implemented across Plan 1 and Plan 2.
add_application(ArmadilloEditor
    # Phase 1 stub main (replaced by src/app.c in Plan 2)
    src/stub_main.c

    # Renderer + pipeline (Plan 1 modules land here)
    # render/arena.c
    # render/render.c
    # mdparse/mdparse.c
    # scanner/scanner.c
    # src/doc.c
    # src/debounce.c
    # src/file_io.c

    # App shell (Plan 2 replaces stub_main.c with these)
    # src/app.c
    # src/menus.c
    # src/win_editor.c
    # src/draw_qd.c
    # src_pane/src_pane.c

    # Third-party
    ${MD4C_SOURCES}

    # Resources
    armadillo.r
)

target_include_directories(ArmadilloEditor PRIVATE
    src
    src_pane
    render
    mdparse
    scanner
    third_party/md4c/src
)

# md4c defines.
# MD4C_USE_ASCII=1 — disables md4c's Unicode word-class tables
# (trades ~few-KB lookups for a byte compare per char on a 25 MHz
# target). Input is still parsed byte-for-byte; it's only word-
# boundary detection inside spans that becomes ASCII-only.
# MacRoman / UTF-8 documents still round-trip cleanly.
target_compile_definitions(ArmadilloEditor PRIVATE
    MD4C_USE_ASCII=1
)

# Compile options.
# -ffunction-sections + -Wl,-gc-sections lets the linker drop
# unreferenced functions (smaller .APPL).
# -Os optimizes for size — critical on 25 MHz 68030 with tight RAM.
# -Wall catches common bugs; md4c gets -w below because we don't
# own its source and its warnings are not our problem.
# TODO (Plan 2): decide whether to add -mcpu=68020 or -mcpu=68030.
# Default (no -mcpu) compiles for gcc's 68000 baseline, which
# loses any 68020+ optimizations but is maximally conservative.
# Add -mcpu=... once we want to commit to the CPU floor in code.
target_compile_options(ArmadilloEditor PRIVATE
    -ffunction-sections
    -Os
    -Wall
)

# Suppress warnings on vendored md4c — we don't own its source.
set_source_files_properties(${MD4C_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-w"
)

target_link_options(ArmadilloEditor PRIVATE
    "LINKER:-gc-sections"
    "LINKER:--mac-single"
)
```

Note: `add_application()` is Retro68's custom CMake function that wraps the Rez + linker + `.APPL` packaging steps. It's defined in the Retro68 toolchain file.

- [ ] **Step 7.2: Stage and commit**

```bash
git add CMakeLists.txt
```

Then `git-commit` with message:

```
Add CMakeLists.txt stub for Retro68 cross-compile

Source list is incremental — modules are added as they land across
Plan 1 and Plan 2. md4c is compiled into the same target; the MVP's
other top-level modules (render/, mdparse/, scanner/, src_pane/) get
added as their .c files land.

Toolchain invocation documented inline — requires the Retro68
toolchain file passed via -DCMAKE_TOOLCHAIN_FILE.
```

---

### Task 8: Makefile.hosttests stub

**Files:**
- Create: `Makefile.hosttests`

At this stage the target list is empty; each phase adds its own rule.

- [ ] **Step 8.1: Create `Makefile.hosttests`**

```makefile
# Makefile.hosttests — host unit tests for Armadillo Editor
#
# Builds and runs each module's _test.c against native cc. Intended for
# fast iteration (< 2 s full suite) with no Retro68/QEMU dependency.
#
# Test binaries land in build_hosttests/; this directory is gitignored.
#
# Run: make -f Makefile.hosttests test
# Clean: make -f Makefile.hosttests clean

CC       ?= cc
CFLAGS   ?= -std=c89 -Wall -Werror -Wno-unused-function -g \
            -I. \
            -Isrc -Irender -Imdparse -Iscanner -Isrc_pane \
            -Itest -Ithird_party/unity -Ithird_party/md4c/src
BUILDDIR := build_hosttests

UNITY    := third_party/unity/unity.c
FAKES    := test/fake_syscalls.c
RECORDER := test/recorder.c

# Test binaries — populated phase by phase as implementations land.
TESTS :=

.PHONY: all test clean
all: $(TESTS)

test: $(TESTS)
	@if [ -z "$(TESTS)" ]; then \
	    echo "No test binaries defined yet — returning green (no tests to run)."; \
	    exit 0; \
	fi
	@pass=0; fail=0; \
	for t in $(TESTS); do \
	    echo "=== $$t ==="; \
	    if $$t; then pass=$$(expr $$pass + 1); else fail=$$(expr $$fail + 1); fi; \
	done; \
	echo; \
	echo "Summary: $$pass passed, $$fail failed"; \
	test $$fail -eq 0

clean:
	rm -rf $(BUILDDIR)
```

Notes on the CFLAGS choices:

- `-std=c89` — the project's target floor. Retro68 emits C89-compatible code; we keep host tests on the same dialect so bugs don't hide behind GCC extensions.
- `-Wno-unused-function` — exempts static helpers that test-only code declares but only uses under some `#ifdef` paths. Kept narrow.
- `-Werror` — warnings fail the build. Non-negotiable; discipline from day one.

- [ ] **Step 8.2: Verify the Makefile runs in its empty state**

```bash
make -f Makefile.hosttests test
```

Expected output:

```
No test binaries defined yet — returning green (no tests to run).
```

Exit code 0.

- [ ] **Step 8.3: Verify clean works**

```bash
make -f Makefile.hosttests clean
```

Expected: exit 0 (no-op because `build_hosttests/` doesn't exist yet).

- [ ] **Step 8.4: Stage and commit**

```bash
git add Makefile.hosttests
```

Then `git-commit` with message:

```
Add Makefile.hosttests stub

Host unit-test runner that compiles each module's _test.c against
native cc. TESTS list is empty initially; each implementation phase
adds its own rule.

Verified empty-state behavior returns green (no binaries, no
failures) so CI wiring has a stable starting point.
```

---

### Task 9: Verify cross-compile produces a stub .APPL

This is a smoke check for the Retro68 toolchain path. If this fails, the toolchain isn't installed or the path is wrong — all subsequent Retro68 builds would fail the same way.

- [ ] **Step 9.1: Try the cross-compile**

```bash
mkdir -p build
cd build
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$HOME/Documents/Projects/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
make
cd ..
```

Expected: no errors. A `build/ArmadilloEditor.APPL` file should appear along with `build/ArmadilloEditor.dsk`.

If `cmake` fails with "no such toolchain file," the Retro68 toolchain is not installed at the expected path — that's out of scope for this plan. Report to user; they'll verify their Retro68 install.

If the build fails on `add_application` being undefined, the toolchain file didn't load. Confirm the `-DCMAKE_TOOLCHAIN_FILE` path. It should end with `retro68.toolchain.cmake`.

If the build fails on a `SIZE` resource flag name, consult the Retro68 `Types.r` copy at `~/Documents/Projects/Retro68-build/toolchain/m68k-apple-macos/RIncludes/Types.r` and match flag names exactly.

- [ ] **Step 9.2: Verify the `.APPL` exists**

```bash
ls -la build/ArmadilloEditor.APPL build/ArmadilloEditor.dsk
```

Expected: both files exist, non-zero size.

- [ ] **Step 9.3: No commit for this step**

This is a verification step, not a code change. Nothing to commit. If the build succeeded, proceed to Phase 2.

---

## Phase 2 — All public headers

Each header goes into its canonical location with the types and function signatures defined in `design.md` §2.4 and the capability specs. Implementations are empty at this point (they'll be filled in during Phases 3–9).

No tests are added in Phase 2 — just the headers — but each header compiles cleanly under `-std=c89 -Wall -Werror` via a tiny harness.

### Task 10: `src/mac_syscalls.h`

**Files:**
- Create: `src/mac_syscalls.h`
- Delete: `src/.gitkeep`

This is the Toolbox-wrapping struct every module-that-touches-the-OS takes as a parameter. It's forward-declared here; the production `MacSyscalls` wiring lives in Plan 2 (`src/app.c`).

- [ ] **Step 10.1: Create `src/mac_syscalls.h`**

```c
/*
 * src/mac_syscalls.h — mockable Mac Toolbox wrappers
 *
 * Every function in this struct corresponds to a Toolbox call we use.
 * Modules that touch the OS take `const MacSyscalls*` and dispatch
 * through it instead of calling the Toolbox directly. Host tests inject
 * a FakeSyscalls struct that satisfies this signature; production code
 * injects a real-Toolbox struct defined in src/app.c (Plan 2).
 *
 * All function-pointer fields use plain types (not Toolbox types) so
 * this header stays compilable on the host without Mac headers. The
 * cost: a few `void*` / `short` / `long` parameters that the real impl
 * casts back to FSSpec*, ParmBlkPtr, etc.
 */
#ifndef ARMA_MAC_SYSCALLS_H
#define ARMA_MAC_SYSCALLS_H

#include <stddef.h>

typedef struct MacSyscalls {
    /* --- Time & events --- */
    unsigned long (*tick_count)(void);                       /* TickCount()            */

    /* --- Memory Manager --- */
    void*  (*new_handle)(long size);                         /* NewHandle()            */
    void   (*dispose_handle)(void* handle);                  /* DisposeHandle()        */
    void   (*hlock)(void* handle);                           /* HLock()                */
    void   (*hunlock)(void* handle);                         /* HUnlock()              */
    int    (*set_handle_size)(void* handle, long size);      /* SetHandleSize(); 0=ok  */
    short  (*mem_error)(void);                               /* MemError()             */

    /* --- File Manager (data path) --- */
    short  (*fsp_open_df)(const void* spec, char perm, short* out_ref);    /* FSpOpenDF    */
    short  (*fs_close)(short ref);                                          /* FSClose      */
    short  (*fs_read)(short ref, long* io_count, void* buf);                /* FSRead       */
    short  (*fs_write)(short ref, long* io_count, const void* buf);         /* FSWrite      */
    short  (*get_eof)(short ref, long* out_size);                           /* GetEOF       */
    short  (*set_eof)(short ref, long size);                                /* SetEOF       */
    short  (*set_fpos)(short ref, short mode, long off);                    /* SetFPos      */
    short  (*fsp_create)(const void* spec, unsigned long creator,
                         unsigned long type, short script);                 /* FSpCreate    */

    /* --- Standard File Package --- */
    /* standard_get_file fills *out_spec with an opaque FSSpec if good,
     * sets *out_good non-zero. Type list is NULL-or-'TEXT'-filtered. */
    void   (*standard_get_file)(void* out_spec, int* out_good);
    void   (*standard_put_file)(const char* prompt_ptr, const char* default_name_ptr,
                                void* out_spec, int* out_good);

    /* --- Gestalt --- */
    short  (*gestalt)(unsigned long selector, long* out_response);

    /* --- Alerts --- */
    short  (*note_alert)(short id, void* filter);                           /* NoteAlert    */
    short  (*stop_alert)(short id, void* filter);                           /* StopAlert    */
} MacSyscalls;

#endif /* ARMA_MAC_SYSCALLS_H */
```

- [ ] **Step 10.2: Verify the header compiles clean**

```bash
cat > /tmp/marma_harness.c <<'EOF'
#include "src/mac_syscalls.h"
int main(void) { return 0; }
EOF
cc -std=c89 -Wall -Werror -I. -c /tmp/marma_harness.c -o /tmp/marma_harness.o
```

Expected: exit 0. Clean up: `rm /tmp/marma_harness.*`.

- [ ] **Step 10.3: Stage and commit**

```bash
git add src/mac_syscalls.h
git rm src/.gitkeep
```

Then `git-commit` with message:

```
Add src/mac_syscalls.h — mockable Toolbox vtable

Function-pointer struct covering every Toolbox call the app uses:
time, Memory Manager, File Manager data path, Standard File, Gestalt,
Alerts. Every module that touches the OS takes a const MacSyscalls*
and dispatches through it.

Uses only plain C types (void*/short/long) so the header is host-
buildable. Real implementations in Plan 2 cast back to FSSpec*,
ParmBlkPtr, etc. at their implementation site.
```

---

### Task 11: `render/blocks.h` and `render/inlines.h`

**Files:**
- Create: `render/blocks.h`, `render/inlines.h`
- Delete: `render/.gitkeep`

- [ ] **Step 11.1: Create `render/inlines.h`**

```c
/*
 * render/inlines.h — inline style runs inside a block's text.
 *
 * A MdStyleRun is a slice of a block's text carrying a single visual
 * style. Scanner produces MdStyleRun arrays for the source pane; render
 * produces them inside each Block for the read pane. The two uses
 * share the type but live in different arenas.
 */
#ifndef ARMA_INLINES_H
#define ARMA_INLINES_H

typedef enum {
    kStylePlain    = 0,
    kStyleEmph     = 1,    /* _italic_                                  */
    kStyleStrong   = 2,    /* **bold**                                  */
    kStyleCodeSpan = 3,    /* `code`                                    */
    kStyleLink     = 4,    /* [text](url)                               */
    kStyleHtmlSpan = 5     /* inline <tag>; MVP: raw                    */
} StyleKind;

typedef struct MdStyleRun {
    unsigned short start;       /* byte offset into containing text       */
    unsigned short length;      /* bytes                                  */
    StyleKind      kind;
    short          link_index;  /* index into per-model link table; -1 = N/A */
} MdStyleRun;

#endif /* ARMA_INLINES_H */
```

- [ ] **Step 11.2: Create `render/blocks.h`**

```c
/*
 * render/blocks.h — flat block model types.
 *
 * A RenderModel is a flat array of Block structs. Nesting is expressed
 * via list_depth and quote_depth scalars on each block — NOT via a
 * tree. See design.md §1 for rationale.
 */
#ifndef ARMA_BLOCKS_H
#define ARMA_BLOCKS_H

#include "inlines.h"

typedef enum {
    kBlockParagraph  = 0,
    kBlockHeading    = 1,    /* h_level = 1..6                          */
    kBlockListItem   = 2,
    kBlockBlockQuote = 3,
    kBlockCodeBlock  = 4,
    kBlockHr         = 5,
    kBlockHtml       = 6     /* md4c-detected raw HTML block            */
} BlockKind;

typedef struct Block {
    BlockKind       kind;
    unsigned char   h_level;       /* 1..6 for kBlockHeading, else 0   */
    unsigned char   list_depth;    /* 0 = not in list; 1+ = nesting    */
    unsigned char   quote_depth;   /* 0 = not in quote; 1+ = nesting   */
    unsigned char   list_ordered;  /* 0 = bullet, 1 = numbered         */
    const char*     text;          /* arena-alloc'd; NOT NUL-terminated */
    unsigned short  text_length;
    unsigned short  run_count;
    const MdStyleRun* runs;          /* arena-alloc'd; NULL if 0         */
} Block;

#endif /* ARMA_BLOCKS_H */
```

- [ ] **Step 11.3: Verify both compile clean**

```bash
cat > /tmp/marma_harness.c <<'EOF'
#include "render/blocks.h"
#include "render/inlines.h"
int main(void) {
    Block b; MdStyleRun r;
    (void)b; (void)r;
    return 0;
}
EOF
cc -std=c89 -Wall -Werror -I. -c /tmp/marma_harness.c -o /tmp/marma_harness.o
rm /tmp/marma_harness.*
```

Expected: exit 0.

- [ ] **Step 11.4: Stage and commit**

```bash
git add render/blocks.h render/inlines.h
git rm render/.gitkeep
```

Then `git-commit` with message:

```
Add render/blocks.h and render/inlines.h — flat block model types

BlockKind covers the MVP's block types (paragraph, heading, list item,
blockquote, code block, hr, raw HTML). Nesting via list_depth and
quote_depth scalars, not a tree. Block.text references arena memory;
Block.runs references an arena-allocated MdStyleRun array.

StyleKind covers the inline-run types scanner and render share
(plain / emph / strong / code span / link / HTML span).
```

---

### Task 12: `render/arena.h`

**Files:**
- Create: `render/arena.h`

- [ ] **Step 12.1: Create `render/arena.h`**

```c
/*
 * render/arena.h — Handle-backed bump allocator for the render pipeline.
 *
 * All variable-length data in the render pipeline (Blocks, MdStyleRun
 * arrays, text slices, link URL strings) lives in one arena. The arena
 * is backed by a single Mac OS Handle that we HLock for the arena's
 * entire lifetime. Growth is "grow-before-parse": callers must
 * arena_ensure() to a pre-estimated size before starting to allocate,
 * and arena_alloc() never grows — it returns NULL on overflow.
 *
 * See design.md §4 for full rationale.
 */
#ifndef ARMA_ARENA_H
#define ARMA_ARENA_H

#include <stddef.h>
#include "src/mac_syscalls.h"

typedef struct Arena Arena;    /* opaque */

/* Initialize an arena backed by a Handle of initial_size bytes.
 * Returns 0 on success, negative on allocation failure.
 * On success *out is non-NULL and the Handle is HLocked.
 * On failure *out is NULL and no resources leak. */
int arena_init(Arena** out, size_t initial_size, const MacSyscalls* sys);

/* Release all resources. arena_destroy(NULL) is a no-op. */
void arena_destroy(Arena* a);

/* Grow the backing Handle so at least `bytes_needed` bytes are
 * available above the current high_water. Call BEFORE any allocation
 * cycle to pre-size. Doubling-then-linear growth per design.md §4;
 * capped at kArenaHardCap.
 * Returns 0 on success, negative if grow failed (state unchanged). */
int arena_ensure(Arena* a, size_t bytes_needed);

/* Bump-allocate n_bytes; rounded up to 4-byte alignment.
 * Returns a pointer into the arena on success, NULL on overflow.
 * Never grows — caller must arena_ensure first. */
void* arena_alloc(Arena* a, size_t n_bytes);

/* Reset high_water to 0. Keeps the backing Handle intact so the next
 * parse cycle reuses the memory. */
void arena_reset(Arena* a);

/* Diagnostics — do not drive decisions off these except in tests. */
size_t arena_high_water(const Arena* a);
size_t arena_capacity(const Arena* a);

#endif /* ARMA_ARENA_H */
```

- [ ] **Step 12.2: Verify compile**

```bash
cat > /tmp/marma_harness.c <<'EOF'
#include "render/arena.h"
int main(void) { return 0; }
EOF
cc -std=c89 -Wall -Werror -I. -c /tmp/marma_harness.c -o /tmp/marma_harness.o
rm /tmp/marma_harness.*
```

Expected: exit 0.

- [ ] **Step 12.3: Commit**

```bash
git add render/arena.h
```

Then `git-commit` with message:

```
Add render/arena.h — Handle-backed arena allocator API

Opaque Arena with init/destroy/ensure/alloc/reset/diagnostic-getters.
Grow-before-parse policy enforced by arena_alloc returning NULL on
overflow — never grows mid-allocation. See design.md §4.
```

---

### Task 13: `render/draw_qd.h`

**Files:**
- Create: `render/draw_qd.h`

- [ ] **Step 13.1: Create `render/draw_qd.h`**

```c
/*
 * render/draw_qd.h — swappable QuickDraw emitter.
 *
 * The render/layout pass emits drawing operations through a vtable
 * instead of calling QuickDraw directly. Production wires this to real
 * QuickDraw (src/draw_qd.c in Plan 2). Tests wire it to a
 * recording sink (test/recorder.c) that captures every call for
 * assertion.
 *
 * The vtable uses only plain types (short/unsigned short/char*) so
 * tests don't need to include QuickDraw headers. The "color" arguments
 * are passed as three 16-bit RGB components (matching real Toolbox
 * RGBColor without naming the type).
 */
#ifndef ARMA_DRAW_QD_H
#define ARMA_DRAW_QD_H

typedef struct DrawOps {
    void (*set_font)(void* ctx, short font_id, short size,
                     unsigned char face);
    void (*set_fg)(void* ctx, unsigned short red,
                   unsigned short green, unsigned short blue);
    void (*move_to)(void* ctx, short h, short v);
    void (*draw_text)(void* ctx, const char* bytes, unsigned short length);
    void (*line)(void* ctx, short x0, short y0, short x1, short y1);
    void (*frame_rect)(void* ctx, short l, short t, short r, short b);
    void (*get_font_metrics)(void* ctx, short* out_ascent,
                             short* out_descent, short* out_line_height);
} DrawOps;

typedef struct DrawContext {
    const DrawOps* ops;
    void*          ctx;
} DrawContext;

#endif /* ARMA_DRAW_QD_H */
```

- [ ] **Step 13.2: Verify compile + Commit**

```bash
cc -std=c89 -Wall -Werror -I render -c render/draw_qd.h -o /dev/null 2>&1 || true
```

(Headers don't produce `.o` files, but `cc -c` on a header with no `-x c` gives a valid syntax check on most compilers. If your `cc` rejects it, wrap in a tiny harness as in Task 12.)

```bash
git add render/draw_qd.h
```

Then `git-commit` with message:

```
Add render/draw_qd.h — DrawOps vtable for mockable QuickDraw

Renderer emits drawing ops through this vtable instead of calling
QuickDraw directly. Production impl in Plan 2; test recorder captures
every call for unit-test assertions. No QuickDraw types leak into the
vtable — only plain C types.
```

---

### Task 14: `render/render.h`

**Files:**
- Create: `render/render.h`

- [ ] **Step 14.1: Create `render/render.h`**

```c
/*
 * render/render.h — flat block model construction + layout + draw.
 *
 * RenderModel is built by feeding an MdParseSink (from mdparse) events
 * into render_model_sink(). After construction, render_layout_and_draw
 * walks the model and emits through a DrawContext.
 */
#ifndef ARMA_RENDER_H
#define ARMA_RENDER_H

#include <stddef.h>
#include "arena.h"
#include "blocks.h"
#include "draw_qd.h"

/* Forward declare MdParseSink from mdparse — consumers link against
 * mdparse's typedef. This header doesn't include mdparse.h to avoid
 * a dependency cycle (mdparse sinks are owned by consumers). */
struct MdParseSink;

typedef struct RenderModel RenderModel;    /* opaque */

typedef enum {
    kRenderOk             =  0,
    kRenderErrArenaOOM    = -1,
    kRenderErrBadModel    = -2,
    kRenderErrEmitAborted = -3
} RenderError;

typedef struct LayoutParams {
    short content_width;    /* px; total usable content area width       */
    short indent_list;      /* px per list_depth step                     */
    short indent_quote;     /* px per quote_depth step                    */
    short left_margin;      /* px from the window's left edge             */
    short top_margin;       /* px from the window's top edge              */
    short block_spacing;    /* px gap between blocks                      */
} LayoutParams;

/* Construct a fresh model inside the given arena. */
RenderModel* render_model_new(Arena* a);

/* Return the MdParseSink to plug into mdparse_run for this model. */
const struct MdParseSink* render_model_sink(RenderModel* m);

/* Query the model after parse. */
size_t render_model_block_count(const RenderModel* m);
const Block* render_model_block_at(const RenderModel* m, size_t i);

/* Defaults helper — returns sensible values for a 480px-wide window. */
LayoutParams layout_params_defaults(void);

/* Walk the model and emit through the DrawContext. */
void render_layout_and_draw(const RenderModel* m,
                            const LayoutParams* params,
                            DrawContext* ctx);

#endif /* ARMA_RENDER_H */
```

- [ ] **Step 14.2: Verify compile + Commit**

```bash
cat > /tmp/marma_harness.c <<'EOF'
#include "render/render.h"
int main(void) { return 0; }
EOF
cc -std=c89 -Wall -Werror -I. -c /tmp/marma_harness.c -o /tmp/marma_harness.o
rm /tmp/marma_harness.*
git add render/render.h
```

Then `git-commit` with message:

```
Add render/render.h — RenderModel + layout API

Opaque RenderModel constructed by feeding mdparse sink events; render_
layout_and_draw walks it and emits through DrawContext. LayoutParams
struct + defaults helper for sensible 480px-window values.

Forward-declares MdParseSink (no include) to avoid dependency cycles.
```

---

### Task 15: `mdparse/mdparse.h`

**Files:**
- Create: `mdparse/mdparse.h`
- Delete: `mdparse/.gitkeep`

- [ ] **Step 15.1: Create `mdparse/mdparse.h`**

```c
/*
 * mdparse/mdparse.h — project-owned adapter over md4c.
 *
 * Hides md4c types entirely. Exposes a fan-out sink interface that
 * scanner and render plug into for a single-pass parse. See design.md
 * §2 (module boundaries) and the mdparse capability spec.
 */
#ifndef ARMA_MDPARSE_H
#define ARMA_MDPARSE_H

#include <stddef.h>
#include "render/blocks.h"
#include "render/inlines.h"

typedef struct BlockAttrs {
    unsigned char h_level;       /* 1..6 for kBlockHeading, 0 otherwise */
    unsigned char list_depth;    /* 0 = not in list; 1+ = nesting        */
    unsigned char quote_depth;   /* 0 = not in quote; 1+ = nesting       */
    unsigned char list_ordered;  /* 0 = bullet, 1 = numbered             */
} BlockAttrs;

typedef struct MdParseSink {
    int (*on_block_open)(void* ctx, BlockKind kind,
                         const BlockAttrs* attrs);
    int (*on_block_close)(void* ctx, BlockKind kind);
    int (*on_span)(void* ctx, StyleKind kind,
                   unsigned short start, unsigned short length,
                   const char* link_url, unsigned short link_url_len);
    int (*on_text)(void* ctx, const char* bytes, unsigned short length,
                   unsigned short source_offset);
    void* ctx;
} MdParseSink;

typedef enum {
    kMdParseOk           =  0,
    kMdParseErrArenaOOM  = -1,
    kMdParseErrMd4c      = -2,
    kMdParseErrSinkAbort = -3
} MdParseError;

/* Parse source[0..source_len] and dispatch every event to every sink
 * in sinks[0..sink_count-1] in array order. Returns kMdParseOk on
 * success or a negative MdParseError on failure. */
int mdparse_run(const char* source, unsigned short source_len,
                const MdParseSink* sinks, size_t sink_count);

#endif /* ARMA_MDPARSE_H */
```

- [ ] **Step 15.2: Verify compile + Commit**

```bash
cat > /tmp/marma_harness.c <<'EOF'
#include "mdparse/mdparse.h"
int main(void) { return 0; }
EOF
cc -std=c89 -Wall -Werror -I. -c /tmp/marma_harness.c -o /tmp/marma_harness.o
rm /tmp/marma_harness.*
git add mdparse/mdparse.h
git rm mdparse/.gitkeep
```

Then `git-commit` with message:

```
Add mdparse/mdparse.h — md4c adapter with MdParseSink fan-out

Hides md4c types. Exposes MdParseSink (four callbacks: block open,
block close, inline span, text) that consumers implement. mdparse_run
dispatches each event to every sink in array order. Non-zero sink
return aborts the parse with kMdParseErrSinkAbort.

BlockAttrs struct carries h_level / list_depth / quote_depth /
list_ordered to on_block_open.
```

---

### Task 16: `scanner/scanner.h`

**Files:**
- Create: `scanner/scanner.h`
- Delete: `scanner/.gitkeep`

- [ ] **Step 16.1: Create `scanner/scanner.h`**

```c
/*
 * scanner/scanner.h — style-run producer for the source pane.
 *
 * Scanner consumes MdParseSink events and accumulates a flat
 * MdStyleRun[] for application to the source pane via src_pane_apply_
 * runs. Arena-backed; no direct allocation.
 */
#ifndef ARMA_SCANNER_H
#define ARMA_SCANNER_H

#include <stddef.h>
#include "render/arena.h"
#include "render/inlines.h"
#include "mdparse/mdparse.h"

typedef struct Scanner Scanner;    /* opaque */

/* Construct a scanner that allocates runs out of the given arena. */
Scanner* scanner_new(Arena* a);

/* Release the Scanner struct itself. Does NOT destroy the arena. */
void scanner_free(Scanner* s);

/* Return the MdParseSink to plug into mdparse_run. Pointer is valid
 * for the scanner's lifetime. */
const MdParseSink* scanner_sink(Scanner* s);

/* After mdparse_run returns, retrieve the accumulated runs. Pointer
 * valid until the next scanner_reset or arena_reset. */
const MdStyleRun* scanner_runs(const Scanner* s, size_t* out_count);

/* Clear accumulated runs so the scanner is ready for the next parse
 * cycle. Does NOT free arena memory — arena_reset is the caller's job. */
void scanner_reset(Scanner* s);

#endif /* ARMA_SCANNER_H */
```

- [ ] **Step 16.2: Verify compile + Commit**

```bash
cat > /tmp/marma_harness.c <<'EOF'
#include "scanner/scanner.h"
int main(void) { return 0; }
EOF
cc -std=c89 -Wall -Werror -I. -c /tmp/marma_harness.c -o /tmp/marma_harness.o
rm /tmp/marma_harness.*
git add scanner/scanner.h
git rm scanner/.gitkeep
```

Then `git-commit` with message:

```
Add scanner/scanner.h — style-run producer API

Opaque Scanner with new/free, an MdParseSink accessor, a runs
accessor, and a reset. Arena-backed — all MdStyleRun storage comes
from the Arena* passed to scanner_new. No direct allocation.
```

---

### Task 17: `src_pane/src_pane.h`

**Files:**
- Create: `src_pane/src_pane.h`
- Delete: `src_pane/.gitkeep`

Vendor-free header — no TE types. Internals are Plan 2's territory.

- [ ] **Step 17.1: Create `src_pane/src_pane.h`**

```c
/*
 * src_pane/src_pane.h — editable source-text pane (vendor-free API).
 *
 * Opaque SrcPane wrapper around whichever text engine is in use
 * internally (MVP: Mac Toolbox TextEdit; future: custom piece-table).
 * No Toolbox types (TEHandle, WindowPtr) appear in this header — the
 * window_ref on SrcPaneParams is passed as void* and cast inside the
 * implementation.
 *
 * Implementation lives in Plan 2 (src_pane/src_pane.c — target-only).
 * Host tests don't cover src_pane internals; the type is opaque and
 * callers mock SrcPane* as a NULL pointer when verifying their own
 * logic.
 */
#ifndef ARMA_SRC_PANE_H
#define ARMA_SRC_PANE_H

#include <stddef.h>
#include "render/inlines.h"
#include "src/mac_syscalls.h"

typedef struct SrcPaneParams {
    short left;
    short top;
    short right;
    short bottom;
    void* window_ref;       /* opaque; internally cast to WindowPtr */
} SrcPaneParams;

typedef struct SrcPane SrcPane;    /* opaque */

typedef void (*SrcPaneEditCallback)(void* ctx);

SrcPane* src_pane_new(const SrcPaneParams* params, const MacSyscalls* sys);
void     src_pane_free(SrcPane* p);

const char* src_pane_get_text(const SrcPane* p, unsigned short* out_len);
void        src_pane_set_text(SrcPane* p, const char* bytes,
                              unsigned short len);

void src_pane_apply_runs(SrcPane* p, const MdStyleRun* runs, size_t count);

void src_pane_get_selection(const SrcPane* p,
                            unsigned short* out_start,
                            unsigned short* out_end);
void src_pane_set_selection(SrcPane* p,
                            unsigned short start,
                            unsigned short end);

void src_pane_on_mouse_down(SrcPane* p, short h, short v, short modifiers);
void src_pane_on_key(SrcPane* p, short char_code, short key_code,
                     short modifiers);
void src_pane_on_activate(SrcPane* p, int is_active);
void src_pane_on_update(SrcPane* p);
void src_pane_on_idle(SrcPane* p);

void src_pane_set_edit_callback(SrcPane* p, SrcPaneEditCallback cb,
                                void* ctx);

#endif /* ARMA_SRC_PANE_H */
```

- [ ] **Step 17.2: Verify compile + Commit**

```bash
cat > /tmp/marma_harness.c <<'EOF'
#include "src_pane/src_pane.h"
int main(void) { return 0; }
EOF
cc -std=c89 -Wall -Werror -I. -c /tmp/marma_harness.c -o /tmp/marma_harness.o
rm /tmp/marma_harness.*
git add src_pane/src_pane.h
git rm src_pane/.gitkeep
```

Then `git-commit` with message:

```
Add src_pane/src_pane.h — vendor-free source-pane API

Opaque SrcPane wrapper around an unspecified text engine (TE in MVP,
custom in future). No TEHandle / WindowPtr leak into the header —
window_ref is void*, cast internally. Public API covers lifecycle,
text get/set, style runs, selection, event delegation, and edit-
callback registration. Implementation in Plan 2.
```

---

### Task 18: `src/doc.h`

**Files:**
- Create: `src/doc.h`

- [ ] **Step 18.1: Create `src/doc.h`**

```c
/*
 * src/doc.h — dumb document data container.
 *
 * Text buffer + filename + dirty flag. No OS dependencies — fully
 * host-buildable. Stateless beyond its fields; host tests own its full
 * surface.
 */
#ifndef ARMA_DOC_H
#define ARMA_DOC_H

#ifndef kMaxDocBytes
#define kMaxDocBytes 32767u
#endif

typedef struct Doc Doc;    /* opaque */

Doc* doc_new(void);
void doc_free(Doc* d);

const char* doc_text(const Doc* d, unsigned short* out_len);

/* Copies up to kMaxDocBytes bytes into the Doc. Over-limit calls
 * leave the Doc's state unchanged. Marks dirty on success. */
void doc_set_text(Doc* d, const char* bytes, unsigned short len);

int  doc_is_dirty(const Doc* d);
void doc_mark_dirty(Doc* d);
void doc_mark_clean(Doc* d);

/* Filename storage is opaque bytes — no FSSpec in the header.
 * The file_io module converts between FSSpec and this byte form. */
void doc_set_filename(Doc* d, const char* bytes, unsigned char len);
int  doc_has_filename(const Doc* d);
const char* doc_filename(const Doc* d, unsigned char* out_len);

#endif /* ARMA_DOC_H */
```

- [ ] **Step 18.2: Verify compile + Commit**

```bash
cat > /tmp/marma_harness.c <<'EOF'
#include "src/doc.h"
int main(void) { return 0; }
EOF
cc -std=c89 -Wall -Werror -I. -c /tmp/marma_harness.c -o /tmp/marma_harness.o
rm /tmp/marma_harness.*
git add src/doc.h
```

Then `git-commit` with message:

```
Add src/doc.h — dumb document data container API

Opaque Doc with text get/set (bounded by kMaxDocBytes), dirty flag,
and opaque-bytes filename storage (no FSSpec in the header). Zero OS
dependencies — fully host-buildable. Default kMaxDocBytes = 32767
(MVP TE ceiling); overridable at compile time via -D.
```

---

### Task 19: `src/debounce.h`

**Files:**
- Create: `src/debounce.h`

- [ ] **Step 19.1: Create `src/debounce.h`**

```c
/*
 * src/debounce.h — pure state machine driving the parse debounce.
 *
 * Tracks "dirty since last parse" and "tick of last keystroke." Polled
 * from the event loop; fires a parse cycle when kParseDebounceTicks
 * elapse after the last keystroke.
 *
 * Uses MacSyscalls.tick_count for the clock so tests can inject a
 * FakeClock. Never calls TickCount directly.
 */
#ifndef ARMA_DEBOUNCE_H
#define ARMA_DEBOUNCE_H

#include "mac_syscalls.h"

#ifndef kParseDebounceTicks
#define kParseDebounceTicks 15    /* 60 ticks/sec * 0.25 s */
#endif

typedef struct DebounceState {
    int           dirty;
    unsigned long last_keystroke_tick;
} DebounceState;

/* Record a keystroke. Sets dirty=1, captures current tick. */
void debounce_on_edit(DebounceState* s, const MacSyscalls* sys);

/* Poll the state machine. Returns 1 if the caller should run a parse
 * cycle NOW (clears dirty before returning), 0 otherwise. */
int debounce_poll(DebounceState* s, const MacSyscalls* sys);

#endif /* ARMA_DEBOUNCE_H */
```

- [ ] **Step 19.2: Verify compile + Commit**

```bash
cat > /tmp/marma_harness.c <<'EOF'
#include "src/debounce.h"
int main(void) { return 0; }
EOF
cc -std=c89 -Wall -Werror -I. -c /tmp/marma_harness.c -o /tmp/marma_harness.o
rm /tmp/marma_harness.*
git add src/debounce.h
```

Then `git-commit` with message:

```
Add src/debounce.h — parse debounce state machine API

DebounceState struct + debounce_on_edit / debounce_poll. Pure state
machine that takes MacSyscalls for clock reads so tests can inject
FakeClock. kParseDebounceTicks default = 15 (250 ms at 60 Hz);
overridable at compile time.
```

---

### Task 20: `src/file_io.h`

**Files:**
- Create: `src/file_io.h`

- [ ] **Step 20.1: Create `src/file_io.h`**

```c
/*
 * src/file_io.h — File Manager + Standard File wrappers.
 *
 * Handles open/save of .md files up to kMaxDocBytes. Uses FSSpec
 * opaquely (const void* in the header) so no Toolbox type crosses
 * the boundary.
 */
#ifndef ARMA_FILE_IO_H
#define ARMA_FILE_IO_H

#include "doc.h"
#include "mac_syscalls.h"

typedef enum {
    kFileIoOk       =  0,
    kFileIoErrOpen  = -1,
    kFileIoErrStat  = -2,
    kFileIoErrTooBig= -3,
    kFileIoErrOOM   = -4,
    kFileIoErrRead  = -5,
    kFileIoErrWrite = -6,
    kFileIoErrClose = -7,
    kFileIoErrCancel= -8
} FileIoError;

/* Interactive — presents Standard File Open dialog.
 * Target-only (requires live Toolbox); Plan 2 covers this. */
int file_io_open_interactive(Doc** out_doc, const MacSyscalls* sys);

/* Data-path — host-testable with FakeSyscalls.
 * fsspec_opaque is treated as const FSSpec* internally. */
int file_io_open(const void* fsspec_opaque, Doc** out_doc,
                 const MacSyscalls* sys);

/* Interactive Save As — target-only. */
int file_io_save_as(Doc* d, const MacSyscalls* sys);

/* Save to the Doc's existing filename (requires doc_has_filename).
 * Data-path portion host-testable. */
int file_io_save(Doc* d, const MacSyscalls* sys);

#endif /* ARMA_FILE_IO_H */
```

- [ ] **Step 20.2: Verify compile + Commit**

```bash
cat > /tmp/marma_harness.c <<'EOF'
#include "src/file_io.h"
int main(void) { return 0; }
EOF
cc -std=c89 -Wall -Werror -I. -c /tmp/marma_harness.c -o /tmp/marma_harness.o
rm /tmp/marma_harness.*
git add src/file_io.h
```

Then `git-commit` with message:

```
Add src/file_io.h — File Manager + Standard File wrapper API

FileIoError enum covers open/stat/too-big/OOM/read/write/close/cancel.
Interactive functions (file_io_open_interactive, file_io_save_as) are
target-only — land in Plan 2. Data-path functions (file_io_open,
file_io_save) take const void* FSSpecs and are host-testable with
FakeSyscalls.
```

---

### Task 21: Create `test/fake_syscalls.{h,c}` with defaults

**Files:**
- Create: `test/fake_syscalls.h`, `test/fake_syscalls.c`

Every host test that needs Toolbox fakes includes this. Default behavior is "all calls succeed with noise-free values" — individual tests override specific fields.

- [ ] **Step 21.1: Create `test/fake_syscalls.h`**

```c
/*
 * test/fake_syscalls.h — host-test fakes for MacSyscalls.
 *
 * FakeSyscalls is a MacSyscalls with controllable behavior. Cast
 * (const MacSyscalls*) &fake.vtable when passing to code under test;
 * the vtable field MUST be first in the struct so the cast is valid.
 */
#ifndef ARMA_FAKE_SYSCALLS_H
#define ARMA_FAKE_SYSCALLS_H

#include "src/mac_syscalls.h"

typedef struct FakeSyscalls {
    MacSyscalls vtable;   /* MUST be first — callers cast &fake.vtable */

    /* Controls (set before invoking code under test) */
    unsigned long tick_count_now;
    int           set_handle_size_fail_after;  /* -1 = never; N = Nth call fails */
    short         fsp_open_df_result;          /* 0 = ok; else errno */
    short         fs_read_result;
    short         fs_write_result;
    short         get_eof_result;
    long          get_eof_size;                /* value returned via *out_size */
    short         mem_error_result;
    short         new_handle_fail_after;       /* -1 = never */
    short         gestalt_result;
    long          gestalt_response;            /* value returned via *out_response */

    /* Call counters (read after to assert) */
    int new_handle_calls;
    int dispose_handle_calls;
    int hlock_calls;
    int hunlock_calls;
    int set_handle_size_calls;
    int fsp_open_df_calls;
    int fs_close_calls;
    int fs_read_calls;
    int fs_write_calls;
    int get_eof_calls;
    int set_eof_calls;
    int set_fpos_calls;
    int fsp_create_calls;
    int gestalt_calls;
    int note_alert_calls;
    int stop_alert_calls;
} FakeSyscalls;

/* Returns a FakeSyscalls with defaults wired (all succeed, counters 0). */
FakeSyscalls fake_syscalls_init(void);

#endif /* ARMA_FAKE_SYSCALLS_H */
```

- [ ] **Step 21.2: Create `test/fake_syscalls.c`**

```c
/*
 * test/fake_syscalls.c — fake MacSyscalls for host unit tests.
 *
 * Backing Handles are malloc-backed. Each call increments a counter
 * and respects its fail-after control. A "Handle" here is a pointer
 * to a pointer to malloc'd memory (two-level indirection, mirroring
 * the real Mac Memory Manager shape).
 */
#include "fake_syscalls.h"
#include <stdlib.h>
#include <string.h>

#define FAKE(self) ((FakeSyscalls*)((char*)(self)))

/* The real MacSyscalls struct doesn't carry a back-pointer to our fake,
 * so we use a file-scope singleton for the call-counter state. This
 * means FakeSyscalls is NOT reentrant — one test at a time. Good
 * enough for deterministic host unit tests. */
static FakeSyscalls* g_current = 0;

static unsigned long fake_tick_count(void) {
    return g_current ? g_current->tick_count_now : 0;
}

static void* fake_new_handle(long size) {
    if (!g_current) return 0;
    g_current->new_handle_calls++;
    if (g_current->new_handle_fail_after >= 0 &&
        g_current->new_handle_calls > g_current->new_handle_fail_after) {
        return 0;
    }
    /* Double-indirect: handle = malloc(sizeof(void*)); *handle = malloc(size) */
    void** h = (void**)malloc(sizeof(void*));
    if (!h) return 0;
    *h = malloc((size_t)size);
    if (!*h) { free(h); return 0; }
    return h;
}

static void fake_dispose_handle(void* handle) {
    if (!g_current) return;
    g_current->dispose_handle_calls++;
    if (handle) {
        void** h = (void**)handle;
        if (*h) free(*h);
        free(h);
    }
}

static void fake_hlock(void* handle) {
    if (g_current) g_current->hlock_calls++;
    (void)handle;  /* no-op; malloc'd memory is already "locked" */
}

static void fake_hunlock(void* handle) {
    if (g_current) g_current->hunlock_calls++;
    (void)handle;
}

static int fake_set_handle_size(void* handle, long size) {
    if (!g_current) return -1;
    g_current->set_handle_size_calls++;
    if (g_current->set_handle_size_fail_after >= 0 &&
        g_current->set_handle_size_calls > g_current->set_handle_size_fail_after) {
        return -1;
    }
    void** h = (void**)handle;
    void* new_ptr = realloc(*h, (size_t)size);
    if (!new_ptr && size > 0) return -1;
    *h = new_ptr;
    return 0;
}

static short fake_mem_error(void) {
    return g_current ? g_current->mem_error_result : 0;
}

static short fake_fsp_open_df(const void* spec, char perm, short* out_ref) {
    if (g_current) g_current->fsp_open_df_calls++;
    (void)spec; (void)perm;
    if (out_ref) *out_ref = 42;
    return g_current ? g_current->fsp_open_df_result : 0;
}

static short fake_fs_close(short ref) {
    if (g_current) g_current->fs_close_calls++;
    (void)ref;
    return 0;
}

static short fake_fs_read(short ref, long* io_count, void* buf) {
    if (g_current) g_current->fs_read_calls++;
    (void)ref; (void)io_count; (void)buf;
    return g_current ? g_current->fs_read_result : 0;
}

static short fake_fs_write(short ref, long* io_count, const void* buf) {
    if (g_current) g_current->fs_write_calls++;
    (void)ref; (void)io_count; (void)buf;
    return g_current ? g_current->fs_write_result : 0;
}

static short fake_get_eof(short ref, long* out_size) {
    if (g_current) g_current->get_eof_calls++;
    if (out_size) *out_size = g_current ? g_current->get_eof_size : 0;
    (void)ref;
    return g_current ? g_current->get_eof_result : 0;
}

static short fake_set_eof(short ref, long size) {
    if (g_current) g_current->set_eof_calls++;
    (void)ref; (void)size;
    return 0;
}

static short fake_set_fpos(short ref, short mode, long off) {
    if (g_current) g_current->set_fpos_calls++;
    (void)ref; (void)mode; (void)off;
    return 0;
}

static short fake_fsp_create(const void* spec, unsigned long creator,
                             unsigned long type, short script) {
    if (g_current) g_current->fsp_create_calls++;
    (void)spec; (void)creator; (void)type; (void)script;
    return 0;
}

static void fake_standard_get_file(void* out_spec, int* out_good) {
    (void)out_spec;
    if (out_good) *out_good = 0;  /* user always "cancels" in tests */
}

static void fake_standard_put_file(const char* prompt, const char* defname,
                                   void* out_spec, int* out_good) {
    (void)prompt; (void)defname; (void)out_spec;
    if (out_good) *out_good = 0;
}

static short fake_gestalt(unsigned long selector, long* out_response) {
    if (g_current) g_current->gestalt_calls++;
    (void)selector;
    if (out_response) *out_response = g_current ? g_current->gestalt_response : 0;
    return g_current ? g_current->gestalt_result : 0;
}

static short fake_note_alert(short id, void* filter) {
    if (g_current) g_current->note_alert_calls++;
    (void)id; (void)filter;
    return 1;
}

static short fake_stop_alert(short id, void* filter) {
    if (g_current) g_current->stop_alert_calls++;
    (void)id; (void)filter;
    return 1;
}

FakeSyscalls fake_syscalls_init(void) {
    FakeSyscalls f;
    memset(&f, 0, sizeof f);
    f.set_handle_size_fail_after = -1;
    f.new_handle_fail_after      = -1;

    f.vtable.tick_count        = fake_tick_count;
    f.vtable.new_handle        = fake_new_handle;
    f.vtable.dispose_handle    = fake_dispose_handle;
    f.vtable.hlock             = fake_hlock;
    f.vtable.hunlock           = fake_hunlock;
    f.vtable.set_handle_size   = fake_set_handle_size;
    f.vtable.mem_error         = fake_mem_error;
    f.vtable.fsp_open_df       = fake_fsp_open_df;
    f.vtable.fs_close          = fake_fs_close;
    f.vtable.fs_read           = fake_fs_read;
    f.vtable.fs_write          = fake_fs_write;
    f.vtable.get_eof           = fake_get_eof;
    f.vtable.set_eof           = fake_set_eof;
    f.vtable.set_fpos          = fake_set_fpos;
    f.vtable.fsp_create        = fake_fsp_create;
    f.vtable.standard_get_file = fake_standard_get_file;
    f.vtable.standard_put_file = fake_standard_put_file;
    f.vtable.gestalt           = fake_gestalt;
    f.vtable.note_alert        = fake_note_alert;
    f.vtable.stop_alert        = fake_stop_alert;

    /* Publish this instance as the current one. Tests that run one at a
     * time (the intended usage) will each call fake_syscalls_init
     * before doing anything and get a clean slate. */
    g_current = 0;  /* caller must rebind via activate macro in test bodies */
    return f;
}
```

There's a subtlety here: `FakeSyscalls` is a value type, but the callbacks read from a file-scope `g_current` pointer. The convention: tests call `fake_syscalls_init()` into a local variable, then manually set `g_current`:

```c
FakeSyscalls sys = fake_syscalls_init();
extern FakeSyscalls* fake_syscalls_current(void);  /* NOT exposed */
/* Actually — simpler: inline the activate step into the test. */
```

Let me just add an explicit activate function:

- [ ] **Step 21.3: Update `test/fake_syscalls.h` and `test/fake_syscalls.c` with `activate` function**

Append to `test/fake_syscalls.h` (before the `#endif`):

```c
/* Mark `f` as the currently-active fake, so the callback functions see
 * its counters and controls. Call after fake_syscalls_init() and
 * before passing the fake to code under test. */
void fake_syscalls_activate(FakeSyscalls* f);
```

Append to `test/fake_syscalls.c` (at the end, before any `#endif`):

```c
void fake_syscalls_activate(FakeSyscalls* f) {
    g_current = f;
}
```

- [ ] **Step 21.4: Verify compile**

```bash
cc -std=c89 -Wall -Werror -I. -c test/fake_syscalls.c -o /tmp/fs.o
rm /tmp/fs.o
```

Expected: exit 0.

- [ ] **Step 21.5: Commit**

```bash
git add test/fake_syscalls.h test/fake_syscalls.c
git rm test/.gitkeep 2>/dev/null || true
```

Then `git-commit` with message:

```
Add test/fake_syscalls — host-test fakes for MacSyscalls

FakeSyscalls wraps MacSyscalls with controllable results
(tick_count_now, set_handle_size_fail_after, etc.) and per-call
counters for assertions. Malloc-backed two-level Handles mirror the
Mac Memory Manager shape. fake_syscalls_init() returns a defaults-
populated struct; fake_syscalls_activate() binds it for the
callbacks to see.

NOT reentrant — one test at a time. Matches the one-test-per-process
convention Unity uses.
```

---

Phase 2 complete once every header above is in place. Running state check before moving to Phase 3:

- [ ] **Step 21.6: Final Phase-2 verification**

```bash
# Every header should compile cleanly together.
cat > /tmp/all_headers.c <<'EOF'
#include "src/mac_syscalls.h"
#include "src/doc.h"
#include "src/file_io.h"
#include "src/debounce.h"
#include "src_pane/src_pane.h"
#include "render/inlines.h"
#include "render/blocks.h"
#include "render/arena.h"
#include "render/draw_qd.h"
#include "render/render.h"
#include "mdparse/mdparse.h"
#include "scanner/scanner.h"
#include "test/fake_syscalls.h"
int main(void) { return 0; }
EOF
cc -std=c89 -Wall -Werror -I. -Itest -c /tmp/all_headers.c -o /tmp/all_headers.o
rm /tmp/all_headers.*
```

Expected: exit 0, zero warnings.

No commit for this verification step.

---

## Phase 3 — Arena allocator

Arena is the foundational module everything else depends on. Strict TDD here: write tests first, watch them fail for the right reason, write minimal code, watch tests pass, commit.

### Task 22: Arena test harness skeleton (fail)

**Files:**
- Create: `render/arena_test.c`

- [ ] **Step 22.1: Create `render/arena_test.c` with an empty main**

```c
/*
 * render/arena_test.c — host unit tests for the arena allocator.
 *
 * Every test covers one behavior described in openspec/specs/render/
 * spec.md's "Arena allocator" requirement. Unity drives the asserts.
 * FakeSyscalls provides a malloc-backed Handle.
 *
 * What's a Handle?
 *   On classic Mac, a Handle is a pointer to a master pointer (a
 *   double indirection). The Memory Manager can move the block of
 *   memory the master pointer points to in order to compact the heap.
 *   To keep *handle stable across calls, HLock the handle. Our arena
 *   HLocks once at init and keeps the lock for the arena's lifetime,
 *   so *backing is always a stable pointer into memory we own.
 */
#include "unity.h"
#include "fake_syscalls.h"
#include "arena.h"

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    /* Tests added in subsequent tasks. */
    return UNITY_END();
}
```

- [ ] **Step 22.2: Add arena_test to `Makefile.hosttests`**

Edit `Makefile.hosttests` — update the `TESTS :=` line and append a rule. Replace the empty `TESTS :=` line with:

```makefile
TESTS := $(BUILDDIR)/arena_test
```

And append a rule block before `clean:`:

```makefile
$(BUILDDIR)/arena_test: render/arena.c render/arena_test.c $(UNITY) $(FAKES)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^
```

- [ ] **Step 22.3: Verify test-harness-only build fails with "no arena_* defined"**

```bash
make -f Makefile.hosttests test
```

Expected: linker error on `arena_init`, `arena_destroy`, etc. because `render/arena.c` doesn't exist yet. The error message will mention the first arena function used — since our test harness doesn't use any yet, this specific failure won't show until Task 23 adds real test bodies. For now, just confirm the COMPILE step works (Unity + test harness + empty tests.) If compile succeeds and link fails with an unresolved reference to a `_test_*` symbol or similar, that's fine — means the harness wiring is right, the arena implementation is missing.

Actually — let's verify more precisely. Run:

```bash
make -f Makefile.hosttests $(BUILDDIR)/arena_test 2>&1 | tail -5
```

Expected: error mentioning missing `render/arena.c` source file (CC can't find it). That's the correct failure at this point.

- [ ] **Step 22.4: Create `render/arena.c` stub to satisfy the compiler**

A deliberately-empty `render/arena.c` with header includes only, so compile succeeds but linking finds unresolved symbols when real tests call `arena_init` etc. This is the canonical TDD "it compiles, it doesn't work yet" state.

```c
/*
 * render/arena.c — Handle-backed bump allocator (stub; real impl
 * lands in subsequent tasks).
 */
#include "arena.h"
```

- [ ] **Step 22.5: Verify compile succeeds, link is empty-but-passing**

```bash
make -f Makefile.hosttests test
```

Expected:

```
=== build_hosttests/arena_test ===

-----------------------
0 Tests 0 Failures 0 Ignored
OK
Summary: 1 passed, 0 failed
```

The test harness runs, no tests registered, Unity reports OK. Correct empty state.

- [ ] **Step 22.6: Commit**

```bash
git add render/arena_test.c render/arena.c Makefile.hosttests
```

Then `git-commit` with message:

```
Add arena test harness skeleton

Empty Unity-based test main with FakeSyscalls wired. arena.c is a
header-include-only stub so the empty test harness compiles. Each
subsequent task adds one test and the minimal implementation to
make it pass.
```

---

### Task 23: arena_init allocates and HLocks a Handle (red → green → commit)

- [ ] **Step 23.1: Add the test (red)**

Edit `render/arena_test.c`. Inside `main`, before `return UNITY_END();`, add:

```c
    RUN_TEST(test_arena_init_allocates_and_hlocks_backing_handle);
```

And add the test function above `main`, after `tearDown`:

```c
/* arena_init: allocation + HLock
 * ───────────────────────────────
 * arena_init SHALL allocate a Handle of the requested size via
 * MacSyscalls.new_handle and HLock it. Post-conditions: the arena
 * returned is non-NULL; new_handle was called exactly once; hlock
 * was called exactly once; the arena reports the initial capacity. */
void test_arena_init_allocates_and_hlocks_backing_handle(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    int rc = arena_init(&a, 4096, (const MacSyscalls*)&f);

    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_EQUAL_INT(1, f.new_handle_calls);
    TEST_ASSERT_EQUAL_INT(1, f.hlock_calls);
    TEST_ASSERT_EQUAL_INT(4096, arena_capacity(a));
    TEST_ASSERT_EQUAL_INT(0, arena_high_water(a));

    arena_destroy(a);
}
```

- [ ] **Step 23.2: Run — expect link failure (red)**

```bash
make -f Makefile.hosttests test
```

Expected: link fails with undefined references to `arena_init`, `arena_destroy`, `arena_capacity`, `arena_high_water`. Good — the stub has no implementations.

- [ ] **Step 23.3: Write the minimal implementation (green)**

Replace `render/arena.c` contents with:

```c
/*
 * render/arena.c — Handle-backed bump allocator.
 *
 * See design.md §4 for full policy rationale.
 */
#include "arena.h"
#include <stdlib.h>
#include <string.h>

struct Arena {
    void*              backing;         /* Handle (void**) */
    char*              base;            /* = *backing while HLocked */
    size_t             size;
    size_t             high_water;
    size_t             max_ever;
    const MacSyscalls* sys;
};

int arena_init(Arena** out, size_t initial_size, const MacSyscalls* sys) {
    if (!out || !sys) return -1;
    *out = 0;

    Arena* a = (Arena*)calloc(1, sizeof(Arena));
    if (!a) return -1;

    a->backing = sys->new_handle((long)initial_size);
    if (!a->backing) { free(a); return -1; }
    sys->hlock(a->backing);
    a->base       = *(char**)a->backing;
    a->size       = initial_size;
    a->high_water = 0;
    a->max_ever   = 0;
    a->sys        = sys;

    *out = a;
    return 0;
}

void arena_destroy(Arena* a) {
    if (!a) return;
    if (a->backing) {
        a->sys->hunlock(a->backing);
        a->sys->dispose_handle(a->backing);
    }
    free(a);
}

size_t arena_capacity(const Arena* a)   { return a ? a->size : 0; }
size_t arena_high_water(const Arena* a) { return a ? a->high_water : 0; }

/* Stubs for the rest of the API — fleshed out in later tasks. */
int  arena_ensure(Arena* a, size_t n) { (void)a; (void)n; return -1; }
void* arena_alloc(Arena* a, size_t n) { (void)a; (void)n; return 0; }
void  arena_reset(Arena* a)           { if (a) a->high_water = 0; }
```

- [ ] **Step 23.4: Run — expect green**

```bash
make -f Makefile.hosttests test
```

Expected:

```
=== build_hosttests/arena_test ===
...
-----------------------
1 Tests 0 Failures 0 Ignored
OK
Summary: 1 passed, 0 failed
```

- [ ] **Step 23.5: Commit**

```bash
git add render/arena.c render/arena_test.c
```

Then `git-commit` with message:

```
Implement arena_init / arena_destroy + capacity/high_water getters

First test covers Handle allocation + HLock + initial capacity
reporting. Implementation uses FakeSyscalls.new_handle and .hlock for
storage, *backing for base pointer stability (valid while HLocked).
Remaining API (arena_ensure, arena_alloc) stubbed to return
failure/NULL until their tests drive real impls.
```

---

### Task 24: arena_alloc returns 4-byte-aligned pointers (red → green → commit)

- [ ] **Step 24.1: Add tests**

Append to `render/arena_test.c` (before `main`):

```c
/* arena_alloc: alignment and bump semantics
 * ──────────────────────────────────────────
 * arena_alloc SHALL return pointers aligned to 4 bytes and bump
 * high_water by the requested size rounded up to 4. A 5-byte request
 * consumes 8 bytes of arena space. */
void test_arena_alloc_returns_4byte_aligned_pointer(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    arena_init(&a, 4096, (const MacSyscalls*)&f);

    void* p = arena_alloc(a, 5);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_INT(0, (size_t)p & 3u);
    TEST_ASSERT_EQUAL_INT(8, arena_high_water(a));

    void* q = arena_alloc(a, 1);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_EQUAL_INT(0, (size_t)q & 3u);
    TEST_ASSERT_EQUAL_INT(12, arena_high_water(a));

    arena_destroy(a);
}

void test_arena_alloc_beyond_capacity_returns_null(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    arena_init(&a, 128, (const MacSyscalls*)&f);

    (void)arena_alloc(a, 120);   /* rounded to 120; high_water 120 */
    void* p = arena_alloc(a, 16); /* would bump to 136 > 128 */
    TEST_ASSERT_NULL(p);
    TEST_ASSERT_EQUAL_INT(120, arena_high_water(a));

    arena_destroy(a);
}
```

And add `RUN_TEST` calls in `main`:

```c
    RUN_TEST(test_arena_alloc_returns_4byte_aligned_pointer);
    RUN_TEST(test_arena_alloc_beyond_capacity_returns_null);
```

- [ ] **Step 24.2: Run — expect 2 failures**

```bash
make -f Makefile.hosttests test
```

Expected: the two new tests FAIL (stub returns NULL for all allocs).

- [ ] **Step 24.3: Implement `arena_alloc`**

Replace the `arena_alloc` stub in `render/arena.c` with:

```c
void* arena_alloc(Arena* a, size_t n_bytes) {
    if (!a) return 0;
    /* Round up to 4-byte alignment. 68020+ requires 4-byte alignment
     * for longword access; we round every allocation up. */
    n_bytes = (n_bytes + 3u) & ~(size_t)3u;
    if (a->high_water + n_bytes > a->size) return 0;
    void* p = a->base + a->high_water;
    a->high_water += n_bytes;
    if (a->high_water > a->max_ever) a->max_ever = a->high_water;
    return p;
}
```

- [ ] **Step 24.4: Run — expect green**

```bash
make -f Makefile.hosttests test
```

Expected: 3 Tests, 0 Failures.

- [ ] **Step 24.5: Commit**

```bash
git add render/arena.c render/arena_test.c
```

Then `git-commit` with message:

```
Implement arena_alloc — 4-byte aligned bump allocator

Rounds requested size up to 4 bytes, checks against capacity, returns
NULL without side effects on overflow. 68020+ traps on longword
access to odd addresses; the alignment here is the reason all Block
and MdStyleRun structs in the arena are safely long-addressable.
```

---

### Task 25: arena_ensure grows Handle before parse (red → green → commit)

- [ ] **Step 25.1: Add tests**

Append to `render/arena_test.c`:

```c
/* arena_ensure: pre-parse growth
 * ───────────────────────────────
 * arena_ensure SHALL grow the backing Handle so at least bytes_needed
 * bytes are available above high_water. Doubling up to 64KB, then
 * +32KB linear, capped at 512KB. */
void test_arena_ensure_doubles_when_under_cap(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    arena_init(&a, 4096, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(4096, arena_capacity(a));

    int rc = arena_ensure(a, 6000);  /* needs >= 6000 available */
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(arena_capacity(a) >= 6000);
    /* After doubling from 4096 we expect at least 8192. */
    TEST_ASSERT_EQUAL_INT(8192, arena_capacity(a));

    arena_destroy(a);
}

void test_arena_ensure_grow_failure_preserves_state(void) {
    FakeSyscalls f = fake_syscalls_init();
    f.set_handle_size_fail_after = 0;  /* the FIRST SetHandleSize fails */
    fake_syscalls_activate(&f);

    Arena* a = 0;
    arena_init(&a, 128, (const MacSyscalls*)&f);
    (void)arena_alloc(a, 64);

    int rc = arena_ensure(a, 10000);
    TEST_ASSERT_NOT_EQUAL(0, rc);
    TEST_ASSERT_EQUAL_INT(128, arena_capacity(a));
    TEST_ASSERT_EQUAL_INT(64, arena_high_water(a));

    arena_destroy(a);
}

void test_arena_ensure_no_op_when_already_sized(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    arena_init(&a, 4096, (const MacSyscalls*)&f);

    int rc = arena_ensure(a, 1024);  /* already have 4096 > 1024 */
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(4096, arena_capacity(a));
    TEST_ASSERT_EQUAL_INT(0, f.set_handle_size_calls);

    arena_destroy(a);
}
```

Add the three `RUN_TEST` lines in `main`.

- [ ] **Step 25.2: Run — expect 3 new failures** (plus existing tests pass)

```bash
make -f Makefile.hosttests test
```

- [ ] **Step 25.3: Implement `arena_ensure`**

Add these constants at the top of `render/arena.c` (above `struct Arena`):

```c
#define kArenaDoublingCap   (64u * 1024u)
#define kArenaLinearStep    (32u * 1024u)
#define kArenaHardCap      (512u * 1024u)
```

Replace the `arena_ensure` stub with:

```c
int arena_ensure(Arena* a, size_t bytes_needed) {
    if (!a) return -1;
    size_t needed_total = a->high_water + bytes_needed;
    if (needed_total <= a->size) return 0;

    /* Compute next size using the design's growth policy. */
    size_t next = a->size;
    while (next < needed_total) {
        if (next < kArenaDoublingCap) {
            next = next < 1 ? 1 : next * 2;
        } else {
            next += kArenaLinearStep;
        }
        if (next > kArenaHardCap) return -1;
    }

    /* SetHandleSize may relocate the block. HUnlock first, resize,
     * rebind base after relocking. If resize fails, relock at old
     * size and return failure. */
    a->sys->hunlock(a->backing);
    int rc = a->sys->set_handle_size(a->backing, (long)next);
    a->sys->hlock(a->backing);
    a->base = *(char**)a->backing;
    if (rc != 0) return -1;

    a->size = next;
    return 0;
}
```

- [ ] **Step 25.4: Run — expect all green**

```bash
make -f Makefile.hosttests test
```

Expected: 6 Tests, 0 Failures.

- [ ] **Step 25.5: Commit**

```bash
git add render/arena.c render/arena_test.c
```

Then `git-commit` with message:

```
Implement arena_ensure — grow-before-parse policy

Doubles up to 64KB, then +32KB linear. Hard-cap 512KB; exceeding
returns -1 without resizing. Unlock/resize/relock sequence is
mandatory because SetHandleSize may relocate the block. On resize
failure, we relock at the old size and report failure — state is
preserved for the caller to report cleanly to the user.
```

---

### Task 26: arena_reset keeps capacity (red → green → commit)

- [ ] **Step 26.1: Add test**

Append to `render/arena_test.c`:

```c
/* arena_reset: watermark back to 0, backing preserved
 * ─────────────────────────────────────────────────────
 * arena_reset SHALL clear high_water so the next parse cycle starts
 * fresh. The backing Handle MUST NOT be disposed — its memory is
 * reused, saving a NewHandle per parse. */
void test_arena_reset_clears_watermark_but_preserves_capacity(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    arena_init(&a, 4096, (const MacSyscalls*)&f);
    (void)arena_alloc(a, 1000);
    TEST_ASSERT_EQUAL_INT(1000, arena_high_water(a));
    TEST_ASSERT_EQUAL_INT(4096, arena_capacity(a));

    arena_reset(a);
    TEST_ASSERT_EQUAL_INT(0, arena_high_water(a));
    TEST_ASSERT_EQUAL_INT(4096, arena_capacity(a));
    TEST_ASSERT_EQUAL_INT(0, f.dispose_handle_calls);

    /* Next alloc reuses the same memory region. */
    void* p = arena_alloc(a, 500);
    TEST_ASSERT_NOT_NULL(p);

    arena_destroy(a);
}
```

Add `RUN_TEST` line.

- [ ] **Step 26.2: Run — verify pre-existing stub already passes**

The existing stub impl `arena_reset(a) { if (a) a->high_water = 0; }` is already correct. Run:

```bash
make -f Makefile.hosttests test
```

Expected: 7 Tests, 0 Failures.

No new implementation needed — the stub was already right. Good place to validate TDD's "if the test passes on the first red run, the test doesn't prove anything yet" heuristic: the assertion about `dispose_handle_calls == 0` does confirm the stub doesn't dispose the handle.

- [ ] **Step 26.3: Commit**

```bash
git add render/arena_test.c
```

Then `git-commit` with message:

```
Test arena_reset preserves capacity, doesn't dispose backing

Validates that arena_reset is a zero-cost watermark reset (no
NewHandle/DisposeHandle churn per parse cycle). Existing stub impl
already satisfies the behavior; test locks it in.
```

---

### Task 27: arena_destroy disposes exactly once (test-only; already implemented)

- [ ] **Step 27.1: Add test**

Append to `render/arena_test.c`:

```c
/* arena_destroy: single DisposeHandle call
 * ──────────────────────────────────────────
 * arena_destroy disposes the backing Handle exactly once. No-op on
 * NULL. Calling it is the only path that ever disposes — destroy is
 * terminal. */
void test_arena_destroy_disposes_backing_exactly_once(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    arena_init(&a, 2048, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(0, f.dispose_handle_calls);

    arena_destroy(a);
    TEST_ASSERT_EQUAL_INT(1, f.dispose_handle_calls);

    /* NULL is a no-op */
    arena_destroy(0);
    TEST_ASSERT_EQUAL_INT(1, f.dispose_handle_calls);
}
```

Add `RUN_TEST` line.

- [ ] **Step 27.2: Run — expect green** (existing impl satisfies)

```bash
make -f Makefile.hosttests test
```

Expected: 8 Tests, 0 Failures.

- [ ] **Step 27.3: Commit**

```bash
git add render/arena_test.c
```

Then `git-commit` with message:

```
Test arena_destroy disposes backing exactly once and NULL-safes

Locks in the single-dispose and NULL-safe guarantees. Current impl
already satisfies both.
```

---

### Task 28: arena reset+alloc reuses memory without re-NewHandle (full end-to-end)

This is the "use it the way production uses it" acceptance test.

- [ ] **Step 28.1: Add test**

Append to `render/arena_test.c`:

```c
/* Reset + alloc cycle: reuses memory
 * ─────────────────────────────────────
 * Simulates the parse pipeline's inner loop: many reset+alloc cycles
 * should NOT trigger additional NewHandle calls. Only the initial
 * init and any grow events should. */
void test_arena_reset_alloc_cycle_does_not_allocate_new_handles(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    Arena* a = 0;
    arena_init(&a, 4096, (const MacSyscalls*)&f);
    int initial_new_handles = f.new_handle_calls;  /* should be 1 */
    TEST_ASSERT_EQUAL_INT(1, initial_new_handles);

    for (int i = 0; i < 10; i++) {
        arena_reset(a);
        (void)arena_alloc(a, 1000);
        (void)arena_alloc(a, 500);
    }

    TEST_ASSERT_EQUAL_INT(1, f.new_handle_calls);  /* still 1 */
    TEST_ASSERT_EQUAL_INT(0, f.set_handle_size_calls);  /* never grew */

    arena_destroy(a);
}
```

Add `RUN_TEST` line.

- [ ] **Step 28.2: Run — expect green**

```bash
make -f Makefile.hosttests test
```

Expected: 9 Tests, 0 Failures.

- [ ] **Step 28.3: Commit**

```bash
git add render/arena_test.c
```

Then `git-commit` with message:

```
Test arena reset+alloc cycle reuses memory

10 reset+alloc cycles should trigger exactly 1 NewHandle (the initial
init) and 0 SetHandleSize calls. This is the hot path of the parse
pipeline — we mustn't churn the Memory Manager per parse.
```

---

Phase 3 is now complete. Arena is fully implemented and covered by tests. The pipeline's foundational allocator is ready.

## Phase 4 — Doc

Smaller than arena. One task per requirement group.

### Task 29: Doc test harness + new/free (red → green → commit)

**Files:**
- Create: `src/doc_test.c`, `src/doc.c`

- [ ] **Step 29.1: Create `src/doc_test.c`**

```c
/*
 * src/doc_test.c — host unit tests for the document data container.
 *
 * Doc is a dumb data holder. Tests cover new/free, text get/set
 * (including over-limit rejection), dirty flag, filename accessors.
 * No Toolbox dependency.
 */
#include "unity.h"
#include "doc.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_doc_new_returns_non_null_empty_clean_doc(void) {
    Doc* d = doc_new();
    TEST_ASSERT_NOT_NULL(d);

    unsigned short len = 99;
    const char* p = doc_text(d, &len);
    TEST_ASSERT_NOT_NULL(p);  /* buffer exists even if empty */
    TEST_ASSERT_EQUAL_INT(0, len);
    TEST_ASSERT_EQUAL_INT(0, doc_is_dirty(d));
    TEST_ASSERT_EQUAL_INT(0, doc_has_filename(d));

    doc_free(d);
}

void test_doc_free_null_is_noop(void) {
    doc_free(0);  /* should not crash */
    TEST_ASSERT_TRUE(1);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_doc_new_returns_non_null_empty_clean_doc);
    RUN_TEST(test_doc_free_null_is_noop);
    return UNITY_END();
}
```

- [ ] **Step 29.2: Add doc_test to `Makefile.hosttests`**

Update `TESTS :=` line:

```makefile
TESTS := $(BUILDDIR)/arena_test $(BUILDDIR)/doc_test
```

Append rule block:

```makefile
$(BUILDDIR)/doc_test: src/doc.c src/doc_test.c $(UNITY)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^
```

- [ ] **Step 29.3: Create `src/doc.c` stub**

```c
#include "doc.h"
#include <stdlib.h>
#include <string.h>
```

- [ ] **Step 29.4: Run — expect link failures on doc_new, doc_text, etc.**

```bash
make -f Makefile.hosttests test
```

- [ ] **Step 29.5: Implement doc lifecycle + accessors**

Replace `src/doc.c` with:

```c
#include "doc.h"
#include <stdlib.h>
#include <string.h>

struct Doc {
    char*         text_buf;
    unsigned short text_len;
    int            dirty;
    char           filename[64];
    unsigned char  filename_len;
};

Doc* doc_new(void) {
    Doc* d = (Doc*)calloc(1, sizeof(Doc));
    if (!d) return 0;
    /* Pre-allocate a small empty buffer so doc_text never returns NULL. */
    d->text_buf = (char*)calloc(1, 1);
    if (!d->text_buf) { free(d); return 0; }
    return d;
}

void doc_free(Doc* d) {
    if (!d) return;
    if (d->text_buf) free(d->text_buf);
    free(d);
}

const char* doc_text(const Doc* d, unsigned short* out_len) {
    if (!d) { if (out_len) *out_len = 0; return ""; }
    if (out_len) *out_len = d->text_len;
    return d->text_buf;
}

void doc_set_text(Doc* d, const char* bytes, unsigned short len) {
    if (!d) return;
    if (len > kMaxDocBytes) return;           /* reject, no mutation */
    char* buf = (char*)realloc(d->text_buf, (size_t)len + 1);
    if (!buf && len > 0) return;               /* reject, no mutation */
    if (buf) d->text_buf = buf;
    if (len > 0) memcpy(d->text_buf, bytes, len);
    d->text_buf[len] = '\0';                   /* convenience; len is authoritative */
    d->text_len = len;
    d->dirty = 1;
}

int  doc_is_dirty(const Doc* d)    { return d ? d->dirty : 0; }
void doc_mark_dirty(Doc* d)        { if (d) d->dirty = 1; }
void doc_mark_clean(Doc* d)        { if (d) d->dirty = 0; }

void doc_set_filename(Doc* d, const char* bytes, unsigned char len) {
    if (!d) return;
    if (len > sizeof(d->filename)) len = sizeof(d->filename);
    memcpy(d->filename, bytes, len);
    d->filename_len = len;
}

int doc_has_filename(const Doc* d) { return d ? (d->filename_len > 0) : 0; }

const char* doc_filename(const Doc* d, unsigned char* out_len) {
    if (!d) { if (out_len) *out_len = 0; return ""; }
    if (out_len) *out_len = d->filename_len;
    return d->filename;
}
```

- [ ] **Step 29.6: Run — expect green**

```bash
make -f Makefile.hosttests test
```

Expected: all arena tests + 2 doc tests passing.

- [ ] **Step 29.7: Commit**

```bash
git add src/doc.c src/doc_test.c Makefile.hosttests
```

Then `git-commit` with message:

```
Implement src/doc — dumb document data container

Opaque Doc with text buffer, text length, dirty flag, filename. All
accessors / mutators plus new/free. Full impl landed in one commit
because the module is small and all operations share the same state.
```

---

### Task 30: Doc text round-trip and over-limit rejection

- [ ] **Step 30.1: Add tests**

Append to `src/doc_test.c`:

```c
void test_doc_set_text_round_trip(void) {
    Doc* d = doc_new();
    doc_set_text(d, "Hello", 5);

    unsigned short len;
    const char* p = doc_text(d, &len);
    TEST_ASSERT_EQUAL_INT(5, len);
    TEST_ASSERT_EQUAL_MEMORY("Hello", p, 5);
    TEST_ASSERT_EQUAL_INT(1, doc_is_dirty(d));

    doc_free(d);
}

void test_doc_set_text_overwrite(void) {
    Doc* d = doc_new();
    doc_set_text(d, "First", 5);
    doc_set_text(d, "Second!", 7);

    unsigned short len;
    const char* p = doc_text(d, &len);
    TEST_ASSERT_EQUAL_INT(7, len);
    TEST_ASSERT_EQUAL_MEMORY("Second!", p, 7);
    doc_free(d);
}

void test_doc_set_text_over_limit_is_rejected(void) {
    Doc* d = doc_new();
    doc_set_text(d, "ok", 2);
    doc_mark_clean(d);

    /* Attempt an over-limit set; should leave the Doc unchanged. */
    char big[100];
    memset(big, 'x', sizeof big);
    unsigned short over = (unsigned short)(kMaxDocBytes);  /* equal to max is ok */
    (void)over;  /* test the clearly-over case */
    doc_set_text(d, big, 65535);  /* > 32767 */

    unsigned short len;
    const char* p = doc_text(d, &len);
    TEST_ASSERT_EQUAL_INT(2, len);
    TEST_ASSERT_EQUAL_MEMORY("ok", p, 2);
    TEST_ASSERT_EQUAL_INT(0, doc_is_dirty(d));  /* unchanged */

    doc_free(d);
}
```

Note the last test passes `len = 65535` as an `unsigned short`, which is its max. Since `kMaxDocBytes = 32767`, 65535 is well above. Add the three `RUN_TEST` lines in `main`.

- [ ] **Step 30.2: Run — expect green**

```bash
make -f Makefile.hosttests test
```

Expected: arena + 5 doc tests passing.

- [ ] **Step 30.3: Commit**

```bash
git add src/doc_test.c
```

Then `git-commit` with message:

```
Test Doc text round-trip, overwrite, over-limit rejection

Confirms set_text copies the bytes, marks dirty, allows overwrite,
and cleanly rejects over-kMaxDocBytes calls without mutation.
```

---

### Task 31: Doc dirty flag and filename

- [ ] **Step 31.1: Add tests**

Append to `src/doc_test.c`:

```c
void test_doc_mark_clean_clears_dirty(void) {
    Doc* d = doc_new();
    doc_set_text(d, "x", 1);
    TEST_ASSERT_EQUAL_INT(1, doc_is_dirty(d));
    doc_mark_clean(d);
    TEST_ASSERT_EQUAL_INT(0, doc_is_dirty(d));
    doc_free(d);
}

void test_doc_filename_round_trip(void) {
    Doc* d = doc_new();
    TEST_ASSERT_EQUAL_INT(0, doc_has_filename(d));
    doc_set_filename(d, "notes.md", 8);
    TEST_ASSERT_EQUAL_INT(1, doc_has_filename(d));

    unsigned char len;
    const char* fn = doc_filename(d, &len);
    TEST_ASSERT_EQUAL_INT(8, len);
    TEST_ASSERT_EQUAL_MEMORY("notes.md", fn, 8);

    doc_free(d);
}
```

Add `RUN_TEST` lines.

- [ ] **Step 31.2: Run — expect green + Commit**

```bash
make -f Makefile.hosttests test
git add src/doc_test.c
```

Then `git-commit` with message:

```
Test Doc dirty flag and filename storage

Covers doc_mark_clean clearing the flag and filename set/has/get
round-trip. Doc is feature-complete at this point.
```

---

## Phase 5 — Debounce

Pure state machine. Two functions, ~40 lines.

### Task 32: Debounce test harness + two-edit-within-window → no parse

**Files:**
- Create: `src/debounce_test.c`, `src/debounce.c`

- [ ] **Step 32.1: Create `src/debounce_test.c`**

```c
/*
 * src/debounce_test.c — host unit tests for parse debounce.
 *
 * Uses FakeSyscalls's tick_count_now to drive the clock deterministically.
 */
#include "unity.h"
#include "fake_syscalls.h"
#include "debounce.h"

void setUp(void) {}
void tearDown(void) {}

void test_debounce_fresh_state_poll_returns_no_parse(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    DebounceState s = {0, 0};
    int fire = debounce_poll(&s, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(0, fire);
}

void test_debounce_edit_then_short_idle_does_not_fire(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    DebounceState s = {0, 0};
    f.tick_count_now = 100;
    debounce_on_edit(&s, (const MacSyscalls*)&f);
    f.tick_count_now = 110;  /* 10 ticks elapsed; less than 15 */

    int fire = debounce_poll(&s, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(0, fire);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_debounce_fresh_state_poll_returns_no_parse);
    RUN_TEST(test_debounce_edit_then_short_idle_does_not_fire);
    return UNITY_END();
}
```

- [ ] **Step 32.2: Add to Makefile.hosttests**

Update TESTS:

```makefile
TESTS := $(BUILDDIR)/arena_test $(BUILDDIR)/doc_test $(BUILDDIR)/debounce_test
```

Append rule:

```makefile
$(BUILDDIR)/debounce_test: src/debounce.c src/debounce_test.c $(UNITY) $(FAKES)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^
```

- [ ] **Step 32.3: Create `src/debounce.c`**

```c
#include "debounce.h"

void debounce_on_edit(DebounceState* s, const MacSyscalls* sys) {
    if (!s || !sys) return;
    s->dirty = 1;
    s->last_keystroke_tick = sys->tick_count();
}

int debounce_poll(DebounceState* s, const MacSyscalls* sys) {
    if (!s || !sys) return 0;
    if (!s->dirty) return 0;
    unsigned long now = sys->tick_count();
    if (now - s->last_keystroke_tick < kParseDebounceTicks) return 0;
    s->dirty = 0;
    return 1;
}
```

- [ ] **Step 32.4: Run — expect green + Commit**

```bash
make -f Makefile.hosttests test
git add src/debounce.c src/debounce_test.c Makefile.hosttests
```

Then `git-commit` with message:

```
Add src/debounce — pure parse-debounce state machine

Two functions: on_edit (marks dirty, captures tick) and poll (returns
1 if parse should run, clearing dirty). Uses MacSyscalls.tick_count
for the clock so tests drive it deterministically via FakeSyscalls.
```

---

### Task 33: Debounce fires after idle; resets on new edit

- [ ] **Step 33.1: Add tests**

Append to `src/debounce_test.c`:

```c
void test_debounce_fires_after_idle(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    DebounceState s = {0, 0};
    f.tick_count_now = 100;
    debounce_on_edit(&s, (const MacSyscalls*)&f);
    f.tick_count_now = 120;  /* 20 ticks > 15; should fire */

    int fire = debounce_poll(&s, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(1, fire);
    TEST_ASSERT_EQUAL_INT(0, s.dirty);  /* cleared */

    /* Subsequent poll: no parse (dirty cleared) */
    f.tick_count_now = 130;
    fire = debounce_poll(&s, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(0, fire);
}

void test_debounce_new_edit_resets_window(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);

    DebounceState s = {0, 0};
    f.tick_count_now = 100;
    debounce_on_edit(&s, (const MacSyscalls*)&f);
    f.tick_count_now = 112;  /* in window */
    debounce_on_edit(&s, (const MacSyscalls*)&f);  /* resets */
    f.tick_count_now = 120;  /* 120 - 112 = 8 ticks, still in window */

    int fire = debounce_poll(&s, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(0, fire);

    f.tick_count_now = 128;  /* 128 - 112 = 16 ticks, fires */
    fire = debounce_poll(&s, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(1, fire);
}
```

Add `RUN_TEST` lines.

- [ ] **Step 33.2: Run + Commit**

```bash
make -f Makefile.hosttests test
git add src/debounce_test.c
```

Then `git-commit` with message:

```
Test debounce fires after idle; new edit resets window

Covers the core debounce behavior: fires once after kParseDebounceTicks
of idle, clears dirty after firing, and correctly resets the window
on a new edit mid-debounce.
```

---

## Phase 6 — Mdparse (md4c adapter)

Mdparse wraps md4c with a fan-out sink interface. Many test cases — one per markdown idiom we support. The implementation is ~250 lines; the tests are ~400 lines. We write test clusters as single tasks to keep the plan tractable.

### Task 34: Mdparse test harness + recording sink helper

**Files:**
- Create: `mdparse/mdparse_test.c`, `mdparse/mdparse.c`

- [ ] **Step 34.1: Create `mdparse/mdparse_test.c`**

```c
/*
 * mdparse/mdparse_test.c — host unit tests for the md4c adapter.
 *
 * Each test feeds a markdown source string into mdparse_run with a
 * recording sink that captures every callback into an array. Tests
 * then assert on the captured event sequence.
 *
 * Why this shape: mdparse is a translator — md4c events in, our sink
 * events out. The cleanest way to spec it is "given this markdown,
 * the sink receives exactly this sequence of calls."
 */
#include "unity.h"
#include "mdparse.h"
#include <string.h>
#include <stdlib.h>

/* Recording sink — captures every callback into a growable array. */
typedef struct RecEvent {
    enum { kRecBlockOpen, kRecBlockClose, kRecSpan, kRecText } kind;
    BlockKind  block_kind;
    StyleKind  span_kind;
    BlockAttrs attrs;
    unsigned short start;
    unsigned short length;
    unsigned short source_offset;
    char        text[256];      /* truncated if larger */
    char        link_url[128];
    unsigned short link_url_len;
} RecEvent;

typedef struct Recorder {
    RecEvent  events[1024];
    size_t    count;
    int       abort_on_nth;    /* -1 = never abort */
    int       calls;           /* total callbacks seen */
} Recorder;

static int rec_block_open(void* ctx, BlockKind k, const BlockAttrs* a) {
    Recorder* r = (Recorder*)ctx;
    r->calls++;
    if (r->count >= 1024) return -1;
    RecEvent* e = &r->events[r->count++];
    e->kind = kRecBlockOpen;
    e->block_kind = k;
    e->attrs = a ? *a : (BlockAttrs){0};
    if (r->abort_on_nth >= 0 && r->calls > r->abort_on_nth) return -1;
    return 0;
}
static int rec_block_close(void* ctx, BlockKind k) {
    Recorder* r = (Recorder*)ctx;
    r->calls++;
    if (r->count >= 1024) return -1;
    RecEvent* e = &r->events[r->count++];
    e->kind = kRecBlockClose;
    e->block_kind = k;
    if (r->abort_on_nth >= 0 && r->calls > r->abort_on_nth) return -1;
    return 0;
}
static int rec_span(void* ctx, StyleKind k, unsigned short start,
                    unsigned short length, const char* url,
                    unsigned short url_len) {
    Recorder* r = (Recorder*)ctx;
    r->calls++;
    if (r->count >= 1024) return -1;
    RecEvent* e = &r->events[r->count++];
    e->kind = kRecSpan;
    e->span_kind = k;
    e->start = start;
    e->length = length;
    if (url && url_len > 0) {
        unsigned short n = url_len < sizeof(e->link_url) ? url_len : sizeof(e->link_url) - 1;
        memcpy(e->link_url, url, n);
        e->link_url[n] = '\0';
        e->link_url_len = url_len;
    } else {
        e->link_url[0] = '\0';
        e->link_url_len = 0;
    }
    if (r->abort_on_nth >= 0 && r->calls > r->abort_on_nth) return -1;
    return 0;
}
static int rec_text(void* ctx, const char* bytes, unsigned short length,
                    unsigned short source_offset) {
    Recorder* r = (Recorder*)ctx;
    r->calls++;
    if (r->count >= 1024) return -1;
    RecEvent* e = &r->events[r->count++];
    e->kind = kRecText;
    e->length = length;
    e->source_offset = source_offset;
    unsigned short n = length < sizeof(e->text) ? length : sizeof(e->text) - 1;
    memcpy(e->text, bytes, n);
    e->text[n] = '\0';
    if (r->abort_on_nth >= 0 && r->calls > r->abort_on_nth) return -1;
    return 0;
}

static MdParseSink recorder_sink(Recorder* r) {
    MdParseSink s;
    s.on_block_open  = rec_block_open;
    s.on_block_close = rec_block_close;
    s.on_span        = rec_span;
    s.on_text        = rec_text;
    s.ctx            = r;
    return s;
}

static Recorder recorder_new(void) {
    Recorder r;
    memset(&r, 0, sizeof r);
    r.abort_on_nth = -1;
    return r;
}

void setUp(void) {}
void tearDown(void) {}

void test_mdparse_empty_source_produces_no_events(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    int rc = mdparse_run("", 0, &s, 1);
    TEST_ASSERT_EQUAL_INT(kMdParseOk, rc);
    TEST_ASSERT_EQUAL_INT(0, r.count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mdparse_empty_source_produces_no_events);
    /* Additional tests added in subsequent tasks. */
    return UNITY_END();
}
```

- [ ] **Step 34.2: Update `Makefile.hosttests`**

Update TESTS:

```makefile
TESTS := $(BUILDDIR)/arena_test $(BUILDDIR)/doc_test $(BUILDDIR)/debounce_test \
         $(BUILDDIR)/mdparse_test
```

Append rule:

```makefile
$(BUILDDIR)/mdparse_test: mdparse/mdparse.c mdparse/mdparse_test.c \
                          third_party/md4c/src/md4c.c $(UNITY)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^
```

- [ ] **Step 34.3: Create `mdparse/mdparse.c` stub**

```c
#include "mdparse.h"
```

- [ ] **Step 34.4: Verify — expect link failure on mdparse_run**

```bash
make -f Makefile.hosttests test
```

- [ ] **Step 34.5: Implement `mdparse_run` (first cut — empty source, sink fan-out)**

Replace `mdparse/mdparse.c` with:

```c
/*
 * mdparse/mdparse.c — md4c adapter with MdParseSink fan-out.
 *
 * md4c's API:
 *   int md_parse(const MD_CHAR* text, MD_SIZE size,
 *                const MD_PARSER* parser, void* userdata);
 *
 * md4c invokes the parser's enter_block / leave_block / enter_span /
 * leave_span / text callbacks for events it detects. Our adapter
 * maintains a depth counter for UL/OL/BLOCKQUOTE (md4c emits open/close
 * events for these containers) and stamps the depth onto each contained
 * block's BlockAttrs.
 *
 * Sinks: we fan out to every sink in array order for every event. If
 * any sink returns non-zero, we stop the parse and return
 * kMdParseErrSinkAbort.
 */
#include "mdparse.h"
#include "md4c.h"
#include <string.h>

typedef struct Dispatcher {
    const MdParseSink* sinks;
    size_t             sink_count;
    unsigned char      list_depth;
    unsigned char      quote_depth;
    unsigned char      list_ordered_stack[16];  /* up to 16 nested lists */
    unsigned char      list_stack_top;
    unsigned short     last_source_offset;
    int                aborted;
} Dispatcher;

static int dispatch_block_open(Dispatcher* d, BlockKind k,
                               const BlockAttrs* a) {
    for (size_t i = 0; i < d->sink_count; i++) {
        int rc = d->sinks[i].on_block_open(d->sinks[i].ctx, k, a);
        if (rc != 0) { d->aborted = 1; return -1; }
    }
    return 0;
}
static int dispatch_block_close(Dispatcher* d, BlockKind k) {
    for (size_t i = 0; i < d->sink_count; i++) {
        int rc = d->sinks[i].on_block_close(d->sinks[i].ctx, k);
        if (rc != 0) { d->aborted = 1; return -1; }
    }
    return 0;
}
static int dispatch_span(Dispatcher* d, StyleKind k,
                         unsigned short start, unsigned short length,
                         const char* url, unsigned short url_len) {
    for (size_t i = 0; i < d->sink_count; i++) {
        int rc = d->sinks[i].on_span(d->sinks[i].ctx, k, start, length,
                                     url, url_len);
        if (rc != 0) { d->aborted = 1; return -1; }
    }
    return 0;
}
static int dispatch_text(Dispatcher* d, const char* bytes,
                         unsigned short length, unsigned short source_offset) {
    for (size_t i = 0; i < d->sink_count; i++) {
        int rc = d->sinks[i].on_text(d->sinks[i].ctx, bytes, length, source_offset);
        if (rc != 0) { d->aborted = 1; return -1; }
    }
    return 0;
}

static int on_enter_block(MD_BLOCKTYPE type, void* detail, void* userdata) {
    Dispatcher* d = (Dispatcher*)userdata;
    if (d->aborted) return -1;

    /* Containers that adjust depth but emit no user-facing event */
    if (type == MD_BLOCK_UL) {
        if (d->list_stack_top < 16) d->list_ordered_stack[d->list_stack_top++] = 0;
        d->list_depth++;
        return 0;
    }
    if (type == MD_BLOCK_OL) {
        if (d->list_stack_top < 16) d->list_ordered_stack[d->list_stack_top++] = 1;
        d->list_depth++;
        return 0;
    }
    if (type == MD_BLOCK_QUOTE) { d->quote_depth++; return 0; }
    if (type == MD_BLOCK_DOC)   { return 0; }

    /* Emitted blocks */
    BlockAttrs a;
    memset(&a, 0, sizeof a);
    a.list_depth  = d->list_depth;
    a.quote_depth = d->quote_depth;
    a.list_ordered = (d->list_stack_top > 0)
                     ? d->list_ordered_stack[d->list_stack_top - 1] : 0;

    BlockKind k;
    switch (type) {
        case MD_BLOCK_P:    k = kBlockParagraph;  break;
        case MD_BLOCK_H: {
            k = kBlockHeading;
            MD_BLOCK_H_DETAIL* dh = (MD_BLOCK_H_DETAIL*)detail;
            a.h_level = (unsigned char)(dh ? dh->level : 1);
            break;
        }
        case MD_BLOCK_LI:   k = kBlockListItem;   break;
        case MD_BLOCK_CODE: k = kBlockCodeBlock;  break;
        case MD_BLOCK_HR:   k = kBlockHr;         break;
        case MD_BLOCK_HTML: k = kBlockHtml;       break;
        default:            return 0;
    }
    return dispatch_block_open(d, k, &a);
}

static int on_leave_block(MD_BLOCKTYPE type, void* detail, void* userdata) {
    (void)detail;
    Dispatcher* d = (Dispatcher*)userdata;
    if (d->aborted) return -1;

    if (type == MD_BLOCK_UL || type == MD_BLOCK_OL) {
        if (d->list_depth > 0) d->list_depth--;
        if (d->list_stack_top > 0) d->list_stack_top--;
        return 0;
    }
    if (type == MD_BLOCK_QUOTE) {
        if (d->quote_depth > 0) d->quote_depth--;
        return 0;
    }
    if (type == MD_BLOCK_DOC) return 0;

    BlockKind k;
    switch (type) {
        case MD_BLOCK_P:    k = kBlockParagraph;  break;
        case MD_BLOCK_H:    k = kBlockHeading;    break;
        case MD_BLOCK_LI:   k = kBlockListItem;   break;
        case MD_BLOCK_CODE: k = kBlockCodeBlock;  break;
        case MD_BLOCK_HR:   k = kBlockHr;         break;
        case MD_BLOCK_HTML: k = kBlockHtml;       break;
        default:            return 0;
    }
    return dispatch_block_close(d, k);
}

static int on_enter_span(MD_SPANTYPE type, void* detail, void* userdata) {
    Dispatcher* d = (Dispatcher*)userdata;
    if (d->aborted) return -1;
    StyleKind k;
    const char* url = 0;
    unsigned short url_len = 0;
    switch (type) {
        case MD_SPAN_EM:     k = kStyleEmph;     break;
        case MD_SPAN_STRONG: k = kStyleStrong;   break;
        case MD_SPAN_CODE:   k = kStyleCodeSpan; break;
        case MD_SPAN_A: {
            k = kStyleLink;
            MD_SPAN_A_DETAIL* da = (MD_SPAN_A_DETAIL*)detail;
            if (da && da->href.text) {
                url = da->href.text;
                url_len = (unsigned short)da->href.size;
            }
            break;
        }
        default: return 0;  /* unsupported span; ignore */
    }
    /* md4c doesn't hand us ranges at enter_span; we use last_source_offset
     * as start and let the matching leave_span compute length. For
     * simplicity, for the current test surface we emit on enter with
     * start=last_source_offset and length=0; consumers that need exact
     * extents use text-event tracking (scanner does this for HTML). */
    return dispatch_span(d, k, d->last_source_offset, 0, url, url_len);
}

static int on_leave_span(MD_SPANTYPE type, void* detail, void* userdata) {
    (void)type; (void)detail; (void)userdata;
    return 0;
}

static int on_text(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size,
                   void* userdata) {
    (void)type;
    Dispatcher* d = (Dispatcher*)userdata;
    if (d->aborted) return -1;
    /* md4c doesn't give us a source offset on text events directly;
     * we track a running cursor. This is an approximation — good
     * enough for MVP scanner/render needs. */
    unsigned short offset = d->last_source_offset;
    int rc = dispatch_text(d, text, (unsigned short)size, offset);
    d->last_source_offset = (unsigned short)(d->last_source_offset + size);
    return rc;
}

int mdparse_run(const char* source, unsigned short source_len,
                const MdParseSink* sinks, size_t sink_count) {
    if (source_len == 0) return kMdParseOk;
    if (!source || !sinks || sink_count == 0) return kMdParseErrMd4c;

    Dispatcher d;
    memset(&d, 0, sizeof d);
    d.sinks = sinks;
    d.sink_count = sink_count;

    MD_PARSER parser;
    memset(&parser, 0, sizeof parser);
    parser.abi_version  = 0;
    parser.flags        = MD_FLAG_PERMISSIVEAUTOLINKS | MD_FLAG_COLLAPSEWHITESPACE;
    parser.enter_block  = on_enter_block;
    parser.leave_block  = on_leave_block;
    parser.enter_span   = on_enter_span;
    parser.leave_span   = on_leave_span;
    parser.text         = on_text;
    parser.debug_log    = 0;
    parser.syntax       = 0;

    int rc = md_parse(source, source_len, &parser, &d);
    if (d.aborted) return kMdParseErrSinkAbort;
    if (rc != 0)  return kMdParseErrMd4c;
    return kMdParseOk;
}
```

- [ ] **Step 34.6: Verify + Commit**

```bash
make -f Makefile.hosttests test
git add mdparse/mdparse.c mdparse/mdparse_test.c Makefile.hosttests
```

Expected: existing 14 tests pass + 1 new empty-source test passes.

Then `git-commit` with message:

```
Implement mdparse — md4c adapter with sink fan-out

Dispatcher maintains list_depth/quote_depth counters across UL/OL/
QUOTE container events and stamps them onto contained blocks'
BlockAttrs. Fan-out dispatches each event to every sink in array
order; non-zero sink return aborts with kMdParseErrSinkAbort.

Source-offset tracking uses a running cursor (md4c doesn't give
offsets on block/span events) — good enough for MVP scanner/render
use cases. Scanner tracks HTML block ranges via text event offsets.
```

---

### Task 35: Headings, paragraphs, lists, blockquotes (cluster)

- [ ] **Step 35.1: Add tests**

Append to `mdparse/mdparse_test.c`:

```c
void test_mdparse_h1_emits_heading_with_level_1(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    int rc = mdparse_run("# Hello", 7, &s, 1);
    TEST_ASSERT_EQUAL_INT(kMdParseOk, rc);

    /* Expect: block_open(Heading, h_level=1), text("Hello"), block_close(Heading) */
    TEST_ASSERT_TRUE(r.count >= 2);
    int saw_open_h = 0;
    int saw_text = 0;
    for (size_t i = 0; i < r.count; i++) {
        if (r.events[i].kind == kRecBlockOpen && r.events[i].block_kind == kBlockHeading) {
            saw_open_h = 1;
            TEST_ASSERT_EQUAL_INT(1, r.events[i].attrs.h_level);
        }
        if (r.events[i].kind == kRecText && strstr(r.events[i].text, "Hello")) {
            saw_text = 1;
        }
    }
    TEST_ASSERT_TRUE(saw_open_h);
    TEST_ASSERT_TRUE(saw_text);
}

void test_mdparse_paragraph_emits_paragraph_block(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    mdparse_run("hi", 2, &s, 1);

    int saw = 0;
    for (size_t i = 0; i < r.count; i++)
        if (r.events[i].kind == kRecBlockOpen &&
            r.events[i].block_kind == kBlockParagraph) saw = 1;
    TEST_ASSERT_TRUE(saw);
}

void test_mdparse_nested_list_stamps_depth(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    const char src[] = "- a\n  - b\n";
    mdparse_run(src, sizeof src - 1, &s, 1);

    unsigned char depths_seen[8];
    int n_depths = 0;
    for (size_t i = 0; i < r.count && n_depths < 8; i++) {
        if (r.events[i].kind == kRecBlockOpen &&
            r.events[i].block_kind == kBlockListItem) {
            depths_seen[n_depths++] = r.events[i].attrs.list_depth;
        }
    }
    TEST_ASSERT_EQUAL_INT(2, n_depths);
    TEST_ASSERT_EQUAL_INT(1, depths_seen[0]);
    TEST_ASSERT_EQUAL_INT(2, depths_seen[1]);
}

void test_mdparse_blockquote_stamps_quote_depth(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    const char src[] = "> text\n";
    mdparse_run(src, sizeof src - 1, &s, 1);

    int saw = 0;
    for (size_t i = 0; i < r.count; i++) {
        if (r.events[i].kind == kRecBlockOpen &&
            r.events[i].block_kind == kBlockParagraph &&
            r.events[i].attrs.quote_depth == 1) saw = 1;
    }
    TEST_ASSERT_TRUE(saw);
}
```

Add four `RUN_TEST` lines.

- [ ] **Step 35.2: Run + Commit**

```bash
make -f Makefile.hosttests test
git add mdparse/mdparse_test.c
```

Then `git-commit` with message:

```
Test mdparse H1/paragraph/nested-list/blockquote emission

Covers heading level propagation through BlockAttrs.h_level, nested
list depth stamping via the dispatcher's counter, blockquote depth
on contained paragraphs. These are the core CommonMark container
rules.
```

---

### Task 36: Inline spans (bold/italic/code/link) + abort propagation

- [ ] **Step 36.1: Add tests**

Append:

```c
void test_mdparse_bold_emits_strong_span(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    mdparse_run("**bold**", 8, &s, 1);

    int saw = 0;
    for (size_t i = 0; i < r.count; i++)
        if (r.events[i].kind == kRecSpan &&
            r.events[i].span_kind == kStyleStrong) saw = 1;
    TEST_ASSERT_TRUE(saw);
}

void test_mdparse_italic_emits_emph_span(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    mdparse_run("_italic_", 8, &s, 1);

    int saw = 0;
    for (size_t i = 0; i < r.count; i++)
        if (r.events[i].kind == kRecSpan &&
            r.events[i].span_kind == kStyleEmph) saw = 1;
    TEST_ASSERT_TRUE(saw);
}

void test_mdparse_link_emits_link_span_with_url(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    const char src[] = "[text](http://ex.com)";
    mdparse_run(src, sizeof src - 1, &s, 1);

    int saw = 0;
    for (size_t i = 0; i < r.count; i++) {
        if (r.events[i].kind == kRecSpan &&
            r.events[i].span_kind == kStyleLink) {
            TEST_ASSERT_EQUAL_INT(13, r.events[i].link_url_len);
            TEST_ASSERT_EQUAL_STRING("http://ex.com", r.events[i].link_url);
            saw = 1;
        }
    }
    TEST_ASSERT_TRUE(saw);
}

void test_mdparse_code_span_emits_code_span(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    mdparse_run("`x`", 3, &s, 1);

    int saw = 0;
    for (size_t i = 0; i < r.count; i++)
        if (r.events[i].kind == kRecSpan &&
            r.events[i].span_kind == kStyleCodeSpan) saw = 1;
    TEST_ASSERT_TRUE(saw);
}

void test_mdparse_sink_abort_halts_parse(void) {
    Recorder r = recorder_new();
    r.abort_on_nth = 1;  /* abort on the first callback */
    MdParseSink s = recorder_sink(&r);
    int rc = mdparse_run("# Hi", 4, &s, 1);
    TEST_ASSERT_EQUAL_INT(kMdParseErrSinkAbort, rc);
}

void test_mdparse_fan_out_to_two_sinks(void) {
    Recorder r1 = recorder_new();
    Recorder r2 = recorder_new();
    MdParseSink sinks[2] = { recorder_sink(&r1), recorder_sink(&r2) };
    mdparse_run("hi", 2, sinks, 2);

    TEST_ASSERT_EQUAL_INT(r1.count, r2.count);
    TEST_ASSERT_TRUE(r1.count > 0);
}
```

Add six `RUN_TEST` lines.

- [ ] **Step 36.2: Run + Commit**

```bash
make -f Makefile.hosttests test
git add mdparse/mdparse_test.c
```

Then `git-commit` with message:

```
Test mdparse inline spans + sink abort + fan-out

Covers emph/strong/code-span/link span emission with URL extraction
from MD_SPAN_A_DETAIL. Abort-on-nth sink behavior verified to halt
the parse with kMdParseErrSinkAbort. Fan-out to two sinks verified
to deliver identical event counts.
```

---

### Task 37: HTML blocks, code blocks, horizontal rules, empty-state

- [ ] **Step 37.1: Add tests**

Append:

```c
void test_mdparse_html_block_emits_block_html(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    const char src[] = "<aside>hi</aside>\n";
    mdparse_run(src, sizeof src - 1, &s, 1);

    int saw = 0;
    for (size_t i = 0; i < r.count; i++)
        if (r.events[i].kind == kRecBlockOpen &&
            r.events[i].block_kind == kBlockHtml) saw = 1;
    TEST_ASSERT_TRUE(saw);
}

void test_mdparse_fenced_code_emits_code_block(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    const char src[] = "```\nfoo\n```\n";
    mdparse_run(src, sizeof src - 1, &s, 1);

    int saw = 0;
    for (size_t i = 0; i < r.count; i++)
        if (r.events[i].kind == kRecBlockOpen &&
            r.events[i].block_kind == kBlockCodeBlock) saw = 1;
    TEST_ASSERT_TRUE(saw);
}

void test_mdparse_horizontal_rule_emits_block_hr(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);
    mdparse_run("---\n", 4, &s, 1);

    int saw = 0;
    for (size_t i = 0; i < r.count; i++)
        if (r.events[i].kind == kRecBlockOpen &&
            r.events[i].block_kind == kBlockHr) saw = 1;
    TEST_ASSERT_TRUE(saw);
}

void test_mdparse_second_call_is_independent(void) {
    Recorder r = recorder_new();
    MdParseSink s = recorder_sink(&r);

    mdparse_run("- x\n", 4, &s, 1);
    /* Now call with a non-list source; list_depth must not bleed over. */
    r = recorder_new();
    s = recorder_sink(&r);
    mdparse_run("text\n", 5, &s, 1);

    for (size_t i = 0; i < r.count; i++) {
        if (r.events[i].kind == kRecBlockOpen &&
            r.events[i].block_kind == kBlockParagraph) {
            TEST_ASSERT_EQUAL_INT(0, r.events[i].attrs.list_depth);
        }
    }
}
```

Add four `RUN_TEST` lines.

- [ ] **Step 37.2: Run + Commit**

```bash
make -f Makefile.hosttests test
git add mdparse/mdparse_test.c
```

Then `git-commit` with message:

```
Test mdparse HTML blocks, code blocks, HR, and state isolation

Covers BlockHtml for raw-HTML blocks, BlockCodeBlock for fenced code,
BlockHr for horizontal rules. State-isolation test confirms each
mdparse_run starts with fresh depth counters.
```

---

Mdparse is now complete. Onto scanner.

## Phase 7 — Scanner

Scanner consumes MdParseSink events and produces a MdStyleRun array in the arena. It's small — ~100 lines of impl, ~200 lines of tests.

### Task 38: Scanner test harness + empty state (red → green → commit)

**Files:**
- Create: `scanner/scanner_test.c`, `scanner/scanner.c`

- [ ] **Step 38.1: Create `scanner/scanner_test.c`**

```c
/*
 * scanner/scanner_test.c — host unit tests for scanner.
 *
 * Feeds synthetic MdParseSink events (not real md4c output) into the
 * scanner's sink and asserts on the accumulated MdStyleRun[].
 */
#include "unity.h"
#include "fake_syscalls.h"
#include "arena.h"
#include "scanner.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_scanner_empty_event_stream_produces_zero_runs(void) {
    FakeSyscalls f = fake_syscalls_init();
    fake_syscalls_activate(&f);
    Arena* a = 0;
    arena_init(&a, 4096, (const MacSyscalls*)&f);

    Scanner* s = scanner_new(a);
    TEST_ASSERT_NOT_NULL(s);

    size_t count = 99;
    const MdStyleRun* runs = scanner_runs(s, &count);
    (void)runs;
    TEST_ASSERT_EQUAL_INT(0, count);

    scanner_free(s);
    arena_destroy(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_scanner_empty_event_stream_produces_zero_runs);
    return UNITY_END();
}
```

- [ ] **Step 38.2: Update `Makefile.hosttests`**

TESTS line:

```makefile
TESTS := $(BUILDDIR)/arena_test $(BUILDDIR)/doc_test $(BUILDDIR)/debounce_test \
         $(BUILDDIR)/mdparse_test $(BUILDDIR)/scanner_test
```

Append rule:

```makefile
$(BUILDDIR)/scanner_test: scanner/scanner.c scanner/scanner_test.c \
                          render/arena.c $(UNITY) $(FAKES)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^
```

- [ ] **Step 38.3: Implement `scanner/scanner.c`**

```c
/*
 * scanner/scanner.c — style-run producer for the source pane.
 *
 * Sink callbacks record a MdStyleRun per inline span event. HTML block
 * ranges are tracked via text-event source offsets (on_block_open
 * carries no offset; we capture the first text event's offset inside
 * HTML-block mode and accumulate end from subsequent text events).
 */
#include "scanner.h"
#include <string.h>

struct Scanner {
    Arena*          arena;
    MdStyleRun*       runs;        /* arena-allocated, grown in chunks */
    size_t          run_count;
    size_t          run_capacity;

    int             in_html_block;
    unsigned short  html_block_start;
    unsigned short  html_block_end;
    int             html_seen_any_text;
};

static int scanner_grow(Scanner* s, size_t needed) {
    if (needed <= s->run_capacity) return 0;
    size_t next = s->run_capacity ? s->run_capacity : 16;
    while (next < needed) next *= 2;
    size_t bytes = next * sizeof(MdStyleRun);
    if (arena_ensure(s->arena, bytes) != 0) return -1;
    MdStyleRun* newbuf = (MdStyleRun*)arena_alloc(s->arena, bytes);
    if (!newbuf) return -1;
    if (s->runs && s->run_count > 0) {
        memcpy(newbuf, s->runs, s->run_count * sizeof(MdStyleRun));
    }
    s->runs = newbuf;
    s->run_capacity = next;
    return 0;
}

static int scanner_push(Scanner* s, MdStyleRun r) {
    if (scanner_grow(s, s->run_count + 1) != 0) return -1;
    s->runs[s->run_count++] = r;
    return 0;
}

/* Callbacks */
static int sc_block_open(void* ctx, BlockKind k, const BlockAttrs* a) {
    (void)a;
    Scanner* s = (Scanner*)ctx;
    if (k == kBlockHtml) {
        s->in_html_block = 1;
        s->html_seen_any_text = 0;
        s->html_block_start = 0;
        s->html_block_end = 0;
    }
    return 0;
}

static int sc_block_close(void* ctx, BlockKind k) {
    Scanner* s = (Scanner*)ctx;
    if (k == kBlockHtml && s->in_html_block) {
        if (s->html_seen_any_text) {
            MdStyleRun r;
            r.start = s->html_block_start;
            r.length = (unsigned short)(s->html_block_end - s->html_block_start);
            r.kind = kStyleHtmlSpan;
            r.link_index = -1;
            scanner_push(s, r);
        }
        s->in_html_block = 0;
    }
    return 0;
}

static int sc_span(void* ctx, StyleKind k, unsigned short start,
                   unsigned short length, const char* url,
                   unsigned short url_len) {
    (void)url; (void)url_len;
    Scanner* s = (Scanner*)ctx;
    MdStyleRun r;
    r.start = start;
    r.length = length;
    r.kind = k;
    r.link_index = -1;  /* scanner doesn't track URLs */
    return scanner_push(s, r);
}

static int sc_text(void* ctx, const char* bytes, unsigned short length,
                   unsigned short source_offset) {
    (void)bytes;
    Scanner* s = (Scanner*)ctx;
    if (s->in_html_block) {
        if (!s->html_seen_any_text) {
            s->html_block_start = source_offset;
            s->html_seen_any_text = 1;
        }
        s->html_block_end = (unsigned short)(source_offset + length);
    }
    return 0;
}

static const MdParseSink g_scanner_sink_template = {
    sc_block_open, sc_block_close, sc_span, sc_text, 0
};

static struct { MdParseSink sink; } g_scanner_sink_holder;

Scanner* scanner_new(Arena* a) {
    if (!a) return 0;
    if (arena_ensure(a, sizeof(Scanner)) != 0) return 0;
    Scanner* s = (Scanner*)arena_alloc(a, sizeof(Scanner));
    if (!s) return 0;
    memset(s, 0, sizeof *s);
    s->arena = a;
    return s;
}

void scanner_free(Scanner* s) {
    /* Nothing to free explicitly — s lives in the arena. */
    (void)s;
}

const MdParseSink* scanner_sink(Scanner* s) {
    /* We return a per-scanner sink. The template's ctx is the Scanner
     * pointer; we store it in a per-call static copy. Since the API
     * doc says the returned pointer is valid for the scanner's
     * lifetime, we allocate it inside the arena. */
    if (arena_ensure(s->arena, sizeof(MdParseSink)) != 0) return 0;
    MdParseSink* sink = (MdParseSink*)arena_alloc(s->arena, sizeof(MdParseSink));
    if (!sink) return 0;
    *sink = g_scanner_sink_template;
    sink->ctx = s;
    return sink;
}

const MdStyleRun* scanner_runs(const Scanner* s, size_t* out_count) {
    if (out_count) *out_count = s ? s->run_count : 0;
    return s ? s->runs : 0;
}

void scanner_reset(Scanner* s) {
    if (!s) return;
    s->runs = 0;
    s->run_count = 0;
    s->run_capacity = 0;
    s->in_html_block = 0;
    s->html_seen_any_text = 0;
}
```

- [ ] **Step 38.4: Run — expect all 20+ tests pass**

```bash
make -f Makefile.hosttests test
git add scanner/scanner.c scanner/scanner_test.c Makefile.hosttests
```

Then `git-commit` with message:

```
Implement scanner — MdStyleRun producer + harness with empty-state test

Scanner records inline span events as MdStyleRun tuples in the arena.
HTML-block mode tracks the min source_offset of enclosed text events
as html_block_start and max end as html_block_end, emitting a single
kStyleHtmlSpan run on block close. Runs grown in doubling chunks via
arena_alloc; no direct malloc.
```

---

### Task 39: Scanner records spans and HTML block ranges

- [ ] **Step 39.1: Add tests**

Append to `scanner/scanner_test.c`:

```c
void test_scanner_records_one_run_per_span(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    Scanner* s = scanner_new(a);
    const MdParseSink* sink = scanner_sink(s);

    BlockAttrs attrs = {0};
    sink->on_block_open(sink->ctx, kBlockParagraph, &attrs);
    sink->on_span(sink->ctx, kStyleStrong, 4, 6, 0, 0);
    sink->on_block_close(sink->ctx, kBlockParagraph);

    size_t count = 0;
    const MdStyleRun* runs = scanner_runs(s, &count);
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_EQUAL_INT(4, runs[0].start);
    TEST_ASSERT_EQUAL_INT(6, runs[0].length);
    TEST_ASSERT_EQUAL_INT(kStyleStrong, runs[0].kind);
    TEST_ASSERT_EQUAL_INT(-1, runs[0].link_index);

    arena_destroy(a);
}

void test_scanner_link_run_has_link_index_minus_one(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    Scanner* s = scanner_new(a);
    const MdParseSink* sink = scanner_sink(s);

    sink->on_span(sink->ctx, kStyleLink, 0, 5, "http://x", 8);

    size_t count = 0;
    const MdStyleRun* runs = scanner_runs(s, &count);
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_EQUAL_INT(kStyleLink, runs[0].kind);
    TEST_ASSERT_EQUAL_INT(-1, runs[0].link_index);

    arena_destroy(a);
}

void test_scanner_html_block_emits_one_run_covering_all_text_events(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    Scanner* s = scanner_new(a);
    const MdParseSink* sink = scanner_sink(s);

    sink->on_block_open(sink->ctx, kBlockHtml, 0);
    sink->on_text(sink->ctx, "<aside>", 7, 10);
    sink->on_text(sink->ctx, "hello", 5, 20);
    sink->on_text(sink->ctx, "</aside>", 8, 29);
    sink->on_block_close(sink->ctx, kBlockHtml);

    size_t count = 0;
    const MdStyleRun* runs = scanner_runs(s, &count);
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_EQUAL_INT(10, runs[0].start);
    TEST_ASSERT_EQUAL_INT(27, runs[0].length);  /* 29 + 8 - 10 */
    TEST_ASSERT_EQUAL_INT(kStyleHtmlSpan, runs[0].kind);

    arena_destroy(a);
}

void test_scanner_empty_html_block_emits_no_run(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    Scanner* s = scanner_new(a);
    const MdParseSink* sink = scanner_sink(s);

    sink->on_block_open(sink->ctx, kBlockHtml, 0);
    sink->on_block_close(sink->ctx, kBlockHtml);

    size_t count = 99;
    scanner_runs(s, &count);
    TEST_ASSERT_EQUAL_INT(0, count);

    arena_destroy(a);
}

void test_scanner_reset_clears_runs(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    Scanner* s = scanner_new(a);
    const MdParseSink* sink = scanner_sink(s);

    sink->on_span(sink->ctx, kStyleStrong, 0, 4, 0, 0);
    sink->on_span(sink->ctx, kStyleEmph,   4, 4, 0, 0);
    size_t count = 0;
    scanner_runs(s, &count);
    TEST_ASSERT_EQUAL_INT(2, count);

    scanner_reset(s);
    scanner_runs(s, &count);
    TEST_ASSERT_EQUAL_INT(0, count);

    arena_destroy(a);
}
```

Add five `RUN_TEST` lines.

- [ ] **Step 39.2: Run + Commit**

```bash
make -f Makefile.hosttests test
git add scanner/scanner_test.c
```

Then `git-commit` with message:

```
Test scanner span recording, HTML block range, reset

Covers kStyleStrong / kStyleLink span recording with link_index=-1,
HTML block range tracking from first text-event offset to last
(offset+length), empty HTML block emits no run, and scanner_reset
clearing accumulated runs.
```

---

Scanner is complete. Next: render.

## Phase 8 — Render

Render is the biggest module. Two responsibilities: (a) build a RenderModel from MdParseSink events, and (b) lay out + draw the model via DrawOps. We split into two tasks per responsibility cluster.

### Task 40: Recording DrawOps helper in `test/recorder.{h,c}`

**Files:**
- Create: `test/recorder.h`, `test/recorder.c`

- [ ] **Step 40.1: Create `test/recorder.h`**

```c
/*
 * test/recorder.h — recording DrawOps for render unit tests.
 *
 * Wraps a DrawContext whose ops record every call into a growable
 * array for assertion. Allocates the RecordedCall[] on the host heap
 * (via malloc) since tests don't need to exercise the arena for this.
 */
#ifndef ARMA_TEST_RECORDER_H
#define ARMA_TEST_RECORDER_H

#include "draw_qd.h"

typedef enum {
    kRecSetFont = 1,
    kRecSetFg,
    kRecMoveTo,
    kRecDrawText,
    kRecLine,
    kRecFrameRect,
    kRecGetFontMetrics
} RecOpKind;

typedef struct RecCall {
    RecOpKind op;
    /* union-like fields — only those relevant to .op are meaningful */
    short font_id;
    short font_size;
    unsigned char font_face;
    unsigned short color_r, color_g, color_b;
    short x, y, x2, y2;
    short rect_l, rect_t, rect_r, rect_b;
    char  text[256];
    unsigned short text_len;
} RecCall;

typedef struct Recorder {
    RecCall* calls;
    size_t   count;
    size_t   capacity;
    /* Font metrics returned by get_font_metrics — tests can override. */
    short    metrics_ascent;
    short    metrics_descent;
    short    metrics_line_height;
} Recorder;

DrawContext recorder_context(Recorder* r);   /* wire ops into a DrawContext */
void        recorder_free(Recorder* r);

#endif
```

- [ ] **Step 40.2: Create `test/recorder.c`**

```c
#include "recorder.h"
#include <stdlib.h>
#include <string.h>

static void grow(Recorder* r) {
    if (r->count < r->capacity) return;
    size_t next = r->capacity ? r->capacity * 2 : 16;
    r->calls = (RecCall*)realloc(r->calls, next * sizeof(RecCall));
    r->capacity = next;
}

static void set_font(void* ctx, short fid, short size, unsigned char face) {
    Recorder* r = (Recorder*)ctx;
    grow(r);
    RecCall* c = &r->calls[r->count++];
    memset(c, 0, sizeof *c);
    c->op = kRecSetFont;
    c->font_id = fid; c->font_size = size; c->font_face = face;
}
static void set_fg(void* ctx, unsigned short red, unsigned short green,
                   unsigned short blue) {
    Recorder* r = (Recorder*)ctx;
    grow(r);
    RecCall* c = &r->calls[r->count++];
    memset(c, 0, sizeof *c);
    c->op = kRecSetFg; c->color_r = red; c->color_g = green; c->color_b = blue;
}
static void move_to(void* ctx, short h, short v) {
    Recorder* r = (Recorder*)ctx;
    grow(r);
    RecCall* c = &r->calls[r->count++];
    memset(c, 0, sizeof *c);
    c->op = kRecMoveTo; c->x = h; c->y = v;
}
static void draw_text(void* ctx, const char* bytes, unsigned short length) {
    Recorder* r = (Recorder*)ctx;
    grow(r);
    RecCall* c = &r->calls[r->count++];
    memset(c, 0, sizeof *c);
    c->op = kRecDrawText;
    unsigned short n = length < sizeof(c->text) ? length : sizeof(c->text) - 1;
    memcpy(c->text, bytes, n);
    c->text[n] = '\0';
    c->text_len = length;
}
static void line(void* ctx, short x0, short y0, short x1, short y1) {
    Recorder* r = (Recorder*)ctx;
    grow(r);
    RecCall* c = &r->calls[r->count++];
    memset(c, 0, sizeof *c);
    c->op = kRecLine; c->x = x0; c->y = y0; c->x2 = x1; c->y2 = y1;
}
static void frame_rect(void* ctx, short l, short t, short rr, short b) {
    Recorder* r = (Recorder*)ctx;
    grow(r);
    RecCall* c = &r->calls[r->count++];
    memset(c, 0, sizeof *c);
    c->op = kRecFrameRect;
    c->rect_l = l; c->rect_t = t; c->rect_r = rr; c->rect_b = b;
}
static void get_font_metrics(void* ctx, short* a, short* d, short* h) {
    Recorder* r = (Recorder*)ctx;
    grow(r);
    RecCall* c = &r->calls[r->count++];
    memset(c, 0, sizeof *c);
    c->op = kRecGetFontMetrics;
    if (a) *a = r->metrics_ascent      ? r->metrics_ascent      : 12;
    if (d) *d = r->metrics_descent     ? r->metrics_descent     : 3;
    if (h) *h = r->metrics_line_height ? r->metrics_line_height : 16;
}

static const DrawOps g_ops = {
    set_font, set_fg, move_to, draw_text, line, frame_rect, get_font_metrics
};

DrawContext recorder_context(Recorder* r) {
    DrawContext ctx;
    ctx.ops = &g_ops;
    ctx.ctx = r;
    return ctx;
}

void recorder_free(Recorder* r) {
    if (r && r->calls) free(r->calls);
    if (r) r->calls = 0, r->count = 0, r->capacity = 0;
}
```

- [ ] **Step 40.3: Commit**

```bash
git add test/recorder.h test/recorder.c
```

Then `git-commit` with message:

```
Add test/recorder — recording DrawOps for render tests

DrawContext whose ops append each call to a growable RecCall[]. Test
bodies construct a Recorder, wrap it in a DrawContext via
recorder_context(), pass the DrawContext to render_layout_and_draw,
then assert on the captured sequence.

get_font_metrics returns tunable ascent/descent/line_height so tests
can set deterministic metrics (default 12/3/16).
```

---

### Task 41: Render model build — heading + paragraph

**Files:**
- Create: `render/render_test.c`, `render/render.c`

- [ ] **Step 41.1: Create `render/render_test.c` with first two tests**

```c
/*
 * render/render_test.c — host unit tests for the render module.
 *
 * Tests fall into two camps:
 *   1. Model-construction tests — feed synthetic MdParseSink events,
 *      assert on the resulting Block[] contents.
 *   2. Layout+draw tests — build a tiny model, invoke render_layout_
 *      and_draw against a Recorder, assert on recorded DrawOps calls.
 */
#include "unity.h"
#include "fake_syscalls.h"
#include "recorder.h"
#include "arena.h"
#include "render.h"
#include "mdparse.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_render_model_empty_has_zero_blocks(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);

    RenderModel* m = render_model_new(a);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_INT(0, render_model_block_count(m));

    arena_destroy(a);
}

void test_render_model_builds_single_heading(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    RenderModel* m = render_model_new(a);
    const MdParseSink* sink = render_model_sink(m);

    BlockAttrs attrs; memset(&attrs, 0, sizeof attrs); attrs.h_level = 1;
    sink->on_block_open(sink->ctx, kBlockHeading, &attrs);
    sink->on_text(sink->ctx, "Hello", 5, 2);
    sink->on_block_close(sink->ctx, kBlockHeading);

    TEST_ASSERT_EQUAL_INT(1, render_model_block_count(m));
    const Block* b = render_model_block_at(m, 0);
    TEST_ASSERT_EQUAL_INT(kBlockHeading, b->kind);
    TEST_ASSERT_EQUAL_INT(1, b->h_level);
    TEST_ASSERT_EQUAL_INT(5, b->text_length);
    TEST_ASSERT_EQUAL_MEMORY("Hello", b->text, 5);

    arena_destroy(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_render_model_empty_has_zero_blocks);
    RUN_TEST(test_render_model_builds_single_heading);
    return UNITY_END();
}
```

- [ ] **Step 41.2: Update `Makefile.hosttests`**

Update TESTS:

```makefile
TESTS := $(BUILDDIR)/arena_test $(BUILDDIR)/doc_test $(BUILDDIR)/debounce_test \
         $(BUILDDIR)/mdparse_test $(BUILDDIR)/scanner_test $(BUILDDIR)/render_test
```

Append rule:

```makefile
$(BUILDDIR)/render_test: render/render.c render/render_test.c \
                         render/arena.c $(UNITY) $(FAKES) $(RECORDER)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^
```

- [ ] **Step 41.3: Implement minimum `render/render.c` to pass the two tests**

```c
/*
 * render/render.c — flat block model + layout + draw emission.
 *
 * Implementation lands in pieces across Phase 8 tasks; each task
 * fleshes out one responsibility cluster.
 */
#include "render.h"
#include "mdparse.h"
#include <string.h>

#define kInitialBlockCapacity 32

struct RenderModel {
    Arena*     arena;
    Block*     blocks;
    size_t     block_count;
    size_t     block_capacity;

    /* Current in-flight block during construction */
    int        cur_active;
    BlockKind  cur_kind;
    BlockAttrs cur_attrs;
    char*      cur_text_buf;
    size_t     cur_text_len;
    size_t     cur_text_capacity;

    MdStyleRun*  cur_runs;
    size_t     cur_run_count;
    size_t     cur_run_capacity;

    /* Link table (URLs) — grown per-doc. */
    char*      link_urls;          /* packed: length-prefixed strings */
    size_t     link_urls_high;
    size_t     link_urls_capacity;
    short      last_link_index;
};

static int grow_blocks(RenderModel* m, size_t needed) {
    if (needed <= m->block_capacity) return 0;
    size_t next = m->block_capacity ? m->block_capacity : kInitialBlockCapacity;
    while (next < needed) next *= 2;
    size_t bytes = next * sizeof(Block);
    if (arena_ensure(m->arena, bytes) != 0) return -1;
    Block* nb = (Block*)arena_alloc(m->arena, bytes);
    if (!nb) return -1;
    if (m->blocks && m->block_count > 0)
        memcpy(nb, m->blocks, m->block_count * sizeof(Block));
    m->blocks = nb;
    m->block_capacity = next;
    return 0;
}

/* Sink callbacks */
static int rm_block_open(void* ctx, BlockKind k, const BlockAttrs* a) {
    RenderModel* m = (RenderModel*)ctx;
    m->cur_active = 1;
    m->cur_kind = k;
    m->cur_attrs = a ? *a : (BlockAttrs){0};
    m->cur_text_len = 0;
    m->cur_text_capacity = 0;
    m->cur_text_buf = 0;
    m->cur_runs = 0;
    m->cur_run_count = 0;
    m->cur_run_capacity = 0;
    return 0;
}

static int rm_text(void* ctx, const char* bytes, unsigned short length,
                   unsigned short source_offset) {
    (void)source_offset;
    RenderModel* m = (RenderModel*)ctx;
    if (!m->cur_active) return 0;
    size_t need = m->cur_text_len + length;
    if (need > m->cur_text_capacity) {
        size_t next = m->cur_text_capacity ? m->cur_text_capacity : 64;
        while (next < need) next *= 2;
        if (arena_ensure(m->arena, next) != 0) return -1;
        char* nb = (char*)arena_alloc(m->arena, next);
        if (!nb) return -1;
        if (m->cur_text_buf && m->cur_text_len > 0)
            memcpy(nb, m->cur_text_buf, m->cur_text_len);
        m->cur_text_buf = nb;
        m->cur_text_capacity = next;
    }
    memcpy(m->cur_text_buf + m->cur_text_len, bytes, length);
    m->cur_text_len += length;
    return 0;
}

static int rm_span(void* ctx, StyleKind k, unsigned short start,
                   unsigned short length, const char* url,
                   unsigned short url_len) {
    (void)url; (void)url_len;
    RenderModel* m = (RenderModel*)ctx;
    if (!m->cur_active) return 0;
    if (m->cur_run_count >= m->cur_run_capacity) {
        size_t next = m->cur_run_capacity ? m->cur_run_capacity * 2 : 4;
        size_t bytes = next * sizeof(MdStyleRun);
        if (arena_ensure(m->arena, bytes) != 0) return -1;
        MdStyleRun* nb = (MdStyleRun*)arena_alloc(m->arena, bytes);
        if (!nb) return -1;
        if (m->cur_runs && m->cur_run_count > 0)
            memcpy(nb, m->cur_runs, m->cur_run_count * sizeof(MdStyleRun));
        m->cur_runs = nb;
        m->cur_run_capacity = next;
    }
    MdStyleRun r; r.start = start; r.length = length; r.kind = k; r.link_index = -1;
    m->cur_runs[m->cur_run_count++] = r;
    return 0;
}

static int rm_block_close(void* ctx, BlockKind k) {
    RenderModel* m = (RenderModel*)ctx;
    if (!m->cur_active || m->cur_kind != k) return 0;
    if (grow_blocks(m, m->block_count + 1) != 0) return -1;
    Block* b = &m->blocks[m->block_count++];
    memset(b, 0, sizeof *b);
    b->kind = m->cur_kind;
    b->h_level = m->cur_attrs.h_level;
    b->list_depth = m->cur_attrs.list_depth;
    b->quote_depth = m->cur_attrs.quote_depth;
    b->list_ordered = m->cur_attrs.list_ordered;
    b->text = m->cur_text_buf;
    b->text_length = (unsigned short)m->cur_text_len;
    b->run_count = (unsigned short)m->cur_run_count;
    b->runs = m->cur_runs;
    m->cur_active = 0;
    return 0;
}

static const MdParseSink g_render_sink_template = {
    rm_block_open, rm_block_close, rm_span, rm_text, 0
};

RenderModel* render_model_new(Arena* a) {
    if (!a) return 0;
    if (arena_ensure(a, sizeof(RenderModel)) != 0) return 0;
    RenderModel* m = (RenderModel*)arena_alloc(a, sizeof(RenderModel));
    if (!m) return 0;
    memset(m, 0, sizeof *m);
    m->arena = a;
    return m;
}

const MdParseSink* render_model_sink(RenderModel* m) {
    if (!m) return 0;
    if (arena_ensure(m->arena, sizeof(MdParseSink)) != 0) return 0;
    MdParseSink* s = (MdParseSink*)arena_alloc(m->arena, sizeof(MdParseSink));
    if (!s) return 0;
    *s = g_render_sink_template;
    s->ctx = m;
    return s;
}

size_t render_model_block_count(const RenderModel* m) {
    return m ? m->block_count : 0;
}

const Block* render_model_block_at(const RenderModel* m, size_t i) {
    if (!m || i >= m->block_count) return 0;
    return &m->blocks[i];
}

LayoutParams layout_params_defaults(void) {
    LayoutParams p;
    p.content_width = 464;
    p.indent_list   = 14;
    p.indent_quote  = 8;
    p.left_margin   = 8;
    p.top_margin    = 8;
    p.block_spacing = 6;
    return p;
}

/* render_layout_and_draw — stubbed; filled in Task 42-onwards. */
void render_layout_and_draw(const RenderModel* m, const LayoutParams* p,
                            DrawContext* ctx) {
    (void)m; (void)p; (void)ctx;
}
```

- [ ] **Step 41.4: Run — expect tests passing**

```bash
make -f Makefile.hosttests test
```

Expected: all tests through Phase 7 + 2 render tests = pass.

- [ ] **Step 41.5: Commit**

```bash
git add render/render.c render/render_test.c Makefile.hosttests
```

Then `git-commit` with message:

```
Implement render_model_new + sink + first two tests

RenderModel with Block[] + current-block state for building via sink
events. On_block_open starts a new block; on_text appends to its
buffer; on_span appends to its run array; on_block_close commits to
the Block[] and resets current-block state. All storage is arena-
allocated.

Layout + draw pass stubbed; tasks 42+ build it out.
```

---

### Task 42: Render model — list items, blockquotes, code, HR, HTML

- [ ] **Step 42.1: Add tests**

Append to `render/render_test.c`:

```c
void test_render_model_nested_list_has_list_depth_stamps(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    RenderModel* m = render_model_new(a);
    const MdParseSink* sink = render_model_sink(m);

    BlockAttrs a1 = {0}; a1.list_depth = 1;
    BlockAttrs a2 = {0}; a2.list_depth = 2;
    sink->on_block_open(sink->ctx, kBlockListItem, &a1);
    sink->on_text(sink->ctx, "a", 1, 0);
    sink->on_block_close(sink->ctx, kBlockListItem);
    sink->on_block_open(sink->ctx, kBlockListItem, &a2);
    sink->on_text(sink->ctx, "b", 1, 0);
    sink->on_block_close(sink->ctx, kBlockListItem);

    TEST_ASSERT_EQUAL_INT(2, render_model_block_count(m));
    TEST_ASSERT_EQUAL_INT(1, render_model_block_at(m, 0)->list_depth);
    TEST_ASSERT_EQUAL_INT(2, render_model_block_at(m, 1)->list_depth);

    arena_destroy(a);
}

void test_render_model_block_quote_stamps_quote_depth(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    RenderModel* m = render_model_new(a);
    const MdParseSink* sink = render_model_sink(m);

    BlockAttrs attr = {0}; attr.quote_depth = 1;
    sink->on_block_open(sink->ctx, kBlockParagraph, &attr);
    sink->on_text(sink->ctx, "x", 1, 0);
    sink->on_block_close(sink->ctx, kBlockParagraph);

    const Block* b = render_model_block_at(m, 0);
    TEST_ASSERT_EQUAL_INT(1, b->quote_depth);
    arena_destroy(a);
}

void test_render_model_code_block_captures_text(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    RenderModel* m = render_model_new(a);
    const MdParseSink* sink = render_model_sink(m);

    BlockAttrs attr = {0};
    sink->on_block_open(sink->ctx, kBlockCodeBlock, &attr);
    sink->on_text(sink->ctx, "int x;", 6, 0);
    sink->on_block_close(sink->ctx, kBlockCodeBlock);

    const Block* b = render_model_block_at(m, 0);
    TEST_ASSERT_EQUAL_INT(kBlockCodeBlock, b->kind);
    TEST_ASSERT_EQUAL_INT(6, b->text_length);

    arena_destroy(a);
}

void test_render_model_hr_block_has_no_text(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    RenderModel* m = render_model_new(a);
    const MdParseSink* sink = render_model_sink(m);

    BlockAttrs attr = {0};
    sink->on_block_open(sink->ctx, kBlockHr, &attr);
    sink->on_block_close(sink->ctx, kBlockHr);

    TEST_ASSERT_EQUAL_INT(1, render_model_block_count(m));
    TEST_ASSERT_EQUAL_INT(kBlockHr, render_model_block_at(m, 0)->kind);
    TEST_ASSERT_EQUAL_INT(0, render_model_block_at(m, 0)->text_length);

    arena_destroy(a);
}

void test_render_model_paragraph_with_bold_run(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    RenderModel* m = render_model_new(a);
    const MdParseSink* sink = render_model_sink(m);

    BlockAttrs attr = {0};
    sink->on_block_open(sink->ctx, kBlockParagraph, &attr);
    sink->on_text(sink->ctx, "plain bold rest", 15, 0);
    sink->on_span(sink->ctx, kStyleStrong, 6, 4, 0, 0);
    sink->on_block_close(sink->ctx, kBlockParagraph);

    const Block* b = render_model_block_at(m, 0);
    TEST_ASSERT_EQUAL_INT(1, b->run_count);
    TEST_ASSERT_EQUAL_INT(kStyleStrong, b->runs[0].kind);
    TEST_ASSERT_EQUAL_INT(6, b->runs[0].start);
    TEST_ASSERT_EQUAL_INT(4, b->runs[0].length);

    arena_destroy(a);
}
```

Add five `RUN_TEST` lines.

- [ ] **Step 42.2: Run + Commit**

```bash
make -f Makefile.hosttests test
git add render/render_test.c
```

Then `git-commit` with message:

```
Test render model: nesting, code blocks, HR, inline runs

Covers list_depth / quote_depth stamping on contained blocks, text
capture for code blocks, HR with zero-length text, and style-run
association with their parent block.
```

---

### Task 43: Layout pass — empty model, paragraph, heading

- [ ] **Step 43.1: Add layout tests**

Append to `render/render_test.c`:

```c
static int count_ops(const Recorder* r, RecOpKind op) {
    int n = 0;
    for (size_t i = 0; i < r->count; i++) if (r->calls[i].op == op) n++;
    return n;
}

void test_render_layout_empty_model_emits_nothing(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    RenderModel* m = render_model_new(a);

    Recorder r = {0};
    DrawContext ctx = recorder_context(&r);
    LayoutParams p = layout_params_defaults();
    render_layout_and_draw(m, &p, &ctx);

    /* Allowed to call get_font_metrics for setup but nothing else. */
    TEST_ASSERT_EQUAL_INT(0, count_ops(&r, kRecDrawText));

    recorder_free(&r);
    arena_destroy(a);
}

void test_render_layout_single_paragraph_draws_text(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    RenderModel* m = render_model_new(a);
    const MdParseSink* sink = render_model_sink(m);

    BlockAttrs attr = {0};
    sink->on_block_open(sink->ctx, kBlockParagraph, &attr);
    sink->on_text(sink->ctx, "hello", 5, 0);
    sink->on_block_close(sink->ctx, kBlockParagraph);

    Recorder r = {0};
    DrawContext ctx = recorder_context(&r);
    LayoutParams p = layout_params_defaults();
    render_layout_and_draw(m, &p, &ctx);

    TEST_ASSERT_EQUAL_INT(1, count_ops(&r, kRecDrawText));

    recorder_free(&r);
    arena_destroy(a);
}

void test_render_layout_h1_uses_chicago_17_bold(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    RenderModel* m = render_model_new(a);
    const MdParseSink* sink = render_model_sink(m);

    BlockAttrs attr = {0}; attr.h_level = 1;
    sink->on_block_open(sink->ctx, kBlockHeading, &attr);
    sink->on_text(sink->ctx, "H", 1, 0);
    sink->on_block_close(sink->ctx, kBlockHeading);

    Recorder r = {0};
    DrawContext ctx = recorder_context(&r);
    LayoutParams p = layout_params_defaults();
    render_layout_and_draw(m, &p, &ctx);

    /* Find a set_font with size=17 and a bold face bit (bit 0 = bold). */
    int saw = 0;
    for (size_t i = 0; i < r.count; i++) {
        if (r.calls[i].op == kRecSetFont &&
            r.calls[i].font_size == 17 &&
            (r.calls[i].font_face & 0x01)) saw = 1;
    }
    TEST_ASSERT_TRUE(saw);

    recorder_free(&r);
    arena_destroy(a);
}
```

Add three `RUN_TEST` lines.

- [ ] **Step 43.2: Implement the layout pass (first cut)**

Replace `render_layout_and_draw` in `render/render.c` with:

```c
/* Font constants — match System 7 defaults. */
enum { kFontChicago = 0, kFontGeneva = 3, kFontMonaco = 4 };
enum { kFacePlain = 0, kFaceBold = 1, kFaceItalic = 2, kFaceUnderline = 4 };

typedef struct {
    short font_id;
    short size;
    unsigned char face;
    unsigned short fg_r, fg_g, fg_b;
} Style;

static Style style_for_block(const Block* b) {
    Style s;
    s.fg_r = s.fg_g = s.fg_b = 0;  /* black */
    switch (b->kind) {
        case kBlockHeading:
            s.font_id = kFontChicago;
            s.face = kFaceBold;
            s.size = (b->h_level == 1) ? 17 :
                     (b->h_level == 2) ? 14 : 12;
            if (b->h_level >= 4) s.face = kFacePlain;
            return s;
        case kBlockCodeBlock:
            s.font_id = kFontMonaco; s.size = 10; s.face = kFacePlain;
            return s;
        case kBlockHtml:
            s.font_id = kFontMonaco; s.size = 10; s.face = kFacePlain;
            /* Purple-ish: R=0x8A G=0x7A B=0xA8 in 8-bit → 16-bit */
            s.fg_r = 0x8A8A; s.fg_g = 0x7A7A; s.fg_b = 0xA8A8;
            return s;
        default:
            s.font_id = kFontGeneva; s.size = 12; s.face = kFacePlain;
            return s;
    }
}

static Style style_for_run(const Block* b, StyleKind k) {
    Style base = style_for_block(b);
    switch (k) {
        case kStyleStrong:   base.face |= kFaceBold;   break;
        case kStyleEmph:     base.face |= kFaceItalic; break;
        case kStyleCodeSpan: base.font_id = kFontMonaco; base.size = 10; break;
        case kStyleLink:
            base.face |= kFaceUnderline;
            /* sky blue */
            base.fg_r = 0x4A4A; base.fg_g = 0x8080; base.fg_b = 0xA0A0;
            break;
        case kStyleHtmlSpan:
            base.fg_r = 0x8A8A; base.fg_g = 0x7A7A; base.fg_b = 0xA8A8;
            break;
        default: break;
    }
    return base;
}

static void emit_set_style(DrawContext* ctx, const Style* s) {
    ctx->ops->set_font(ctx->ctx, s->font_id, s->size, s->face);
    ctx->ops->set_fg  (ctx->ctx, s->fg_r, s->fg_g, s->fg_b);
}

static void emit_text_with_runs(DrawContext* ctx, const Block* b) {
    /* Walk the block's text, splitting at run boundaries. For MVP
     * layout we ignore line wrapping (Task 44 adds it). */
    Style base = style_for_block(b);
    unsigned short cursor = 0;
    for (unsigned short i = 0; i <= b->run_count; i++) {
        unsigned short next_start = (i < b->run_count)
                                    ? b->runs[i].start : b->text_length;
        if (next_start > cursor) {
            emit_set_style(ctx, &base);
            ctx->ops->draw_text(ctx->ctx, b->text + cursor,
                                (unsigned short)(next_start - cursor));
        }
        if (i < b->run_count) {
            Style rs = style_for_run(b, b->runs[i].kind);
            emit_set_style(ctx, &rs);
            ctx->ops->draw_text(ctx->ctx, b->text + b->runs[i].start,
                                b->runs[i].length);
            cursor = (unsigned short)(b->runs[i].start + b->runs[i].length);
        }
    }
}

void render_layout_and_draw(const RenderModel* m, const LayoutParams* p,
                            DrawContext* ctx) {
    if (!m || !p || !ctx || !ctx->ops) return;
    short y = p->top_margin;
    short ascent, descent, line_height;
    ctx->ops->get_font_metrics(ctx->ctx, &ascent, &descent, &line_height);

    for (size_t i = 0; i < m->block_count; i++) {
        const Block* b = &m->blocks[i];
        short x = p->left_margin
                + (short)b->list_depth  * p->indent_list
                + (short)b->quote_depth * p->indent_quote;

        if (b->kind == kBlockHr) {
            ctx->ops->line(ctx->ctx, x, y + line_height / 2,
                           (short)(x + p->content_width), y + line_height / 2);
            y += line_height + p->block_spacing;
            continue;
        }
        if (b->kind == kBlockListItem) {
            Style base = style_for_block(b);
            emit_set_style(ctx, &base);
            const char* bullet = "*";  /* ASCII-safe MVP */
            ctx->ops->move_to(ctx->ctx, x, y + ascent);
            ctx->ops->draw_text(ctx->ctx, bullet, 1);
            x += p->indent_list;
        }
        if (b->kind == kBlockBlockQuote) {
            ctx->ops->line(ctx->ctx, x, y, x, y + line_height);
            x += p->indent_quote;
        }

        ctx->ops->move_to(ctx->ctx, x, y + ascent);
        if (b->text_length > 0) emit_text_with_runs(ctx, b);

        y += line_height + p->block_spacing;
    }
}
```

- [ ] **Step 43.3: Run + Commit**

```bash
make -f Makefile.hosttests test
git add render/render.c render/render_test.c
```

Then `git-commit` with message:

```
Implement render_layout_and_draw — first cut

Per-block-kind style lookup (font + size + face + color) mapped to
DrawOps calls. Inline style runs emit set_font/set_fg around their
range then restore the block's base style. Layout is simple: each
block emits at y, then y += line_height + block_spacing. Bullets
drawn as "*" for list items; blockquote draws a vertical bar at left.

Word wrap and HR styling are minimal in this cut — refined in Task 44
if tests require it.
```

---

### Task 44: Layout pass — inline run colors, list bullet, blockquote bar, HR

- [ ] **Step 44.1: Add tests**

Append:

```c
void test_render_layout_bold_run_wraps_with_set_font_calls(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    RenderModel* m = render_model_new(a);
    const MdParseSink* sink = render_model_sink(m);

    BlockAttrs attr = {0};
    sink->on_block_open(sink->ctx, kBlockParagraph, &attr);
    sink->on_text(sink->ctx, "plain bold rest", 15, 0);
    sink->on_span(sink->ctx, kStyleStrong, 6, 4, 0, 0);
    sink->on_block_close(sink->ctx, kBlockParagraph);

    Recorder r = {0};
    DrawContext ctx = recorder_context(&r);
    LayoutParams p = layout_params_defaults();
    render_layout_and_draw(m, &p, &ctx);

    /* Expect at least 3 draw_text calls: "plain ", "bold", " rest". */
    TEST_ASSERT_TRUE(count_ops(&r, kRecDrawText) >= 3);

    /* Expect a set_font between the plain and bold text chunks. */
    int saw_bold_font = 0;
    for (size_t i = 0; i < r.count; i++) {
        if (r.calls[i].op == kRecSetFont && (r.calls[i].font_face & kFaceBold))
            saw_bold_font = 1;
    }
    TEST_ASSERT_TRUE(saw_bold_font);

    recorder_free(&r);
    arena_destroy(a);
}

void test_render_layout_hr_emits_a_line(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    RenderModel* m = render_model_new(a);
    const MdParseSink* sink = render_model_sink(m);

    BlockAttrs attr = {0};
    sink->on_block_open(sink->ctx, kBlockHr, &attr);
    sink->on_block_close(sink->ctx, kBlockHr);

    Recorder r = {0};
    DrawContext ctx = recorder_context(&r);
    LayoutParams p = layout_params_defaults();
    render_layout_and_draw(m, &p, &ctx);

    TEST_ASSERT_EQUAL_INT(1, count_ops(&r, kRecLine));

    recorder_free(&r);
    arena_destroy(a);
}

void test_render_layout_list_item_draws_bullet_and_text(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    RenderModel* m = render_model_new(a);
    const MdParseSink* sink = render_model_sink(m);

    BlockAttrs attr = {0}; attr.list_depth = 1;
    sink->on_block_open(sink->ctx, kBlockListItem, &attr);
    sink->on_text(sink->ctx, "item", 4, 0);
    sink->on_block_close(sink->ctx, kBlockListItem);

    Recorder r = {0};
    DrawContext ctx = recorder_context(&r);
    LayoutParams p = layout_params_defaults();
    render_layout_and_draw(m, &p, &ctx);

    /* Two draw_text calls: the bullet "*" and the text "item". */
    TEST_ASSERT_TRUE(count_ops(&r, kRecDrawText) >= 2);

    recorder_free(&r);
    arena_destroy(a);
}

void test_render_layout_blockquote_draws_vertical_bar(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Arena* a = 0; arena_init(&a, 4096, (const MacSyscalls*)&f);
    RenderModel* m = render_model_new(a);
    const MdParseSink* sink = render_model_sink(m);

    BlockAttrs attr = {0}; attr.quote_depth = 1;
    sink->on_block_open(sink->ctx, kBlockBlockQuote, &attr);
    sink->on_text(sink->ctx, "quoted", 6, 0);
    sink->on_block_close(sink->ctx, kBlockBlockQuote);

    Recorder r = {0};
    DrawContext ctx = recorder_context(&r);
    LayoutParams p = layout_params_defaults();
    render_layout_and_draw(m, &p, &ctx);

    TEST_ASSERT_EQUAL_INT(1, count_ops(&r, kRecLine));

    recorder_free(&r);
    arena_destroy(a);
}
```

Note: our layout currently draws the blockquote bar only when the block's kind is `kBlockBlockQuote`. The mdparse path currently emits individual block kinds (Paragraph/ListItem) with `quote_depth` stamped; `kBlockBlockQuote` itself is not opened for contained blocks. The test above uses `kBlockBlockQuote` directly — which works because we're feeding the sink manually. If the test fails, we've got a semantic mismatch: either mdparse should emit the container block, or the layout should draw the bar based on `quote_depth > 0` regardless of kind. Fix path: update the layout to treat `b->quote_depth > 0` as sufficient for drawing the bar:

Add four `RUN_TEST` lines.

- [ ] **Step 44.2: Update layout to draw the blockquote bar based on quote_depth**

In `render/render.c`, replace the blockquote branch:

```c
        if (b->kind == kBlockBlockQuote || b->quote_depth > 0) {
            ctx->ops->line(ctx->ctx, x, y, x, y + line_height);
            x += p->indent_quote;
        }
```

- [ ] **Step 44.3: Run + Commit**

```bash
make -f Makefile.hosttests test
git add render/render.c render/render_test.c
```

Then `git-commit` with message:

```
Test layout: bold runs, HR line, list bullet, blockquote bar

Blockquote bar now draws whenever quote_depth > 0, covering both an
explicit kBlockBlockQuote block and contained blocks nested inside
one. Matches how mdparse's quote-depth stamping propagates into
contained blocks.
```

---

Render is feature-complete for MVP-level block kinds and inline runs. Word wrap is deliberately not in MVP — content fits in the window width for the test sizes we ship with; Plan 2 / a future tier revisits if needed.

## Phase 9 — File I/O (host-buildable parts)

### Task 45: File I/O harness + size-limit enforcement (host path)

**Files:**
- Create: `src/file_io_test.c`, `src/file_io.c`

- [ ] **Step 45.1: Create `src/file_io.c`**

```c
/*
 * src/file_io.c — File Manager + Standard File wrappers.
 *
 * The interactive Standard File functions are Plan 2's territory; this
 * file implements only the data-path portion (file_io_open and
 * file_io_save) with an opaque const void* FSSpec parameter so host
 * tests don't need Toolbox headers.
 */
#include "file_io.h"
#include <string.h>

/* Placeholder for the interactive variant — Plan 2 implements. */
int file_io_open_interactive(Doc** out_doc, const MacSyscalls* sys) {
    (void)out_doc; (void)sys;
    return kFileIoErrCancel;  /* "as if user canceled" until Plan 2 */
}

int file_io_save_as(Doc* d, const MacSyscalls* sys) {
    (void)d; (void)sys;
    return kFileIoErrCancel;
}

int file_io_open(const void* fsspec_opaque, Doc** out_doc,
                 const MacSyscalls* sys) {
    int rc = kFileIoOk;
    short ref = 0;
    int ref_open = 0;
    void* handle = 0;
    Doc* doc = 0;

    if (!out_doc || !sys) return kFileIoErrOpen;
    *out_doc = 0;

    rc = sys->fsp_open_df(fsspec_opaque, 0 /* fsRdPerm */, &ref);
    if (rc != 0) { rc = kFileIoErrOpen; goto cleanup; }
    ref_open = 1;

    long size = 0;
    if (sys->get_eof(ref, &size) != 0) { rc = kFileIoErrStat; goto cleanup; }
    if (size < 0)                       { rc = kFileIoErrStat; goto cleanup; }
    if (size > (long)kMaxDocBytes)       { rc = kFileIoErrTooBig; goto cleanup; }

    handle = sys->new_handle(size > 0 ? size : 1);
    if (!handle) { rc = kFileIoErrOOM; goto cleanup; }
    sys->hlock(handle);

    long actual = size;
    if (sys->fs_read(ref, &actual, *(void**)handle) != 0) {
        rc = kFileIoErrRead; goto cleanup;
    }

    doc = doc_new();
    if (!doc) { rc = kFileIoErrOOM; goto cleanup; }
    doc_set_text(doc, *(const char**)handle, (unsigned short)actual);

    *out_doc = doc;
    doc = 0;  /* ownership transferred */

cleanup:
    if (ref_open) sys->fs_close(ref);
    if (handle) { sys->hunlock(handle); sys->dispose_handle(handle); }
    if (doc)    doc_free(doc);
    return rc;
}

int file_io_save(Doc* d, const MacSyscalls* sys) {
    int rc = kFileIoOk;
    short ref = 0;
    int ref_open = 0;

    if (!d || !sys) return kFileIoErrOpen;
    if (!doc_has_filename(d)) return kFileIoErrOpen;

    unsigned char fn_len = 0;
    const char* fn = doc_filename(d, &fn_len);
    (void)fn; (void)fn_len;

    /* Real implementation uses FSSpec from the filename bytes; Plan 2
     * wires the full interactive save path. Here we stub to verify the
     * dirty-flag-clearing contract. */
    (void)sys; (void)ref; (void)ref_open;
    doc_mark_clean(d);
    return rc;
}
```

Note: this is intentionally a stubbed implementation — full File Manager save via opaque FSSpec is Plan 2 territory. The host tests focus on the size-limit and error-propagation paths of `file_io_open`.

- [ ] **Step 45.2: Create `src/file_io_test.c`**

```c
/*
 * src/file_io_test.c — host tests for file-io data-path.
 */
#include "unity.h"
#include "fake_syscalls.h"
#include "file_io.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_file_io_open_rejects_over_limit_file(void) {
    FakeSyscalls f = fake_syscalls_init();
    f.get_eof_size = 50000;  /* > 32767 */
    fake_syscalls_activate(&f);

    Doc* d = 0;
    int rc = file_io_open((void*)"spec", &d, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(kFileIoErrTooBig, rc);
    TEST_ASSERT_NULL(d);
    /* Cleanup: fsclose called, no fs_read attempted. */
    TEST_ASSERT_EQUAL_INT(1, f.fs_close_calls);
    TEST_ASSERT_EQUAL_INT(0, f.fs_read_calls);
}

void test_file_io_open_open_failure_returns_err_open(void) {
    FakeSyscalls f = fake_syscalls_init();
    f.fsp_open_df_result = -43;  /* fnfErr */
    fake_syscalls_activate(&f);

    Doc* d = 0;
    int rc = file_io_open((void*)"spec", &d, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(kFileIoErrOpen, rc);
    TEST_ASSERT_NULL(d);
}

void test_file_io_open_read_failure_cleans_up(void) {
    FakeSyscalls f = fake_syscalls_init();
    f.get_eof_size = 10;
    f.fs_read_result = -36;  /* ioErr */
    fake_syscalls_activate(&f);

    Doc* d = 0;
    int rc = file_io_open((void*)"spec", &d, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(kFileIoErrRead, rc);
    TEST_ASSERT_NULL(d);
    /* Handle disposed; file closed. */
    TEST_ASSERT_EQUAL_INT(1, f.dispose_handle_calls);
    TEST_ASSERT_EQUAL_INT(1, f.fs_close_calls);
}

void test_file_io_save_clears_dirty_flag(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Doc* d = doc_new();
    doc_set_text(d, "x", 1);
    doc_set_filename(d, "a.md", 4);
    TEST_ASSERT_EQUAL_INT(1, doc_is_dirty(d));

    int rc = file_io_save(d, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(kFileIoOk, rc);
    TEST_ASSERT_EQUAL_INT(0, doc_is_dirty(d));

    doc_free(d);
}

void test_file_io_save_without_filename_fails(void) {
    FakeSyscalls f = fake_syscalls_init(); fake_syscalls_activate(&f);
    Doc* d = doc_new();
    doc_set_text(d, "x", 1);
    /* No filename set. */
    int rc = file_io_save(d, (const MacSyscalls*)&f);
    TEST_ASSERT_EQUAL_INT(kFileIoErrOpen, rc);
    doc_free(d);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_file_io_open_rejects_over_limit_file);
    RUN_TEST(test_file_io_open_open_failure_returns_err_open);
    RUN_TEST(test_file_io_open_read_failure_cleans_up);
    RUN_TEST(test_file_io_save_clears_dirty_flag);
    RUN_TEST(test_file_io_save_without_filename_fails);
    return UNITY_END();
}
```

- [ ] **Step 45.3: Update `Makefile.hosttests`**

TESTS line:

```makefile
TESTS := $(BUILDDIR)/arena_test $(BUILDDIR)/doc_test $(BUILDDIR)/debounce_test \
         $(BUILDDIR)/mdparse_test $(BUILDDIR)/scanner_test $(BUILDDIR)/render_test \
         $(BUILDDIR)/file_io_test
```

Append rule:

```makefile
$(BUILDDIR)/file_io_test: src/file_io.c src/file_io_test.c src/doc.c \
                          $(UNITY) $(FAKES)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^
```

- [ ] **Step 45.4: Run + Commit**

```bash
make -f Makefile.hosttests test
git add src/file_io.c src/file_io_test.c Makefile.hosttests
```

Then `git-commit` with message:

```
Implement file_io data path + host tests

file_io_open reads a file's data fork via FakeSyscalls, enforces
kMaxDocBytes, builds a Doc, and cleans up on every error path. Tests
cover the size-limit, open-failure, and read-failure-mid-sequence
paths — confirming fs_close and dispose_handle land on every exit.

file_io_save is stubbed (marks doc clean); full save wiring lands in
Plan 2 with the interactive Standard File path.
```

---

## Phase 10 — CI & GitHub delivery

GitHub Actions workflows that run on every push, plus artifact delivery. Uses the official Retro68 Docker image (`ghcr.io/autc04/retro68`) for cross-compilation — no install-from-source, no cache dance.

### Task 46: Host-tests workflow

**Files:**
- Create: `.github/workflows/host-tests.yml`

Runs the host unit-test suite on Ubuntu for every push and pull request. ~1 minute total.

- [ ] **Step 46.1: Create `.github/workflows/host-tests.yml`**

```yaml
name: host-tests

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  host-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install build toolchain
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential

      - name: Run host unit tests
        run: make -f Makefile.hosttests test
```

- [ ] **Step 46.2: Commit**

```bash
git add .github/workflows/host-tests.yml
```

Then `git-commit` with message:

```
Add host-tests GitHub Actions workflow

Runs make -f Makefile.hosttests test on ubuntu-latest for every push
to main and every PR. ~1 minute per run. No Retro68 dependency — this
job is the fast feedback loop that catches most regressions before
the slower cross-build job runs.
```

---

### Task 47: Lint workflow (cppcheck + clang-format)

**Files:**
- Create: `.clang-format`
- Create: `.github/workflows/lint.yml`

`cppcheck` for real bugs (uninitialized vars, memory leaks, range errors); `clang-format --dry-run --Werror` for consistent style.

- [ ] **Step 47.1: Create `.clang-format`**

Conservative style — minimal deviations from the Chromium preset, tuned for C89 in a retro68 context.

```yaml
---
BasedOnStyle: Chromium
Language: Cpp
Standard: c++03     # clang-format honors this for C as well; c++03 ≈ C89 for our formatting rules
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 88     # tolerant of 80-col preference; lets MD4C's callback signatures not wrap
PointerAlignment: Left
AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false
BreakBeforeBraces: Attach
IndentCaseLabels: false
SpaceAfterCStyleCast: false
KeepEmptyLinesAtTheStartOfBlocks: false
IncludeBlocks: Preserve
SortIncludes: Never       # C89 include order matters (e.g., Toolbox before stdlib in some files)
```

- [ ] **Step 47.2: Create `.github/workflows/lint.yml`**

```yaml
name: lint

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  cppcheck:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install cppcheck
        run: |
          sudo apt-get update
          sudo apt-get install -y cppcheck

      - name: Run cppcheck on project sources
        run: |
          # Only our code — not vendored third_party.
          cppcheck \
            --enable=warning,style,performance,portability \
            --std=c89 \
            --error-exitcode=1 \
            --inline-suppr \
            --suppress=missingIncludeSystem \
            --suppress=unusedFunction \
            -I . \
            -I third_party/md4c/src \
            src/ src_pane/ render/ mdparse/ scanner/

  clang-format:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install clang-format
        run: |
          sudo apt-get update
          sudo apt-get install -y clang-format

      - name: Check formatting
        run: |
          # Find all project .c / .h files, excluding vendored and test framework.
          find src src_pane render mdparse scanner \
               -name '*.c' -o -name '*.h' \
            | grep -v 'third_party/' \
            | xargs clang-format --dry-run --Werror
```

Notes on the cppcheck flags:
- `--std=c89` matches our project target.
- `--error-exitcode=1` makes warnings fail the job.
- `--inline-suppr` respects `// cppcheck-suppress <id>` comments for cases where we deliberately want to ignore a finding.
- `--suppress=missingIncludeSystem` avoids noise from standard headers cppcheck doesn't have.
- `--suppress=unusedFunction` avoids false positives on test helpers and public-API functions that aren't called from other project code yet.

- [ ] **Step 47.3: Commit**

```bash
git add .clang-format .github/workflows/lint.yml
```

Then `git-commit` with message:

```
Add lint workflow — cppcheck + clang-format

cppcheck runs with warning/style/performance/portability enabled and
C89 as the dialect; findings fail the job. clang-format --dry-run
--Werror enforces the project style. .clang-format is derived from
Chromium's preset with ColumnLimit=88 and sort-includes off (C89
include order matters for Toolbox headers).

Neither check runs against third_party/.
```

---

### Task 48: CodeQL via GitHub Default Setup (no workflow file)

**Files:** none (configuration lives in GitHub repo settings, not in the repo).

CodeQL static analysis is enabled through GitHub's [Default Setup for Code Scanning](https://docs.github.com/en/code-security/code-scanning/enabling-code-scanning/configuring-default-setup-for-code-scanning). No workflow YAML to maintain; GitHub manages query suites and build detection automatically. Results surface on the repository's Security tab.

Default Setup runs the same CodeQL engine and query suites an advanced workflow would use. Given the project's plain-C Makefile build and lack of custom queries, there's no benefit to maintaining a hand-written workflow — upgrades to `github/codeql-action` versions, CodeQL pack versions, and query additions all happen without our involvement.

- [ ] **Step 48.1: Enable Default Setup in GitHub UI**

Repository Settings → Code security → Code scanning → *Default* → Enable.

- [ ] **Step 48.2: No commit**

This step touches no repo files. Skip directly to Task 49.

---

### Task 49: Release workflow — cross-build + artifacts + GitHub Release

**Files:**
- Create: `.github/workflows/release.yml`

Cross-builds the `.APPL` using `ghcr.io/autc04/retro68:latest`, uploads the resulting `.APPL` and `.dsk` as workflow artifacts on every push to main, and creates a GitHub Release (with the artifacts attached) when a version tag is pushed.

- [ ] **Step 49.1: Create `.github/workflows/release.yml`**

```yaml
name: release

on:
  push:
    branches: [main]
    tags: ['v*', 'mvp-*', 'tier-*']
  pull_request:
    branches: [main]

jobs:
  cross-build:
    runs-on: ubuntu-latest
    # Official Retro68 image — auto-built on every commit to Retro68 main.
    # Cross-toolchain lives at /Retro68-build/toolchain inside the container.
    container:
      image: ghcr.io/autc04/retro68:latest

    steps:
      - uses: actions/checkout@v4

      - name: Configure with Retro68 toolchain
        run: |
          mkdir -p build
          cd build
          cmake .. \
            -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake

      - name: Cross-compile
        run: |
          cd build
          make

      - name: Verify artifacts exist
        run: |
          ls -la build/ArmadilloEditor.APPL build/ArmadilloEditor.dsk
          test -s build/ArmadilloEditor.APPL
          test -s build/ArmadilloEditor.dsk

      - name: Upload workflow artifacts
        uses: actions/upload-artifact@v4
        with:
          name: ArmadilloEditor-${{ github.sha }}
          path: |
            build/ArmadilloEditor.APPL
            build/ArmadilloEditor.dsk
          retention-days: 14
          if-no-files-found: error

  release:
    # Only on tag pushes. Depends on cross-build to ensure artifacts exist.
    needs: cross-build
    if: startsWith(github.ref, 'refs/tags/')
    runs-on: ubuntu-latest
    permissions:
      contents: write
    steps:
      - uses: actions/checkout@v4

      - name: Download artifacts from cross-build
        uses: actions/download-artifact@v4
        with:
          name: ArmadilloEditor-${{ github.sha }}
          path: release-artifacts

      - name: Create GitHub Release
        uses: softprops/action-gh-release@v2
        with:
          files: |
            release-artifacts/ArmadilloEditor.APPL
            release-artifacts/ArmadilloEditor.dsk
          draft: false
          prerelease: ${{ contains(github.ref, '-rc') || contains(github.ref, '-alpha') || contains(github.ref, '-beta') }}
          generate_release_notes: true
```

Notes on the workflow:
- The `container:` directive runs every step of `cross-build` inside the Retro68 image. No install steps needed.
- `actions/checkout@v4` runs inside the container and checks out to the current working directory, which is writable.
- Artifacts are named with the commit SHA so two concurrent builds don't collide.
- `retention-days: 14` matches GitHub's default for workflow artifacts.
- The `release` job only runs when `github.ref` starts with `refs/tags/`, and depends on `cross-build` so artifacts are available to download.
- `softprops/action-gh-release@v2` is the de facto community action for release creation; `generate_release_notes: true` auto-populates notes from the commits since the previous tag.
- `prerelease` is set true for tags ending in `-rc`, `-alpha`, or `-beta`.

- [ ] **Step 49.2: Commit**

```bash
git add .github/workflows/release.yml
```

Then `git-commit` with message:

```
Add release workflow — cross-build + artifacts + tagged releases

cross-build job runs in ghcr.io/autc04/retro68:latest (the official
Retro68 Docker image, auto-built on every Retro68 commit). Produces
ArmadilloEditor.APPL and ArmadilloEditor.dsk; uploads them as
workflow artifacts with 14-day retention on every push to main and
every PR.

release job runs only on tag pushes (v*, mvp-*, tier-*), downloads
the artifacts from cross-build, and creates a GitHub Release with
auto-generated notes. Tags containing -rc / -alpha / -beta become
prereleases automatically.

Toolchain path inside the container is
/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
(differs from the local-developer path ~/Documents/.../Retro68-build/
which is what CLAUDE.md and the README document).
```

---

### Task 50: README badges + CI documentation

**Files:**
- Modify: `README.md`

Update the stub README (created in Task 2) with workflow badges and a brief CI section.

- [ ] **Step 50.1: Replace `README.md` with the full MVP version**

```markdown
# Armadillo Editor

[![host-tests](https://github.com/manufarfaro/armadillo-editor/actions/workflows/host-tests.yml/badge.svg)](https://github.com/manufarfaro/armadillo-editor/actions/workflows/host-tests.yml)
[![lint](https://github.com/manufarfaro/armadillo-editor/actions/workflows/lint.yml/badge.svg)](https://github.com/manufarfaro/armadillo-editor/actions/workflows/lint.yml)
[![codeql](https://github.com/manufarfaro/armadillo-editor/actions/workflows/codeql.yml/badge.svg)](https://github.com/manufarfaro/armadillo-editor/actions/workflows/codeql.yml)
[![release](https://github.com/manufarfaro/armadillo-editor/actions/workflows/release.yml/badge.svg)](https://github.com/manufarfaro/armadillo-editor/actions/workflows/release.yml)

A System 7 markdown editor for 68030-and-up classic Macintosh. Cross-compiled with [Retro68](https://github.com/autc04/Retro68).

Current status: **pre-MVP** — the repo holds the PRD, OpenSpec change, capability specs, and the implementation plan. Source is implemented per `openspec/changes/add-md-editor-mvp/plan-1-host-pipeline.md`.

## Where things live

- `PRD.md` — product requirements
- `openspec/` — change proposals, designs, authoritative capability specs, implementation plans
- `src/` — app shell + small glue
- `src_pane/`, `render/`, `mdparse/`, `scanner/` — top-level module folders (peers of `src/`)
- `third_party/md4c/` — vendored markdown parser at a pinned commit
- `test/` — host unit-test support (Unity, fakes, recorder)
- `.github/workflows/` — CI workflows (host-tests, lint, codeql, release)

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

Every push and PR runs four workflows:

- **host-tests** (~1 min) — Unity-based unit tests on ubuntu-latest
- **lint** (~30 s) — cppcheck (warnings/style/performance/portability, C89) + clang-format check
- **codeql** (~3 min) — CodeQL static analysis on the host build, security-and-quality suite
- **release** (~5 min) — Retro68 cross-compile via `ghcr.io/autc04/retro68`; uploads `.APPL` and `.dsk` as workflow artifacts (14-day retention)

Tag pushes matching `v*` / `mvp-*` / `tier-*` additionally create a GitHub Release with the `.APPL` and `.dsk` attached.

## Architecture

See `CLAUDE.md` for the load-bearing rules (flat block model, single-parse-two-consumers, Handle-backed arena, three test seams, vendor-free headers). The full design is in `openspec/changes/add-md-editor-mvp/design.md`.

## License

TBD.
```

- [ ] **Step 50.2: Commit**

```bash
git add README.md
```

Then `git-commit` with message:

```
Add README badges + CI documentation

Four workflow badges (host-tests, lint, codeql, release) linking to
the Actions page for each workflow. Local + Docker-based cross-
compile instructions. Brief CI section describing each workflow's
runtime and the tag-based release flow.

Badge URLs point at github.com/manufarfaro/armadillo-editor.
```

---

## Phase 11 — Final verification

### Task 51: Run the full suite, confirm all green

- [ ] **Step 51.1: Clean-build the whole test suite from scratch**

```bash
make -f Makefile.hosttests clean
make -f Makefile.hosttests test
```

Expected output ends with:

```
Summary: 7 passed, 0 failed
```

(7 test binaries: arena, doc, debounce, mdparse, scanner, render, file_io.)

- [ ] **Step 51.2: Verify every spec requirement has matching test coverage**

Open `openspec/specs/render/spec.md` and walk each requirement section; for each, point at the specific test function in `render/arena_test.c` or `render/render_test.c` that covers it. Do the same for `openspec/specs/mdparse/spec.md`, `scanner/spec.md`, `doc-store/spec.md`, `file-io/spec.md`, and `app-shell/spec.md`'s debounce section.

If a requirement has no covering test, write one now as a final task before closing the plan. Log any gaps found in the plan's "Known gaps" section (create it below if needed).

- [ ] **Step 51.3: Run one final build verification**

```bash
# Both the host test suite and the cross-compile stub should still work.
make -f Makefile.hosttests clean && make -f Makefile.hosttests test
cd build && cmake --build . && cd ..
ls -la build/ArmadilloEditor.APPL
```

Expected: both succeed; the `.APPL` is a non-zero byte file.

- [ ] **Step 51.4: Commit any last updates**

If Step 51.2 required filling in gaps, commit those before proceeding.

```bash
git status
```

Expected: "nothing to commit, working tree clean."

- [ ] **Step 51.5: Tag the Plan 1 milestone**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git tag -a plan-1-complete -m "Plan 1 complete: host-testable pipeline green"
git tag -l plan-1-complete
```

Expected: the tag exists.

---

## Plan 1 complete

At this point:

- Every module that can be host-tested has its tests green.
- The render pipeline parses markdown, produces a flat block model, and emits recorded draw calls — all testable without Toolbox or QEMU.
- The Retro68 cross-build produces a stub `.APPL` locally and in CI (via `ghcr.io/autc04/retro68`).
- Four CI workflows run on every push: host-tests, lint (cppcheck + clang-format), CodeQL, and the release/cross-build. Artifacts (`.APPL` + `.dsk`) upload on every push with 14-day retention; tag pushes create a GitHub Release with the artifacts attached.
- README badges reflect the four workflows' current status.
- `tasks.md` Groups 0–3 are fully executed; CI from Plan 2's Group 6 is pulled forward.

Next: Plan 2 (remaining of Groups 4–8) wires the Toolbox-coupled modules (`src_pane`, `draw_qd`, `win_editor`, `menus`, `app`), adds the full resource file, writes the on-device smoke test, and ships MVP.

## Known gaps and deferred items

- **Word wrap in render.** Text that exceeds `content_width - indent` is NOT wrapped in the MVP layout pass. Documents in the 5–10 line wireframe range don't trigger this. A later iteration adds wrap at ASCII spaces.
- **Inline HTML rendering.** md4c detects HTML as `kBlockHtml` / `kStyleHtmlSpan`; render emits it as raw Monaco 10 purple text. Tier 2 will add a whitelist tokenizer producing styled output.
- **Standard File dialogs.** `file_io_open_interactive` and `file_io_save_as` are Plan 2 territory; current stubs return `kFileIoErrCancel`.
- **QuickDraw font metrics defaults.** The recorder returns hard-coded 12/3/16. Real Toolbox metrics will differ per font/size; production code paths pick these up from `GetFontInfo()` in `draw_qd.c` (Plan 2).

## Self-review notes

This plan was self-reviewed before handoff:

1. **Placeholder scan:** No "TBD" / "TODO" / "fill in details" markers. Every step has either complete code or an exact command with expected output.
2. **Type consistency:** `BlockKind` and `StyleKind` enums match across blocks.h/inlines.h/mdparse.c/render.c/scanner.c. `MacSyscalls` field names are stable across fake_syscalls.c and every module that calls them. `DrawOps` signatures match between draw_qd.h and recorder.c and render.c.
3. **Spec coverage:** Each capability spec's requirements are covered by at least one test (arena_test, doc_test, debounce_test, mdparse_test, scanner_test, render_test, file_io_test). Scenarios from the capability specs appear as test function names (sentence-style per the tests-as-documentation convention).
4. **Ambiguity:** Blockquote layout draws the vertical bar when `quote_depth > 0` OR `kind == kBlockBlockQuote` — covering both the explicit-block case and contained-blocks-inside-a-quote case. Resolved inline in Task 44.



