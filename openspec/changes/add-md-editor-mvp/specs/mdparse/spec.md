# Delta Spec: mdparse

## ADDED Requirements

All requirements in `openspec/specs/mdparse/spec.md` are new additions in this change:

- **Vendor-free public interface** — `mdparse/mdparse.h` exposes no md4c types; only project-owned `BlockKind`, `StyleKind`, `BlockAttrs`, `MdParseSink`, `MdParseError`.
- **`MdParseSink` callback interface** — `on_block_open`, `on_block_close`, `on_span`, `on_text`; abort via non-zero return.
- **Fan-out to multiple sinks** — `mdparse_run(source, len, sinks, sink_count)` dispatches every event to every sink in array order.
- **Block kind mapping** — md4c `MD_BLOCK_*` to project `BlockKind` per the mapping table; `MD_BLOCK_UL/OL/QUOTE` tracked as depth counters rather than emitted events.
- **Span kind mapping** — md4c `MD_SPAN_*` to project `StyleKind`; link URL carried from `MD_SPAN_A_DETAIL`.
- **Text offsets into source** — `on_text(source_offset)` points to original source bytes (pre-unescape).
- **Error remapping** — `MdParseError` enum; md4c return codes translated at the boundary.
- **Zero state leak between calls** — no static parser state; depth counters reset per invocation.
- **Host testability** — full md4c integration tested on host with synthetic fixtures.

## Initial Implementation Notes

Vendored md4c source lives at `third_party/md4c/src/md4c.c` + `md4c.h`. Only `md4c.c` is compiled into the build (not `md4c-html.c` or the CLI). Pinned commit recorded in `third_party/md4c/COMMIT.txt`.
