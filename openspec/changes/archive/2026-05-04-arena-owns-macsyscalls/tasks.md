# Tasks: Arena Owns MacSyscalls

Implementation checklist for the lifetime-coupling fix on `render/arena.c`. All tasks shipped on PR #1's `feat/plan-1-host-pipeline` follow-up commits and merged into main.

## 1. Implementation

- [x] 1.1 Replace `Arena`'s `const MacSyscalls* sys` field with by-value `MacSyscalls sys`; copy `*sys` into the field at `arena_init`
- [x] 1.2 Update internal call sites in `render/arena.c` from `a->sys->foo()` to `a->sys.foo()` (5 sites: 2 in `arena_destroy`, 3 in `arena_ensure`)
- [x] 1.3 Pin per-call contract in `src/debounce.c` with `/* sys is per-call; not retained. */` comments on `debounce_on_edit` and `debounce_poll`
- [x] 1.4 Pin per-call contract in `src/file_io.c` with the same comment on each of the four public functions

## 2. Documentation

- [x] 2.1 Update `CLAUDE.md` "Three test seams" bullet to split per-call APIs vs. long-lived owners
- [x] 2.2 Update `openspec/changes/add-md-editor-mvp/design.md` §4.2 to reflect by-value `Arena.sys` storage
- [x] 2.3 Cross-reference this change folder from the design.md paragraph

## 3. Verification

- [x] 3.1 Host tests still pass with no test-code changes (7 binaries green)
- [x] 3.2 Push branch to PR #1; CI green (host-tests / lint / release)
- [x] 3.3 CodeQL Default Setup re-scan no longer reports `render/arena.c:44` (verified after PR merge into main)
- [x] 3.4 Companion alert `test/fake_syscalls.c:200` confirmed out of scope (stays as separate finding)
