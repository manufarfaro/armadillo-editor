# src-pane Specification

## Purpose

Provide the editable source-text view of the markdown document. `SrcPane` is an opaque, vendor-free wrapper around whichever text engine is in use (Mac Toolbox TextEdit for MVP; a custom piece-table engine in a future tier). The rest of the codebase interacts with the source pane through this capability's public API only — never through `TEHandle`, `WEReference`, or any internal text-engine type.

This capability owns the editable text state (what the user types), the caret and selection, and the application of syntax-coloring style runs computed by the `scanner` module. It does not own document metadata (filename, dirty flag — those live in `doc-store`), parsing (`mdparse`), style-run production (`scanner`), or rendering of the Read view (`render`).

## Requirements

### Requirement: Opaque handle

The `SrcPane` type SHALL be declared as an opaque struct in `src_pane/src_pane.h`. The public header SHALL NOT include `<TextEdit.h>`, `<Windows.h>`, or any Toolbox header. Callers SHALL NOT access fields directly.

#### Scenario: Public header is vendor-free
- GIVEN `src_pane/src_pane.h`
- WHEN inspected for `#include` directives
- THEN it includes only `<stddef.h>` and project-owned headers (`render/inlines.h` for `MdStyleRun`; `src/mac_syscalls.h` for `MacSyscalls`)

#### Scenario: Caller cannot reach into the struct
- GIVEN a caller includes only `src_pane/src_pane.h`
- WHEN the caller attempts to access `SrcPane::te_handle`
- THEN compilation fails (struct is incomplete outside `src_pane.c`)

### Requirement: Lifecycle

The module SHALL provide:

```c
typedef struct SrcPaneParams {
    short left, top, right, bottom;   /* content rect */
    void* window_ref;                  /* opaque; WindowPtr under the hood */
} SrcPaneParams;

SrcPane* src_pane_new(const SrcPaneParams*, const MacSyscalls*);
void     src_pane_free(SrcPane*);
```

`src_pane_new` SHALL allocate and initialize the underlying text engine (TE in MVP), returning `NULL` on failure. `src_pane_free` SHALL release all resources, including the text-engine handle. The `window_ref` is passed as `void*` to avoid leaking `WindowPtr` into the public API; internally the module casts back to `WindowPtr`.

#### Scenario: Successful creation
- GIVEN valid `SrcPaneParams` and a `MacSyscalls` that returns success for all init calls
- WHEN `src_pane_new` is called
- THEN a non-NULL `SrcPane*` is returned

#### Scenario: Creation failure
- GIVEN `MacSyscalls.te_new` returns NULL (simulated OOM)
- WHEN `src_pane_new` is called
- THEN NULL is returned and no resources leak

### Requirement: Text get/set

The module SHALL provide text-buffer access symmetric with `doc-store`:

```c
const char* src_pane_get_text(const SrcPane*, unsigned short* out_len);
void        src_pane_set_text(SrcPane*, const char* bytes, unsigned short len);
```

`src_pane_set_text` SHALL replace the entire buffer contents, reset the selection to (0, 0), and schedule a redraw. `src_pane_get_text` SHALL return a pointer valid until the next mutation (caller copies if they need stability).

#### Scenario: Load a document into the pane
- GIVEN a newly created `SrcPane` with empty text
- WHEN `src_pane_set_text(p, "# Hello", 7)` is called
- THEN `src_pane_get_text(p, &len)` returns bytes matching "# Hello" and `len == 7`

### Requirement: Style-run application

The module SHALL accept style-run arrays from the `scanner` module and apply them to the current text buffer:

```c
void src_pane_apply_runs(SrcPane*, const MdStyleRun* runs, size_t count);
```

Each `MdStyleRun` SHALL be translated to the underlying text engine's style-application call (for TE: `TESetStyle`). The translation maps `StyleKind` values to font, face, and color per the project's style table:

| StyleKind        | Font     | Size | Face         | Color    |
|------------------|----------|------|--------------|----------|
| kStylePlain      | Monaco   | 10   | plain        | ink      |
| kStyleEmph       | Monaco   | 10   | italic       | salmon   |
| kStyleStrong     | Monaco   | 10   | bold         | teal     |
| kStyleCodeSpan   | Monaco   | 10   | plain        | teal-dk  |
| kStyleLink       | Monaco   | 10   | underline    | sky      |
| kStyleHtmlSpan   | Monaco   | 10   | plain        | purple   |

Exact values are defined in `src_pane.c` and MAY be tuned by a design iteration; the mapping itself is stable.

#### Scenario: Apply a bold run
- GIVEN a `SrcPane` with text "Hello **world** end" and a single run `{start=6, length=9, kind=kStyleStrong}`
- WHEN `src_pane_apply_runs(p, &run, 1)` is called
- THEN bytes 6..14 render in bold teal on the next redraw

#### Scenario: Apply overlapping runs
- GIVEN a `SrcPane` with text "a _b_ c" and runs `{start=2, length=3, kind=kStyleEmph}` and `{start=3, length=1, kind=kStyleStrong}`
- WHEN `src_pane_apply_runs` is called with both runs
- THEN the engine receives both style applications in array order; final visual style reflects the order-dependent outcome of the engine's policy (TE: last-applied wins per attribute)

### Requirement: Selection

The module SHALL expose caret and selection:

```c
void src_pane_get_selection(const SrcPane*, unsigned short* out_start, unsigned short* out_end);
void src_pane_set_selection(SrcPane*, unsigned short start, unsigned short end);
```

The selection is inclusive-start, exclusive-end (standard Mac convention). Start ≤ end; the engine SHALL clamp out-of-range values to the text length.

#### Scenario: Set and read selection
- GIVEN a `SrcPane` with 20 bytes of text
- WHEN `src_pane_set_selection(p, 3, 7)` is called
- THEN `src_pane_get_selection(p, &s, &e)` returns `s=3, e=7`

### Requirement: Event delegation

The editor window SHALL delegate mouseDown, keyDown, activateEvt, updateEvt, and idle events to the source pane when it is the active pane:

```c
void src_pane_on_mouse_down(SrcPane*, short h, short v, short modifiers);
void src_pane_on_key(SrcPane*, short char_code, short key_code, short modifiers);
void src_pane_on_activate(SrcPane*, int is_active);
void src_pane_on_update(SrcPane*);
void src_pane_on_idle(SrcPane*);    /* for caret blink */
```

These wrap the corresponding Toolbox calls (`TEClick`, `TEKey`, `TEActivate`/`TEDeactivate`, `TEUpdate`, `TEIdle`) on the underlying engine.

#### Scenario: Key event inserts character
- GIVEN an active `SrcPane` with caret at position 5 in text "Hello"
- WHEN `src_pane_on_key(p, 'X', 0x58, 0)` is fired
- THEN the buffer becomes "HelloX" and the caret moves to position 6

### Requirement: Edit notification

The module SHALL notify callers when the text buffer has been mutated (via key, paste, cut, clear), so the editor window can mark the document dirty and the debounce state machine can schedule a parse:

```c
typedef void (*SrcPaneEditCallback)(void* ctx);
void src_pane_set_edit_callback(SrcPane*, SrcPaneEditCallback cb, void* ctx);
```

The callback SHALL fire on every event that changes the text buffer. It MAY fire more than once for a single logical edit (e.g., a paste of 500 bytes might fire per-character in TE-backed implementations); callers MUST be idempotent-friendly.

#### Scenario: Typing fires the edit callback
- GIVEN a `SrcPane` with an edit callback registered
- WHEN `src_pane_on_key(p, 'A', 0x41, 0)` is fired
- THEN the callback fires at least once before `src_pane_on_key` returns

### Requirement: Host testability boundary

`src_pane` SHALL NOT be host-testable in MVP — its implementation depends on TE which requires a running Toolbox. The module's *interface* (header) and its callers' use of the interface ARE host-testable (callers mock `SrcPane*` as a NULL opaque pointer and verify their own logic). Implementation-level verification of `src_pane.c` is deferred to the on-device smoke test (`src/smoke_test.c`).

#### Scenario: No host unit tests exist for src_pane
- GIVEN the `Makefile.hosttests` target list
- WHEN inspected
- THEN no `build/src_pane_test` target is present

#### Scenario: Smoke test covers src_pane
- GIVEN the on-device smoke-test `.APPL` runs in the Quadra 800 VM
- WHEN the smoke test creates a `SrcPane`, types characters, applies style runs, and queries the buffer
- THEN the observed behavior matches the spec requirements above
