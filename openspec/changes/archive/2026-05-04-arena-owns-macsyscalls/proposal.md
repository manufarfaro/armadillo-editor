# Proposal: Arena owns MacSyscalls by value

## Intent

Eliminate the lifetime-coupling bug class CodeQL flagged at `render/arena.c:44`, where the heap-allocated `Arena` retained a `const MacSyscalls*` pointing at caller-owned storage (potentially on the stack in host tests).

Replace the borrowed pointer with a private by-value snapshot owned by the `Arena`. After init, the `Arena` no longer references the caller's `MacSyscalls` storage in any way.

This is a follow-up to `add-md-editor-mvp` (Plan 1), addressing one of the GitHub Advanced Security findings on PR #1.

## Scope

### In scope

- Change `Arena`'s internal `sys` field from `const MacSyscalls*` to `MacSyscalls` (by value), keeping the `arena_init` signature unchanged.
- Rewrite all internal call sites in `render/arena.c` from `a->sys->foo()` to `a->sys.foo()`.
- Audit `src/debounce.c` and `src/file_io.c` to confirm neither retains `sys` past a single call. Add a one-line comment to each public function pinning the per-call contract.
- Update `CLAUDE.md` and `openspec/changes/add-md-editor-mvp/design.md` §4 with the per-call vs. long-lived storage rule.
- Verify all host tests still pass with no test-code changes.

### Out of scope

- `test/fake_syscalls.c`'s `g_current` singleton — separate CodeQL alert, separate bug class, dismissed via UI.
- `SrcPane` (Plan 2 of `add-md-editor-mvp`) — the convention update declares its expected storage shape; the actual code lands when `SrcPane` is built.
- AddressSanitizer in CI — was option #3 in the earlier triage; not bundled here.
- `arena_init` parameter signature — stays `const MacSyscalls* sys` so existing call sites do not change.

## Why now

CodeQL's Default Setup raised the alert on PR #1's first scan. Two viable responses:

1. Dismiss as "Won't fix" with a documented lifetime contract.
2. Eliminate the bug class structurally so the alert disappears and future modules can't repeat the mistake.

Option 2 costs ~80 bytes per `Arena` instance (negligible against a 4 MB RAM budget) and zero test churn. The convention split it forces — per-call vs. long-lived — also clarifies the storage rule for `SrcPane` when Plan 2 lands.
