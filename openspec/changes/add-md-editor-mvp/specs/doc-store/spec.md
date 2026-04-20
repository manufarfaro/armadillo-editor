# Delta Spec: doc-store

## ADDED Requirements

All requirements in `openspec/specs/doc-store/spec.md` are new additions in this change:

- **Opaque `Doc` handle** — declared as opaque struct in `src/doc.h`; no field access outside `src/doc.c`.
- **Lifecycle** — `doc_new()` / `doc_free()`; `doc_free(NULL)` is safe.
- **Text buffer access** — `doc_text()` / `doc_set_text()`; set marks dirty, copies bytes into internal buffer.
- **Dirty flag** — `doc_is_dirty()`, `doc_mark_clean()`, `doc_mark_dirty()`; implicitly set by `doc_set_text`.
- **Filename association** — opaque byte-sequence storage; `doc_set_filename`, `doc_has_filename`, `doc_filename`; no `FSSpec` in public header.
- **No Toolbox dependencies** — host-buildable with `-std=c89`.
- **Bounded document size** — `kMaxDocBytes` default 32767; over-limit `doc_set_text` leaves state unchanged.
