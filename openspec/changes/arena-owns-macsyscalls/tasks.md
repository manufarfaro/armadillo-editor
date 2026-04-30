# Arena Owns MacSyscalls — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Change `Arena`'s internal `sys` field from `const MacSyscalls*` to a by-value `MacSyscalls`, eliminating the dangling-pointer bug class CodeQL flagged at `render/arena.c:44`.

**Architecture:** Single-module storage refactor. `arena_init`'s public signature stays unchanged (`const MacSyscalls* sys`). Inside the function, the body copies `*sys` into the new `MacSyscalls sys;` field at init. Every internal call site in `render/arena.c` flips from `a->sys->foo()` to `a->sys.foo()`. No public-API change. No test-code change. Plus a convention-doc update splitting the "per-call APIs" from "long-lived owners" rule, and per-call audit comments in `src/debounce.c` and `src/file_io.c`.

**Tech Stack:** C89, Unity test framework, `Makefile.hosttests` for host tests, GitHub-hosted CodeQL Default Setup for the regression check.

---

## File Structure

| File | Change | Why |
|---|---|---|
| `render/arena.c` | Modify | Field type changes from pointer to by-value; ~5 call sites change `->` → `.` |
| `src/debounce.c` | Modify | Add per-call comment to each public function (no behavior change) |
| `src/file_io.c` | Modify | Add per-call comment to each public function (no behavior change) |
| `CLAUDE.md` | Modify | Convention paragraph splits per-call vs. long-lived rule |
| `openspec/changes/add-md-editor-mvp/design.md` | Modify | §4.2 struct snippet reflects new field type |

No new files. No test-file changes. No header changes.

---

## Task 1: Establish baseline (all tests green)

**Files:**
- Run-only: `Makefile.hosttests`

- [x] **Step 1: Run the full host test suite from a clean state**

```bash
cd /Users/manufarfaro/Documents/Projects/armadillo-editor
make -f Makefile.hosttests clean
make -f Makefile.hosttests test
```

Expected: every `_test` binary runs and reports `OK` with zero failures. The total run prints `------ 0 FAILURES`. If anything is red, stop and investigate before any code change — we need a clean baseline to attribute regressions to.

- [x] **Step 2: Note the test count for cross-checking later**

Read the final summary line printed by `make`. Record the total test count somewhere local. After Task 2 we re-run and confirm the same number passes.

---

## Task 2: Arena owns MacSyscalls by value

**Files:**
- Modify: `render/arena.c:14-21` (struct), `render/arena.c:44` (init body), `render/arena.c:53-54` (destroy), `render/arena.c:99-101` (ensure)

- [x] **Step 1: Change the struct field from pointer to by-value**

Edit `render/arena.c` lines 14–21. Replace:

```c
struct Arena {
    void*              backing;         /* Handle (void**) */
    char*              base;            /* = *backing while HLocked */
    size_t             size;
    size_t             high_water;
    size_t             max_ever;
    const MacSyscalls* sys;
};
```

with:

```c
struct Arena {
    void*       backing;         /* Handle (void**) */
    char*       base;            /* = *backing while HLocked */
    size_t      size;
    size_t      high_water;
    size_t      max_ever;
    MacSyscalls sys;             /* by-value copy taken at arena_init */
};
```

- [x] **Step 2: Replace the lifetime-contract comment with the new contract in `arena_init`**

Edit `render/arena.c:38-44`. Replace the entire block:

```c
    /* Caller contract: `sys` must outlive the Arena. Production wires
     * a file-scope MacSyscalls in src/app.c; tests pass a stack-local
     * FakeSyscalls that outlives every arena use in its enclosing
     * test function. CodeQL flags this as "stack address in non-local
     * memory" — that alert is a known false-positive under our
     * lifetime contract. */
    a->sys        = sys;
```

with:

```c
    /* Copy the syscall vtable into the Arena by value. After this point
     * the Arena owns its own snapshot and does not depend on `sys`'s
     * storage lifetime. The 80-byte cost (20 function pointers × 4 B
     * on 68k) is negligible against the 4 MB RAM budget and eliminates
     * the dangling-pointer bug class CodeQL flagged here. */
    a->sys = *sys;
```

- [x] **Step 3: Update every internal call site from `->` to `.`**

In `render/arena.c`, change these four lines:

| Line | Before | After |
|---|---|---|
| 53 | `a->sys->hunlock(a->backing);` | `a->sys.hunlock(a->backing);` |
| 54 | `a->sys->dispose_handle(a->backing);` | `a->sys.dispose_handle(a->backing);` |
| 99 | `a->sys->hunlock(a->backing);` | `a->sys.hunlock(a->backing);` |
| 100 | `rc = a->sys->set_handle_size(a->backing, (long)next);` | `rc = a->sys.set_handle_size(a->backing, (long)next);` |
| 101 | `a->sys->hlock(a->backing);` | `a->sys.hlock(a->backing);` |

After the edit, run `grep -n 'sys->' render/arena.c` — expected output: empty. If any `->` remains, it's a missed call site.

- [x] **Step 4: Build and run the full host test suite**

```bash
make -f Makefile.hosttests clean
make -f Makefile.hosttests test
```

Expected: the same test count as the baseline in Task 1 passes. Zero failures. Behavior is identical because the function pointers in `a->sys` are still the same addresses (copied from `*sys`); the only change is where they live (Arena's own memory vs. caller's memory).

- [x] **Step 5: Verify with the CodeQL rule offline (sanity check)**

```bash
grep -n 'const MacSyscalls\* sys;' render/arena.c
```

Expected: no matches. The field is no longer a pointer, so the CodeQL rule "Local variable address stored in non-local memory" can't trigger on this file.

- [x] **Step 6: Commit**

```bash
SSH_AUTH_SOCK="/Users/manufarfaro/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git -C /Users/manufarfaro/Documents/Projects/armadillo-editor add render/arena.c
SSH_AUTH_SOCK="/Users/manufarfaro/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git -C /Users/manufarfaro/Documents/Projects/armadillo-editor commit -m "refactor(render/arena): own MacSyscalls by value

Replace the const MacSyscalls* field with a MacSyscalls field copied
in at arena_init. After init the Arena no longer depends on the
caller's storage lifetime, eliminating the dangling-pointer bug
class CodeQL flagged at render/arena.c:44.

Public signatures unchanged. No test changes required — the function
pointers stored in the copy point at the same fakes/wrappers as the
original, so behavior is identical."
```

---

## Task 3: Audit and pin per-call contract in `src/debounce.c`

**Files:**
- Modify: `src/debounce.c:14-18` (debounce_on_edit), `src/debounce.c:20-28` (debounce_poll)

- [x] **Step 1: Confirm `sys` is not retained anywhere**

```bash
grep -n 'sys' src/debounce.c src/debounce.h
```

Expected: every reference is either a function parameter (`const MacSyscalls* sys`) or a call through it (`sys->tick_count()`). No assignment of `sys` to any field, struct, or static. If you find one, stop — the audit found a bug, escalate to a maintainer.

- [x] **Step 2: Add a one-line per-call comment to each public function**

Edit `src/debounce.c`. After line 14 (the opening brace of `debounce_on_edit`), insert:

```c
    /* sys is per-call; not retained. */
```

So the function head reads:

```c
void debounce_on_edit(DebounceState* s, const MacSyscalls* sys) {
    /* sys is per-call; not retained. */
    if (!s || !sys) return;
    s->dirty = 1;
    s->last_keystroke_tick = sys->tick_count();
}
```

Do the same for `debounce_poll` — insert the comment as the first line inside the function body:

```c
int debounce_poll(DebounceState* s, const MacSyscalls* sys) {
    /* sys is per-call; not retained. */
    unsigned long now;
    if (!s || !sys) return 0;
    if (!s->dirty) return 0;
    now = sys->tick_count();
    if (now - s->last_keystroke_tick < kParseDebounceTicks) return 0;
    s->dirty = 0;
    return 1;
}
```

- [x] **Step 3: Build and re-run the test suite**

```bash
make -f Makefile.hosttests clean
make -f Makefile.hosttests test
```

Expected: same test count passes; zero failures. Comments don't change behavior.

- [x] **Step 4: Commit**

```bash
SSH_AUTH_SOCK="/Users/manufarfaro/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git -C /Users/manufarfaro/Documents/Projects/armadillo-editor add src/debounce.c
SSH_AUTH_SOCK="/Users/manufarfaro/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git -C /Users/manufarfaro/Documents/Projects/armadillo-editor commit -m "docs(src/debounce): pin per-call contract on sys parameter

Audit confirms debounce_on_edit and debounce_poll use sys only within
the call and never retain it. Add a one-line comment to each pinning
the contract for future readers and reviewers."
```

---

## Task 4: Audit and pin per-call contract in `src/file_io.c`

**Files:**
- Modify: `src/file_io.c:22-25` (file_io_open_interactive), `src/file_io.c:27-30` (file_io_save_as), `src/file_io.c:34-76` (file_io_open), `src/file_io.c:78-94` (file_io_save)

- [x] **Step 1: Confirm `sys` is not retained anywhere**

```bash
grep -n 'sys' src/file_io.c src/file_io.h
```

Expected: every reference is a function parameter or a call through it. No assignment of `sys` to any field, struct, or static. If you find one, stop — the audit found a bug, escalate.

- [x] **Step 2: Add the per-call comment to `file_io_open_interactive`**

Edit `src/file_io.c` lines 22–25. Replace:

```c
int file_io_open_interactive(Doc** out_doc, const MacSyscalls* sys) {
    (void)out_doc; (void)sys;
    return kFileIoErrCancel;
}
```

with:

```c
int file_io_open_interactive(Doc** out_doc, const MacSyscalls* sys) {
    /* sys is per-call; not retained. */
    (void)out_doc; (void)sys;
    return kFileIoErrCancel;
}
```

- [x] **Step 3: Add the per-call comment to `file_io_save_as`**

Edit `src/file_io.c` lines 27–30. Replace:

```c
int file_io_save_as(Doc* d, const MacSyscalls* sys) {
    (void)d; (void)sys;
    return kFileIoErrCancel;
}
```

with:

```c
int file_io_save_as(Doc* d, const MacSyscalls* sys) {
    /* sys is per-call; not retained. */
    (void)d; (void)sys;
    return kFileIoErrCancel;
}
```

- [x] **Step 4: Add the per-call comment to `file_io_open`**

Edit `src/file_io.c` line 36 (right after the function's opening brace, before the local declarations). Insert the comment so the function head reads:

```c
int file_io_open(const void* fsspec_opaque, Doc** out_doc,
                 const MacSyscalls* sys) {
    /* sys is per-call; not retained. */
    int rc = kFileIoOk;
    short ref = 0;
    int ref_open = 0;
    void* handle = 0;
    Doc* doc = 0;
    long size = 0;
    long actual = 0;

    if (!out_doc || !sys) return kFileIoErrOpen;
    /* ... rest of function body unchanged ... */
```

- [x] **Step 5: Add the per-call comment to `file_io_save`**

Edit `src/file_io.c` line 79. Insert the comment as the first line inside the body. The function head should read:

```c
int file_io_save(Doc* d, const MacSyscalls* sys) {
    /* sys is per-call; not retained. */
    unsigned char fn_len = 0;
    const char* fn;

    if (!d || !sys) return kFileIoErrOpen;
    /* ... rest of function body unchanged ... */
```

- [x] **Step 6: Build and re-run the test suite**

```bash
make -f Makefile.hosttests clean
make -f Makefile.hosttests test
```

Expected: same test count passes; zero failures.

- [x] **Step 7: Commit**

```bash
SSH_AUTH_SOCK="/Users/manufarfaro/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git -C /Users/manufarfaro/Documents/Projects/armadillo-editor add src/file_io.c
SSH_AUTH_SOCK="/Users/manufarfaro/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git -C /Users/manufarfaro/Documents/Projects/armadillo-editor commit -m "docs(src/file_io): pin per-call contract on sys parameter

Audit confirms all four public file_io_* functions use sys only
within the call and never retain it. Add a one-line comment to each
pinning the contract for future readers and reviewers."
```

---

## Task 5: Update `CLAUDE.md` convention rule

**Files:**
- Modify: `CLAUDE.md` (the "Three test seams" bullet under "Architecture (the non-obvious, cross-file parts)")

- [x] **Step 1: Locate the existing rule**

Open `CLAUDE.md` and find the bullet that begins with `**Three test seams.**`. Currently it reads (one block):

```
- **Three test seams.** `MacSyscalls` (Toolbox vtable), `DrawOps` (QuickDraw vtable), `MdParseSink` (parser events). Every module that touches the OS takes `const MacSyscalls*`. The renderer calls through `DrawOps`. Scanner/render plug into `MdParseSink`. Host tests inject fakes at each seam.
```

- [x] **Step 2: Replace the bullet with the per-call vs. long-lived split**

Replace the block above with:

```
- **Three test seams.** `MacSyscalls` (Toolbox vtable), `DrawOps` (QuickDraw vtable), `MdParseSink` (parser events). The renderer calls through `DrawOps`; scanner/render plug into `MdParseSink`; host tests inject fakes at each seam. Two storage shapes for `MacSyscalls`:
  - **Per-call APIs** (functions that use `sys` only during the call and don't retain it) take `const MacSyscalls* sys` as a parameter. Cheap (4-byte arg), no lifetime issue. Examples: `debounce_on_edit`, `debounce_poll`, `file_io_open`, `file_io_save`.
  - **Long-lived owners** (heap-allocated structs that retain `sys` past their constructor's stack frame) take `const MacSyscalls* sys` as a parameter, then copy `*sys` into a `MacSyscalls` field by value at init. The struct owns its private 80-byte snapshot and is independent of the caller's storage lifetime. Examples: `Arena`, `SrcPane` (Plan 2).
```

- [x] **Step 3: Verify the file still parses as Markdown**

```bash
grep -A2 -B2 'Per-call APIs' CLAUDE.md
```

Expected: the new bullet renders correctly with no broken indentation or fence imbalance.

- [x] **Step 4: Commit**

```bash
SSH_AUTH_SOCK="/Users/manufarfaro/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git -C /Users/manufarfaro/Documents/Projects/armadillo-editor add CLAUDE.md
SSH_AUTH_SOCK="/Users/manufarfaro/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git -C /Users/manufarfaro/Documents/Projects/armadillo-editor commit -m "docs(CLAUDE.md): split MacSyscalls storage rule into per-call vs. long-lived

Replace the blanket 'every module takes const MacSyscalls*' rule with
the two-shape rule that the Arena refactor establishes: per-call APIs
take a pointer (cheap, no retention); long-lived owners copy by value
at init (own private snapshot). Future modules pick the shape that
matches what they do."
```

---

## Task 6: Update `openspec/changes/add-md-editor-mvp/design.md` §4.2

**Files:**
- Modify: `openspec/changes/add-md-editor-mvp/design.md:410-419` (the struct snippet)

- [x] **Step 1: Find the snippet at §4.2**

Open `openspec/changes/add-md-editor-mvp/design.md` and locate lines 410–419 — the `struct Arena` snippet inside the "render/arena.c — internals" code fence.

- [x] **Step 2: Replace the struct snippet to reflect by-value storage**

Replace lines 410–419:

```c
/* render/arena.c — internals */
struct Arena {
    Handle              backing;
    char*               base;      /* = *backing while HLocked */
    size_t              size;      /* current Handle size */
    size_t              high_water;
    size_t              max_ever;
    const MacSyscalls*  sys;
};
```

with:

```c
/* render/arena.c — internals */
struct Arena {
    Handle      backing;
    char*       base;      /* = *backing while HLocked */
    size_t      size;      /* current Handle size */
    size_t      high_water;
    size_t      max_ever;
    MacSyscalls sys;       /* by-value copy taken at arena_init */
};
```

- [x] **Step 3: Add a one-paragraph note to §4.2 below the code fence**

In `openspec/changes/add-md-editor-mvp/design.md`, the structure around line 425 is:

```
424	#define kArenaHardCap       (512u * 1024u)
425	```      ← closing fence of the code block
426	         ← empty line
427	### 4.3 Four policy decisions
```

Insert a new paragraph between line 425 (the closing fence) and line 427 (the next heading). The empty line at 426 stays where it is; insert the paragraph and one blank line below it, so the result reads:

```
425	```
426	
427	The `MacSyscalls` field is stored by value, not as a pointer. `arena_init` takes a `const MacSyscalls*` parameter and copies `*sys` into the field at init. After that the `Arena` owns its private 80-byte snapshot and does not depend on the caller's storage lifetime. This eliminates the lifetime-coupling bug that an earlier draft (with a `const MacSyscalls*` field) introduced — see `openspec/changes/arena-owns-macsyscalls/` for the follow-up change that established this rule.
428	
429	### 4.3 Four policy decisions
```

- [x] **Step 4: Verify the file is still well-formed Markdown**

```bash
grep -n 'MacSyscalls sys' openspec/changes/add-md-editor-mvp/design.md
```

Expected: at least one match showing the new by-value field. The struct snippet should now read `MacSyscalls sys` (not `const MacSyscalls* sys`) within §4.2.

- [x] **Step 5: Commit**

```bash
SSH_AUTH_SOCK="/Users/manufarfaro/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git -C /Users/manufarfaro/Documents/Projects/armadillo-editor add openspec/changes/add-md-editor-mvp/design.md
SSH_AUTH_SOCK="/Users/manufarfaro/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git -C /Users/manufarfaro/Documents/Projects/armadillo-editor commit -m "docs(design): reflect Arena by-value MacSyscalls storage

Update §4.2 struct snippet from const MacSyscalls* sys to MacSyscalls
sys, and add the paragraph documenting the lifetime contract change.
Cross-references the arena-owns-macsyscalls follow-up change."
```

---

## Task 7: Push and confirm CodeQL alert disappears

**Files:**
- No file changes — this task is verification only.

- [ ] **Step 1: Push the branch**

```bash
git -C /Users/manufarfaro/Documents/Projects/armadillo-editor push origin feature/plan-1-host-pipeline
```

Expected: push succeeds; PR #1 picks up the new commits automatically.

- [ ] **Step 2: Wait for CodeQL Default Setup to re-scan**

GitHub Default Setup re-runs CodeQL on every push. Watch the Actions tab on PR #1 — the "CodeQL" check appears under the conversation. Wait for it to finish (typically 2–5 minutes).

- [ ] **Step 3: Verify the `render/arena.c:44` alert is gone**

Visit: https://github.com/manufarfaro/armadillo-editor/pull/1
Look at the "Files changed" tab and the conversation tab. The previous "Local variable address stored in non-local memory" finding pinned to `render/arena.c` line 44 should no longer appear in the bot's review.

If the alert still shows, the rule may have repointed at a different line. Inspect the new finding location and adjust — but the by-value field type means the original CWE-562 trigger condition is structurally absent, so the alert genuinely should disappear.

- [ ] **Step 4: Confirm `test/fake_syscalls.c:200` is still expected (out of scope)**

The CodeQL alert at `test/fake_syscalls.c:200` (the `g_current` singleton) is the OTHER finding, out of scope for this change. It should still appear after this push, and you (the human reviewer) dismiss it via the GitHub UI as "Won't fix" with the rationale already drafted on PR #1.

If both alerts disappeared, double-check the `fake_syscalls.c:200` one — you might have accidentally fixed it too, in which case great, but we didn't intend to.
