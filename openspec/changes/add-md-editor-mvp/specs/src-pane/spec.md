# Delta Spec: src-pane

## ADDED Requirements

All requirements in `openspec/specs/src-pane/spec.md` are new additions in this change:

- **Opaque `SrcPane` handle** — declared in `src_pane/src_pane.h`; header is vendor-free (no Toolbox / TE types).
- **Lifecycle** — `src_pane_new` / `src_pane_free`; `window_ref` passed as `void*` to avoid leaking `WindowPtr`.
- **Text get/set** — `src_pane_get_text` / `src_pane_set_text`.
- **Style-run application** — `src_pane_apply_runs` translates project `MdStyleRun` values to the underlying engine's style-application calls per a fixed mapping (Monaco 10, with color/face varying by `StyleKind`).
- **Selection** — `src_pane_get_selection` / `src_pane_set_selection`; inclusive-start, exclusive-end.
- **Event delegation** — `src_pane_on_mouse_down` / `src_pane_on_key` / `src_pane_on_activate` / `src_pane_on_update` / `src_pane_on_idle` wrapping the corresponding TE calls.
- **Edit notification** — `src_pane_set_edit_callback` fires on every buffer mutation.
- **Host testability boundary** — module is target-only; `src_pane.c` is verified via on-device smoke test, not host unit tests.

## Initial Implementation Notes

MVP internals use Mac Toolbox `TextEdit` (`TEHandle`, `TESetStyle`, `TEKey`, etc.) entirely inside `src_pane.c`. No TE types cross the module boundary. The module is designed for its internals to be replaced with a custom piece-table engine in a future change — see `src_pane/README.md`.
