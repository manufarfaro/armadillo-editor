# Delta Spec: scanner

## ADDED Requirements

All requirements in `openspec/specs/scanner/spec.md` are new additions in this change:

- **Opaque `Scanner` handle** — declared in `scanner/scanner.h`; public header includes only `<stddef.h>`, `render/inlines.h`, `mdparse/mdparse.h`, `render/arena.h`.
- **Arena-backed allocation** — all `MdStyleRun` storage from a caller-provided `Arena*`; no direct `malloc` / `NewHandle`.
- **Sink interface** — `scanner_sink()` returns an `MdParseSink` that records `MdStyleRun` tuples from span events.
- **Run accumulation and retrieval** — `scanner_runs()` returns the accumulated array in input order.
- **Reset** — `scanner_reset()` clears recorded runs without freeing arena.
- **Nested span handling** — one run per span event; no merging; source pane's apply policy decides composition.
- **HTML span / block handling** — `kStyleHtmlSpan` runs for both inline HTML spans and entire `kBlockHtml` blocks (tracked via `on_block_open` / `on_block_close` pair).
- **Link index** — scanner-recorded link runs use `link_index = -1` (URLs are not rendered differently in the source pane).
- **Host testability** — synthetic `MdParseSink` events bypass md4c entirely.
