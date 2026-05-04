# Arena owns MacSyscalls by value — Design

**Date:** 2026-04-29
**Status:** Approved, ready for implementation plan
**Scope:** `render/arena.{h,c}`, `src/debounce.c`, `src/file_io.c`, `CLAUDE.md`, `openspec/changes/add-md-editor-mvp/design.md`

## Goal

Eliminate the lifetime-coupling bug class CodeQL flagged at `render/arena.c:44`, where a heap-allocated `Arena` retained a `const MacSyscalls*` pointing at caller-owned (potentially stack-allocated) storage. Replace the borrowed pointer with a private by-value snapshot owned by the `Arena`.

## Motivation

CodeQL Default Setup raised "Local variable address stored in non-local memory" (CWE-562) on the line:

```c
a->sys = sys;   /* Arena retains caller's pointer */
```

In production code (`src/app.c`, Plan 2) the real `MacSyscalls` is file-scope `static` with program lifetime, so the bug is structurally impossible. In host tests, `FakeSyscalls` lives on the stack — currently safe because every test function outlives every `Arena` it constructs, but the contract is documented in prose, not enforced by types.

The fix removes the implicit lifetime contract. After init, the `Arena` no longer references the caller's `MacSyscalls` storage in any way.

## Architecture Change

### `render/arena.c`

Replace the pointer field with a by-value field:

```c
struct Arena {
    void*              backing;
    char*              base;
    size_t             size;
    size_t             high_water;
    size_t             max_ever;
    MacSyscalls        sys;        /* was: const MacSyscalls* sys; */
};
```

`arena_init` keeps the same signature — the parameter stays a pointer, only the storage changes:

```c
int arena_init(Arena** out, size_t initial_size, const MacSyscalls* sys) {
    /* ... allocate Arena, allocate backing Handle ... */
    a->sys = *sys;        /* copy the 80-byte vtable into Arena */
    /* ... */
}
```

Every internal use changes from `->` to `.`:

```c
a->sys.hunlock(a->backing);
a->sys.set_handle_size(a->backing, (long)next);
a->sys.hlock(a->backing);
a->sys.dispose_handle(a->backing);
```

### Memory cost

`MacSyscalls` is 20 function pointers × 4 bytes (68k pointer width) = **80 bytes per `Arena` instance**. With one `Arena` per open document and ≤3 documents in MVP, total overhead is ≤240 bytes against a 4 MB RAM budget (0.006%).

## Convention Update

The current rule in `CLAUDE.md` and `design.md` §4 — *"every module that touches the OS takes `const MacSyscalls*`"* — conflates per-call use with long-term storage. Replace it with two rules:

**Per-call APIs.** Functions that use `sys` only during the call and do not retain it take `const MacSyscalls* sys` as a parameter. Cheap (4-byte argument), no lifetime issue. Examples: `debounce_poll`, `file_io_open`, `file_io_save`.

**Long-lived owners.** Heap-allocated structs that retain `sys` past the constructor's stack frame take `const MacSyscalls* sys` as a parameter, then copy `*sys` into a `MacSyscalls` field by value at init. The struct owns its private snapshot. Examples: `Arena` (after this change), `SrcPane` (when built in Plan 2).

This documents the lifetime contract that the `Arena` change establishes, so future modules don't repeat the original mistake.

## Audit Per Module

For every existing module that takes `const MacSyscalls*`, confirm the storage shape and add a one-line comment at the top of each function pinning down the contract:

| Module | Current shape | Action |
|---|---|---|
| `render/arena.c` | retains pointer (the bug) | apply the by-value field change above |
| `src/debounce.c` | per-call only (no retention) | add `/* sys is per-call; not retained. */` to `debounce_on_edit` and `debounce_poll` |
| `src/file_io.c` | per-call only (no retention) | same comment in each public function (`file_io_open`, `file_io_open_interactive`, `file_io_save`, `file_io_save_as`) |

The comments are load-bearing only as reviewer signals — they confirm the audit happened and pin the contract for future edits.

## Test Impact

No test code changes required. Verified by reading `render/arena_test.c`, `render/render_test.c`, `scanner/scanner_test.c`:

- Every call site passes `(const MacSyscalls*)&f` — `arena_init`'s signature is unchanged.
- Tests that mutate `FakeSyscalls` state fields (e.g., `set_handle_size_fail_after`) do so **before** `arena_init`. The values are captured into the `Arena`'s copy at the dereference inside `arena_init`.
- Counter assertions (`f.new_handle_calls`, `f.dispose_handle_calls`, etc.) read state fields that the fakes mutate through the `g_current` singleton, not through the `Arena`'s copied vtable. Reads see live values; behavior is unchanged.

The build flips from green to green.

## CodeQL Outcome

- **`render/arena.c:44`** — alert disappears automatically once the field is `MacSyscalls sys;` (no pointer stored = no rule trigger).
- **`test/fake_syscalls.c:200`** — alert unchanged. Separate bug class (file-scope `g_current` singleton), out of scope here. Dismissed via the GitHub UI as "Won't fix" with the rationale already drafted on the PR.

## Out of Scope

- `fake_syscalls.c`'s `g_current` singleton stays as-is. Different bug class, different decision; UI dismissal already planned.
- `SrcPane` (Plan 2) does not get implemented here. The convention update declares its expected storage shape; the actual code lands when `SrcPane` is built.
- No unrelated refactors bundled in. No removal of `const` qualifiers, no signature churn beyond what the `Arena` field change requires.

## Acceptance Criteria

1. `Arena`'s `sys` field is `MacSyscalls`, not `const MacSyscalls*`.
2. All internal calls inside `render/arena.c` use `a->sys.foo()` syntax.
3. `make -f Makefile.hosttests test` passes with no test changes.
4. `CLAUDE.md` and `openspec/changes/add-md-editor-mvp/design.md` §4 reflect the per-call vs. long-lived rule.
5. `src/debounce.c` and `src/file_io.c` carry the per-call comment on each public function.
6. The CodeQL Default Setup re-scan no longer reports `render/arena.c:44` (`test/fake_syscalls.c:200` may persist; that's expected and dismissed separately).
