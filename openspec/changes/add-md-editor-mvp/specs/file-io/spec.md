# Delta Spec: file-io

## ADDED Requirements

All requirements in `openspec/specs/file-io/spec.md` are new additions in this change:

- **`FileIoError` enum** — 9 error codes covering open/stat/too-big/OOM/read/write/close/cancel/ok.
- **Open with Standard File dialog** — `file_io_open_interactive`; filters `'TEXT'`; returns `kFileIoErrCancel` on user cancel.
- **Open by path** — `file_io_open` takes an opaque `const void*` (internally `const FSSpec*`); enforces `kMaxDocBytes` size limit; builds a Doc with the file's bytes; records the filename.
- **Save with Standard File dialog** — `file_io_save_as` calls `StandardPutFile`; on confirm delegates to `file_io_save`.
- **Save to existing filename** — `file_io_save` requires `doc_has_filename`; truncates and rewrites; clears the dirty flag on success; sets type `'TEXT'` and creator `'Arma'` on newly created files.
- **Resource cleanup on all paths** — goto-cleanup pattern; no leaks on any error path.
- **Line-ending policy** — MVP does NOT convert line endings; read and write byte-for-byte.
- **Host testability (data-path parts)** — `FakeSyscalls`-driven tests for `file_io_open` and `file_io_save` (minus the dialog calls); Standard File dialog parts verified via on-device smoke test only.
