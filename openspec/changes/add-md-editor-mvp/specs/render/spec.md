# Delta Spec: render

## ADDED Requirements

All requirements in `openspec/specs/render/spec.md` are new additions in this change:

- **Arena allocator** — `Handle`-backed, `HLock`-for-life, 4-byte-aligned bump allocator; grow-before-parse via `arena_ensure`; doubling-to-64KB-then-linear-32KB growth; 512 KB hard cap; reset-not-free semantics.
- **Flat block model** — `RenderModel` holds a flat `Block[]` array; nesting in `list_depth` and `quote_depth` scalars; no tree.
- **Style runs per block** — arena-allocated `MdStyleRun[]` per block with offsets relative to `Block.text`.
- **Link table** — arena-allocated URL strings indexed by `MdStyleRun.link_index`.
- **`DrawOps` vtable** — all graphical output through the vtable; production wrapping real QuickDraw lives in `src/draw_qd.c`; tests wrap a recorder.
- **Block layout and drawing** — per-block-kind font / face / color / chrome mapping per the table in the authoritative spec; horizontal offset = `indent_list * list_depth + indent_quote * quote_depth + left_margin`.
- **Inline style run application** — `set_font` / `set_fg` before each run's text, restore default after.
- **Word wrap within `content_width`** — wraps at ASCII spaces; advances `line_height` per visual line.
- **Layout parameters** — `LayoutParams` struct + `layout_params_defaults()` helper.
- **Host testability via recording emitter** — full renderer runs on host without Toolbox.
- **Error reporting** — `RenderError` enum; arena OOM surfaces through `mdparse_run`'s return code.

## Initial Implementation Notes

HTML blocks and inline HTML spans are rendered as raw text in Monaco 10 purple in MVP. The whitelist renderer that produces styled `<aside>` boxes, nested semantic tags, etc., arrives in Tier 2 (`add-html-whitelist-renderer` change).
