# Plan 2a Implementation Plan — Bare Shell

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Boot a Quadra-targeted `.APPL` that opens an empty editor window with a TextEdit-backed source pane, accepts typing, closes cleanly via the window go-away box and File → Close, and quits via ⌘Q / File → Quit. No file I/O, no view toggle, no parse cycle. Ship as `v0.1.0`.

**Architecture:** Single-document Mac System 7 app. `src/app.c` owns Toolbox init, the production `MacSyscalls` struct, and the `WaitNextEvent` loop. `src/menus.c` handles menu-bar dispatch. `src/win_editor.c` owns the editor window and one `SrcPane`. `src_pane/src_pane.c` is a `TENew`-backed editable pane behind the vendor-free `src_pane.h` API. `render/draw_qd_real.c` is the real-QuickDraw `DrawOps` vtable (compiled but not yet used by anyone in 2a — `src_pane` calls TextEdit's own drawing). All modules continue the per-call vs. long-lived `MacSyscalls` rule from Plan 1.

**Tech Stack:** Retro68 GCC (m68k-apple-macos), C89, Mac Toolbox (System 7), TextEdit, QuickDraw, Standard File (only the headers in 2a), Rez, CMake. Build runs in `ghcr.io/autc04/retro68:latest`. No host tests — Plan 2a modules are Toolbox-only.

**Pragmatic deviation from `app-shell` spec:** the spec reads "every other module that touches the OS SHALL accept a `const MacSyscalls*` parameter, never calling the Toolbox directly." For Plan 2 modules (`menus`, `win_editor`, `src_pane`, `draw_qd_real`) — which are themselves Toolbox-internal-by-nature and not host-testable — direct Toolbox calls inside their `.c` files are acceptable. They still **take and pass through** `const MacSyscalls* sys` to subordinate Plan 1 modules (`file_io`, `debounce`, `arena`) so the test seam continues to work where it matters. This deviation is recorded in the design log at the bottom of `plan-2-design.md`.

---

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `armadillo.r` | Modify | Expand from SIZE-only stub to full resource set: `MBAR` 128, `MENU` 128–131 (Apple/File/Edit/View), `WIND` 128, `ALRT` 256/257, `DLOG` 256/257, `STR#` 128, `vers` 1 |
| `CMakeLists.txt` | Modify | Uncomment Plan 1 sources, add Plan 2a sources, drop `src/stub_main.c` |
| `src/stub_main.c` | Delete | Replaced by `src/app.c` |
| `src/app.c` | Create | `main()`, Toolbox init, production `MacSyscalls` struct (file-scope static, ~20 wrapper functions), main event loop, dispatch |
| `src/menus.h` | Create | `menus_install`, `menus_handle_command(menu, item, win, sys)`, `MenuCommand` enum |
| `src/menus.c` | Create | Menu install via `GetMBar`/`SetMenuBar`/`DrawMenuBar`; `menus_handle_command` dispatch table; About box (`Alert` 256) |
| `src/win_editor.h` | Create | Opaque `WinEditor*` API: `win_editor_new`, `win_editor_free`, `win_editor_handle_event`, `win_editor_close`, `win_editor_window_ref` |
| `src/win_editor.c` | Create | Owns one `SrcPane*`, opens `WIND` 128, lays out source-pane bounds, dispatches mouse/key/update/activate, by-value `MacSyscalls` field |
| `src_pane/src_pane.c` | Create | TextEdit-backed implementation of the existing `src_pane.h` API; `TENew`, `TEKey`, `TEClick`, `TEActivate`, `TEDeactivate`, `TEUpdate`, `TEDispose`; by-value `MacSyscalls` field |
| `render/draw_qd_real.h` | Create | `draw_qd_real_make_context(GrafPtr)` returning a `DrawContext` with the real QuickDraw vtable |
| `render/draw_qd_real.c` | Create | Forwarders to `SetFont`/`MoveTo`/`DrawText`/`LineTo`/`FrameRect`/`GetFontInfo` and `RGBForeColor` |
| `openspec/changes/add-md-editor-mvp/qa-checklist-2a.md` | Create | Manual QA checklist for the QEMU smoke run before tagging `v0.1.0` |

No new tests (no host-testable code). No header changes to `src_pane.h`, `mac_syscalls.h`, or any Plan 1 file.

---

## Group A — Build wiring & resources

### Task 1: Expand `armadillo.r` to the full resource set

**Files:**
- Modify: `armadillo.r`

- [ ] **Step 1: Replace the file with the expanded resource set**

The current `armadillo.r` only has `'SIZE' (-1)`. Plan 2a needs menus, a window, alerts, error strings, and version info. Replace the entire file content (preserve the existing `'SIZE'` resource verbatim) with:

```rez
/*
 * armadillo.r — Rez resource file for Armadillo Editor
 *
 * Plan 2a expands this from the SIZE-only stub to the full set required
 * for a working .APPL: MBAR + four MENUs, one WIND, two ALRT/DLOG pairs,
 * the STR# 128 error-string list per design.md §5.2, and a 'vers' (1)
 * resource for Finder Get Info.
 */

#include "Types.r"

/* Size Manager resource. (Unchanged from Plan 1.) */
resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    canBackground,
    multiFinderAware,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    is32BitCompatible,
    isHighLevelEventAware,
    localAndRemoteHLEvents,
    notStationeryAware,
    dontUseTextEditServices,
    reserved,
    reserved,
    reserved,
    2 * 1024 * 1024,
    1 * 1024 * 1024
};

/* ---------- Menu bar ---------- */

resource 'MBAR' (128) {
    { 128, 129, 130, 131 }      /* Apple, File, Edit, View */
};

resource 'MENU' (128, "Apple") {
    128, textMenuProc,
    0x7FFFFFFD,                 /* enable all but item 1 placeholder pre-init */
    enabled,
    apple,
    {
        "About Armadillo Editor…",
            noicon, nokey, nomark, plain;
        "-",
            noicon, nokey, nomark, plain;
    }
};

resource 'MENU' (129, "File") {
    129, textMenuProc,
    allEnabled,
    enabled,
    "File",
    {
        "New",          noicon, "N", nomark, plain;
        "Open…",        noicon, "O", nomark, plain;
        "-",            noicon, nokey, nomark, plain;
        "Close",        noicon, "W", nomark, plain;
        "Save",         noicon, "S", nomark, plain;
        "Save As…",     noicon, nokey, nomark, plain;
        "-",            noicon, nokey, nomark, plain;
        "Quit",         noicon, "Q", nomark, plain;
    }
};

resource 'MENU' (130, "Edit") {
    130, textMenuProc,
    allEnabled,
    enabled,
    "Edit",
    {
        "Undo",         noicon, "Z", nomark, plain;
        "-",            noicon, nokey, nomark, plain;
        "Cut",          noicon, "X", nomark, plain;
        "Copy",         noicon, "C", nomark, plain;
        "Paste",        noicon, "V", nomark, plain;
        "Clear",        noicon, nokey, nomark, plain;
        "-",            noicon, nokey, nomark, plain;
        "Select All",   noicon, "A", nomark, plain;
    }
};

resource 'MENU' (131, "View") {
    131, textMenuProc,
    allEnabled,
    enabled,
    "View",
    {
        "Source",       noicon, "1", nomark, plain;
        "Read",         noicon, "2", nomark, plain;
    }
};

/* ---------- Editor window ---------- */

resource 'WIND' (128, "Editor", purgeable) {
    {40, 40, 360, 540},         /* top, left, bottom, right (Quadra default) */
    documentProc,               /* standard document window with title bar */
    invisible,                  /* shown explicitly by win_editor_new */
    goAway,
    0x0,
    "Untitled.md"
};

/* ---------- About + unsaved-changes alerts ---------- */

resource 'DLOG' (256, "About", purgeable) {
    {80, 100, 220, 412},
    dBoxProc,
    invisible,
    noGoAway,
    0x0,
    256,                        /* shared DITL with the alert */
    "About Armadillo Editor"
};

resource 'ALRT' (256, "About", purgeable) {
    {80, 100, 220, 412},
    256,
    {
        OK, visible, silent;
        OK, visible, silent;
        OK, visible, silent;
        OK, visible, silent;
    },
    alertPositionMainScreen
};

resource 'DITL' (256, "About") {
    {
        {110, 220, 130, 290}, Button       { enabled, "OK" },
        { 16,  16,  36, 296}, StaticText   { disabled, "Armadillo Editor" },
        { 44,  16,  64, 296}, StaticText   { disabled, "A native System 7 markdown editor." },
        { 72,  16,  92, 296}, StaticText   { disabled, "Version 0.1.0 — Bare Shell" }
    }
};

resource 'ALRT' (257, "Unsaved Changes", purgeable) {
    {100, 100, 220, 460},
    257,
    {
        OK,     visible, silent;
        OK,     visible, silent;
        OK,     visible, silent;
        OK,     visible, silent;
    },
    alertPositionMainScreen
};

resource 'DITL' (257, "Unsaved Changes") {
    {
        { 76, 260,  96, 340}, Button       { enabled, "Save" },
        {  8,  72,  64, 340}, StaticText   { disabled,
            "^0 has unsaved changes. Save them before closing?" },
        { 76, 160,  96, 240}, Button       { enabled, "Don't Save" },
        { 76,  60,  96, 140}, Button       { enabled, "Cancel" }
    }
};

/* ---------- Error strings ---------- */

/* STR# 128 indices match design.md §5.2 (1-based per Mac convention). */
resource 'STR#' (128, "Error strings") {
    {
        "Required resource missing.";
        "Out of memory.";
        "Could not open the document.";
        "The document is too large (32 KB maximum).";
        "Could not read the document.";
        "Could not save the document.";
        "Markdown parse failed.";
        "Render failed.";
    }
};

/* ---------- Version (Finder Get Info) ---------- */

resource 'vers' (1, "Application version") {
    0x00, 0x10,                 /* 0.1.0 */
    development,
    0x00,
    verUS,
    "0.1.0",
    "0.1.0 — Bare Shell"
};
```

- [ ] **Step 2: Verify Rez can compile the resource file**

Build via the Retro68 container or local toolchain:

```bash
docker run --rm -v "$PWD":/work -w /work ghcr.io/autc04/retro68:latest \
  bash -c "mkdir -p build && cd build && \
    cmake .. -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake && \
    make ArmadilloEditor 2>&1 | tail -20"
```

Expected: `armadillo.r` compiles without errors. The `.APPL` is built from `src/stub_main.c` for now (still linkable).

- [ ] **Step 3: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add armadillo.r
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "feat(resources): add MBAR, menus, window, alerts, error strings, vers

Expand armadillo.r from the SIZE-only stub to the full Plan 2a
resource set: four-menu MBAR (Apple/File/Edit/View), editor window
template (WIND 128), About + unsaved-changes alert pairs (ALRT/DITL
256/257), STR# 128 error strings (per design.md §5.2 indices), and a
vers (1) resource for Finder Get Info display.

The .APPL still links from stub_main.c — wiring src/app.c happens in
the next tasks. This commit gets the resource fork ready so subsequent
code can call GetMBar(128), GetNewWindow(128, ...), Alert(256, ...),
etc. without needing simultaneous resource changes."
```

---

### Task 2: Update `CMakeLists.txt` to wire Plan 1 + Plan 2a sources

**Files:**
- Modify: `CMakeLists.txt:28-53` (the `add_application` call)

- [ ] **Step 1: Replace the source list inside `add_application(...)`**

Currently the source list has Plan 1 sources commented out and only `src/stub_main.c` active. Replace the entire `add_application(...)` block (lines 28–53) with:

```cmake
add_application(ArmadilloEditor
    # App shell (Plan 2a)
    src/app.c
    src/menus.c
    src/win_editor.c

    # Source pane (Plan 2a — TextEdit-backed)
    src_pane/src_pane.c

    # Renderer + pipeline (Plan 1)
    render/arena.c
    render/render.c
    render/draw_qd_real.c
    mdparse/mdparse.c
    scanner/scanner.c

    # Doc + state machines + file I/O (Plan 1)
    src/doc.c
    src/debounce.c
    src/file_io.c

    # Third-party
    ${MD4C_SOURCES}

    # Resources
    armadillo.r
)
```

- [ ] **Step 2: Verify the change is syntactically valid CMake**

Run cmake against the modified file (the listed `.c` files don't exist yet, but cmake's syntax check should still pass):

```bash
docker run --rm -v "$PWD":/work -w /work ghcr.io/autc04/retro68:latest \
  bash -c "mkdir -p build && cd build && \
    cmake .. -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake 2>&1 | tail -10"
```

Expected: `cmake` configures successfully. Building (`make`) will fail because the listed `.c` files don't exist — that's expected at this point. The configure step is what we're verifying.

- [ ] **Step 3: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add CMakeLists.txt
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "build(cmake): wire Plan 1 sources + Plan 2a app-shell sources

Uncomment Plan 1 sources (render, mdparse, scanner, doc, debounce,
file_io) and add Plan 2a sources (app, menus, win_editor, src_pane,
draw_qd_real). The listed .c files don't exist yet; subsequent tasks
create each file as a stub-then-real implementation. Cross-build will
fail until those land — that's expected."
```

---

## Group B — Module skeletons (compiles; no behavior)

Tasks 3–7 create one stub `.c` (and `.h` where applicable) per module, each with empty/no-op function bodies. After the group is complete, the cross-build links and produces a `.APPL` that does nothing (its `main()` returns 0 immediately). The point of this group is to lock in module boundaries and signatures before behavior is added.

### Task 3: `render/draw_qd_real.{h,c}` skeleton

**Files:**
- Create: `render/draw_qd_real.h`
- Create: `render/draw_qd_real.c`

- [ ] **Step 1: Create the header `render/draw_qd_real.h`**

```c
/*
 * render/draw_qd_real.h — real-QuickDraw DrawOps vtable.
 *
 * Production wiring of the DrawOps test seam. The vtable is filled
 * with thin forwarders to QuickDraw + the Color Manager. Used by the
 * Read pane when render_layout_and_draw runs against a real GrafPort.
 *
 * Plan 2a creates this file but does NOT yet wire it into win_editor —
 * the read pane is implemented in Plan 2b. Until then this is a
 * compile-only target so it can't drift relative to DrawOps' shape.
 */
#ifndef ARMA_DRAW_QD_REAL_H
#define ARMA_DRAW_QD_REAL_H

#include "render/draw_qd.h"

/* Build a DrawContext that draws into the supplied GrafPort.
 * The port pointer is opaque here (vendor-free header) — caller passes
 * a WindowPtr (which IS-A GrafPtr in Toolbox terms) cast to void*. */
DrawContext draw_qd_real_make_context(void* grafport_opaque);

#endif /* ARMA_DRAW_QD_REAL_H */
```

- [ ] **Step 2: Create the skeleton implementation `render/draw_qd_real.c`**

```c
/*
 * render/draw_qd_real.c — real-QuickDraw DrawOps vtable (skeleton).
 *
 * Plan 2a: DrawOps callbacks are present but no-op (they cast the
 * grafport and return). Plan 2b fills them in with real QuickDraw
 * calls (SetFont, MoveTo, DrawText, LineTo, FrameRect, GetFontInfo)
 * and RGBForeColor for set_fg. The skeleton lets us include this in
 * the build now without exercising any unfinished behaviour.
 */
#include "draw_qd_real.h"

static void real_set_font(void* ctx, short font_id, short size,
                          unsigned char face) {
    (void)ctx; (void)font_id; (void)size; (void)face;
}

static void real_set_fg(void* ctx, unsigned short red, unsigned short green,
                        unsigned short blue) {
    (void)ctx; (void)red; (void)green; (void)blue;
}

static void real_move_to(void* ctx, short h, short v) {
    (void)ctx; (void)h; (void)v;
}

static void real_draw_text(void* ctx, const char* bytes,
                           unsigned short length) {
    (void)ctx; (void)bytes; (void)length;
}

static void real_line(void* ctx, short x0, short y0, short x1, short y1) {
    (void)ctx; (void)x0; (void)y0; (void)x1; (void)y1;
}

static void real_frame_rect(void* ctx, short l, short t, short r, short b) {
    (void)ctx; (void)l; (void)t; (void)r; (void)b;
}

static void real_get_font_metrics(void* ctx, short* a, short* d, short* h) {
    (void)ctx;
    if (a) *a = 12;       /* sane defaults so render math doesn't hit zeros */
    if (d) *d = 3;
    if (h) *h = 16;
}

static const DrawOps g_real_ops = {
    real_set_font, real_set_fg, real_move_to, real_draw_text,
    real_line, real_frame_rect, real_get_font_metrics
};

DrawContext draw_qd_real_make_context(void* grafport_opaque) {
    DrawContext ctx;
    ctx.ops = &g_real_ops;
    ctx.ctx = grafport_opaque;
    return ctx;
}
```

- [ ] **Step 3: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add render/draw_qd_real.h render/draw_qd_real.c
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "feat(render): add draw_qd_real DrawOps skeleton

Stub implementation of the real-QuickDraw DrawOps vtable. Callbacks
are present but no-op; metrics return sane defaults (12/3/16) so any
math that consumes them doesn't divide by zero. Plan 2b fills these
in with real QuickDraw calls when the Read pane is wired up."
```

---

### Task 4: `src/menus.{h,c}` skeleton

**Files:**
- Create: `src/menus.h`
- Create: `src/menus.c`

- [ ] **Step 1: Create the header `src/menus.h`**

```c
/*
 * src/menus.h — menu-bar setup and command dispatch.
 *
 * Vendor-free public API. Internal implementation calls Toolbox menu
 * routines directly (GetMBar, SetMenuBar, MenuSelect, MenuKey, …) —
 * those don't cross this header.
 *
 * Menu IDs and item indices come from armadillo.r:
 *   - MBAR 128 lists MENU 128 (Apple), 129 (File), 130 (Edit), 131 (View)
 *   - File items: 1=New, 2=Open, 3=─, 4=Close, 5=Save, 6=Save As, 7=─, 8=Quit
 *   - Edit items: 1=Undo, 2=─, 3=Cut, 4=Copy, 5=Paste, 6=Clear, 7=─, 8=Select All
 *   - View items: 1=Source, 2=Read
 *
 * Plan 2a only wires File→Quit, File→New, File→Close, and Apple→About.
 * Open/Save/Save As/Edit/View land in Plan 2b.
 */
#ifndef ARMA_MENUS_H
#define ARMA_MENUS_H

#include "src/mac_syscalls.h"

struct WinEditor;     /* forward decl — opaque */
typedef struct WinEditor WinEditor;

typedef enum {
    kMenuActionNone   = 0,
    kMenuActionQuit   = 1,
    kMenuActionClose  = 2
} MenuAction;

/* Install the menu bar from MBAR 128. Call once at startup, after
 * Toolbox init. */
void menus_install(void);

/* Dispatch a packed (menu, item) selector returned from MenuSelect or
 * MenuKey. `win` may be NULL (e.g., About item with no window open).
 * Returns kMenuActionQuit when the user picked File→Quit so the event
 * loop can exit; kMenuActionClose for File→Close (caller decides
 * whether the close is allowed). */
MenuAction menus_handle_command(long menu_select, WinEditor* win,
                                const MacSyscalls* sys);

#endif /* ARMA_MENUS_H */
```

- [ ] **Step 2: Create the skeleton implementation `src/menus.c`**

```c
/*
 * src/menus.c — menu-bar setup and command dispatch (Plan 2a skeleton).
 *
 * Plan 2a wires only the bare minimum: install the menu bar from MBAR
 * 128, dispatch File→Quit / File→Close / Apple→About. All other items
 * are recognised (MenuSelect returns their IDs) but not yet acted on;
 * Plan 2b adds File→Open/Save/Save As, Edit→*, and View→Source/Read.
 */
#include "menus.h"
#include <Menus.h>

void menus_install(void) {
    /* Plan 2a Step 2 of the menu task wires this up. */
}

MenuAction menus_handle_command(long menu_select, WinEditor* win,
                                const MacSyscalls* sys) {
    (void)menu_select; (void)win; (void)sys;
    return kMenuActionNone;
}
```

- [ ] **Step 3: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add src/menus.h src/menus.c
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "feat(menus): add menus module skeleton

Stub menus_install + menus_handle_command. Header documents the
MBAR/MENU IDs from armadillo.r and the MenuAction return values.
Implementation is no-op until the wiring tasks; this commit just
makes the .c compile so app.c can include it."
```

---

### Task 5: `src_pane/src_pane.c` skeleton

**Files:**
- Create: `src_pane/src_pane.c`

- [ ] **Step 1: Create the skeleton implementation matching `src_pane.h`**

The header `src_pane/src_pane.h` already exists from Plan 1. Just create the matching `.c` with no-op bodies.

```c
/*
 * src_pane/src_pane.c — TextEdit-backed source pane (Plan 2a skeleton).
 *
 * Header lives in src_pane/src_pane.h (vendor-free; opaque SrcPane*).
 * Plan 2a's TENew + event handlers go in the wiring tasks below; this
 * file's first commit is just stubs so the .APPL links.
 *
 * Implementation choice (per CLAUDE.md): MVP uses Mac Toolbox TextEdit
 * under the hood. A future custom piece-table engine will swap in
 * behind the same opaque API without touching callers.
 */
#include "src_pane.h"
#include <stdlib.h>

struct SrcPane {
    /* Filled in by Plan 2a's TENew wiring task. */
    int placeholder;
};

SrcPane* src_pane_new(const SrcPaneParams* params, const MacSyscalls* sys) {
    (void)params; (void)sys;
    return (SrcPane*)calloc(1, sizeof(SrcPane));
}

void src_pane_free(SrcPane* p) {
    free(p);
}

const char* src_pane_get_text(const SrcPane* p, unsigned short* out_len) {
    (void)p;
    if (out_len) *out_len = 0;
    return "";
}

void src_pane_set_text(SrcPane* p, const char* bytes, unsigned short len) {
    (void)p; (void)bytes; (void)len;
}

void src_pane_apply_runs(SrcPane* p, const StyleRun* runs, size_t count) {
    (void)p; (void)runs; (void)count;
}

void src_pane_get_selection(const SrcPane* p,
                            unsigned short* out_start,
                            unsigned short* out_end) {
    (void)p;
    if (out_start) *out_start = 0;
    if (out_end)   *out_end   = 0;
}

void src_pane_set_selection(SrcPane* p,
                            unsigned short start,
                            unsigned short end) {
    (void)p; (void)start; (void)end;
}

void src_pane_on_mouse_down(SrcPane* p, short h, short v, short modifiers) {
    (void)p; (void)h; (void)v; (void)modifiers;
}

void src_pane_on_key(SrcPane* p, short char_code, short key_code,
                     short modifiers) {
    (void)p; (void)char_code; (void)key_code; (void)modifiers;
}

void src_pane_on_activate(SrcPane* p, int is_active) {
    (void)p; (void)is_active;
}

void src_pane_on_update(SrcPane* p) {
    (void)p;
}

void src_pane_on_idle(SrcPane* p) {
    (void)p;
}

void src_pane_set_edit_callback(SrcPane* p, SrcPaneEditCallback cb,
                                void* ctx) {
    (void)p; (void)cb; (void)ctx;
}
```

- [ ] **Step 2: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add src_pane/src_pane.c
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "feat(src_pane): add src_pane.c skeleton matching the existing header

Stub bodies for every entry point in src_pane.h. src_pane_new returns
a calloc'd SrcPane with a single placeholder field; src_pane_free
matches with free. Everything else is no-op. Plan 2a's TENew task
fills these in; Plan 2b adds style-run application."
```

---

### Task 6: `src/win_editor.{h,c}` skeleton

**Files:**
- Create: `src/win_editor.h`
- Create: `src/win_editor.c`

- [ ] **Step 1: Create the header `src/win_editor.h`**

```c
/*
 * src/win_editor.h — editor window (one .md document at a time).
 *
 * Vendor-free public API. Owns one Doc*, one SrcPane*, and (Plan 2b)
 * one RenderModel* + one Arena*. Holds MacSyscalls by value because
 * the editor outlives any single function call.
 *
 * Plan 2a: window opens with an empty Doc and an empty TE-backed
 * SrcPane. handle_event routes mouse/key/update/activate to the right
 * sub-component. close() and free() tear it down cleanly.
 *
 * Plan 2b adds: Doc swap on file open, parse cycle drive, view toggle,
 * unsaved-changes guard before close.
 */
#ifndef ARMA_WIN_EDITOR_H
#define ARMA_WIN_EDITOR_H

#include "src/mac_syscalls.h"

typedef struct WinEditor WinEditor;

/* Open the editor window from WIND 128 with an empty Doc.
 * Returns NULL on failure (resource missing, OOM, or TENew failure). */
WinEditor* win_editor_new(const MacSyscalls* sys);

/* Free everything: SrcPane, Doc, the window. NULL-safe. */
void win_editor_free(WinEditor* w);

/* The editor's Mac WindowPtr, type-erased. Callers cast back inside
 * their own Toolbox-aware .c file. */
void* win_editor_window_ref(const WinEditor* w);

/* Dispatch a Mac event (mouseDown/keyDown/updateEvt/activateEvt) that
 * has already been determined to belong to this window. The event
 * pointer is opaque here (vendor-free header). */
void win_editor_handle_event(WinEditor* w, const void* event_record);

/* User-initiated close. Plan 2a: just frees the window. Plan 2b: shows
 * the unsaved-changes alert if the doc is dirty. Returns 1 if the
 * close happened, 0 if the user cancelled. */
int win_editor_close(WinEditor* w);

#endif /* ARMA_WIN_EDITOR_H */
```

- [ ] **Step 2: Create the skeleton implementation `src/win_editor.c`**

```c
/*
 * src/win_editor.c — editor window (Plan 2a skeleton).
 *
 * Plan 2a fills in: WIND 128 open in win_editor_new, SrcPane creation
 * sized to the window's content area, event dispatch to the SrcPane.
 * Plan 2b layers on the parse cycle drive and view toggle.
 *
 * MacSyscalls is held by value (per the long-lived-owners convention
 * established in arena-owns-macsyscalls).
 */
#include "win_editor.h"
#include "src_pane/src_pane.h"
#include "src/doc.h"
#include <stdlib.h>

struct WinEditor {
    void*       window_ref;     /* WindowPtr, type-erased */
    Doc*        doc;
    SrcPane*    src_pane;
    MacSyscalls sys;            /* by-value snapshot */
};

WinEditor* win_editor_new(const MacSyscalls* sys) {
    WinEditor* w;
    if (!sys) return 0;
    w = (WinEditor*)calloc(1, sizeof(WinEditor));
    if (!w) return 0;
    w->sys = *sys;
    /* Plan 2a Step 13 fills in: window_ref, doc, src_pane. */
    return w;
}

void win_editor_free(WinEditor* w) {
    if (!w) return;
    if (w->src_pane) src_pane_free(w->src_pane);
    if (w->doc)      doc_free(w->doc);
    /* window_ref disposal added in the wiring task. */
    free(w);
}

void* win_editor_window_ref(const WinEditor* w) {
    return w ? w->window_ref : 0;
}

void win_editor_handle_event(WinEditor* w, const void* event_record) {
    (void)w; (void)event_record;
}

int win_editor_close(WinEditor* w) {
    (void)w;
    return 1;
}
```

- [ ] **Step 3: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add src/win_editor.h src/win_editor.c
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "feat(win_editor): add window-controller module skeleton

Vendor-free header for the per-window editor: opens WIND 128, owns
one Doc + one SrcPane, dispatches events. Skeleton .c stubs all
entry points and stores MacSyscalls by value (long-lived-owner rule).
Window/Doc/SrcPane creation is wired in Step 13."
```

---

### Task 7: `src/app.c` skeleton + delete `src/stub_main.c`

**Files:**
- Create: `src/app.c`
- Delete: `src/stub_main.c`
- Modify: `CMakeLists.txt` (no edit needed — `src/app.c` was already added in Task 2; just verify `src/stub_main.c` is no longer in the source list)

- [ ] **Step 1: Verify `src/stub_main.c` is no longer in `CMakeLists.txt`**

```bash
grep -n 'stub_main' CMakeLists.txt
```

Expected: no output. If `stub_main.c` is still listed, remove that line.

- [ ] **Step 2: Create `src/app.c` with the minimal main**

```c
/*
 * src/app.c — Armadillo Editor application shell.
 *
 * Owns Toolbox init, the production MacSyscalls struct (file-scope
 * static, ~20 thin wrappers around real Toolbox calls), the main
 * WaitNextEvent loop, and high-level event dispatch. Replaces the
 * Phase 1 stub_main.c.
 *
 * Plan 2a brings this up in pieces:
 *   - This step: minimal main() that does Toolbox init and exits.
 *   - Step 8:    populate the production MacSyscalls struct.
 *   - Step 9:    main event loop (WaitNextEvent + quit flag).
 *   - Step 10:   menu install + dispatch.
 *   - Step 11:   keyDown ⌘-shortcut routing.
 *   - Step 12:   About box on Apple → About.
 *   - Step 13:   open editor window via File → New.
 *   - Step 15:   mouseDown dispatch (menu vs. content vs. go-away).
 */
#include <Quickdraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <Menus.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Events.h>

static void toolbox_init(void) {
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(0L);
    FlushEvents(everyEvent, 0);
}

int main(void) {
    toolbox_init();
    /* Plan 2a Step 9 adds the event loop here. */
    return 0;
}
```

- [ ] **Step 3: Delete `src/stub_main.c`**

```bash
git rm src/stub_main.c
```

- [ ] **Step 4: Cross-build to verify everything links**

```bash
docker run --rm -v "$PWD":/work -w /work ghcr.io/autc04/retro68:latest \
  bash -c "rm -rf build && mkdir build && cd build && \
    cmake .. -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake && \
    make ArmadilloEditor 2>&1 | tail -20"
```

Expected: `make` succeeds. `build/ArmadilloEditor.APPL` and `build/ArmadilloEditor.dsk` are produced. The app does Toolbox init and exits — boring but linkable.

- [ ] **Step 5: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add src/app.c
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "feat(app): replace stub_main.c with src/app.c skeleton

Toolbox init (InitGraf/InitFonts/InitWindows/InitMenus/TEInit/
InitDialogs/FlushEvents) + immediate exit. Drops src/stub_main.c.
The .APPL still does nothing visible — subsequent steps add the
production MacSyscalls struct, main event loop, menu dispatch, and
the editor window. Cross-build is green at this checkpoint."
```

---

## Group C — Wire up real Toolbox behavior

Each task in this group builds on the previous skeleton(s). After each task the cross-build passes and the app's behavior advances incrementally.

### Task 8: Production `MacSyscalls` struct in `src/app.c`

**Files:**
- Modify: `src/app.c` (add wrappers + struct above `main`)

- [ ] **Step 1: Add the wrapper functions at file scope, above `main`**

Insert this block between the `#include` lines and the `toolbox_init` function:

```c
#include "src/mac_syscalls.h"
#include <MacMemory.h>
#include <Files.h>
#include <StandardFile.h>
#include <Gestalt.h>

/* Real Toolbox wrappers used by the production MacSyscalls vtable.
 *
 * Each wrapper is a thin static forwarder: the function pointer in
 * MacSyscalls has a project-owned signature (no Toolbox types in the
 * header), so the wrapper does the cast back to FSSpecPtr / Handle /
 * etc. The actual Toolbox call sits at the bottom of each wrapper.
 *
 * This is the ONLY place in the codebase that calls these Toolbox
 * routines directly. Everywhere else dispatches through the vtable. */

static unsigned long real_tick_count(void) {
    return TickCount();
}

static void* real_new_handle(long size) {
    return (void*)NewHandle(size);
}

static void real_dispose_handle(void* h) {
    if (h) DisposeHandle((Handle)h);
}

static void real_hlock(void* h) {
    if (h) HLock((Handle)h);
}

static void real_hunlock(void* h) {
    if (h) HUnlock((Handle)h);
}

static int real_set_handle_size(void* h, long size) {
    if (!h) return -1;
    SetHandleSize((Handle)h, size);
    return MemError() == noErr ? 0 : -1;
}

static short real_mem_error(void) {
    return MemError();
}

static short real_fsp_open_df(const void* spec, char perm, short* out_ref) {
    return FSpOpenDF((const FSSpec*)spec, perm, out_ref);
}

static short real_fs_close(short ref) {
    return FSClose(ref);
}

static short real_fs_read(short ref, long* io_count, void* buf) {
    return FSRead(ref, io_count, buf);
}

static short real_fs_write(short ref, long* io_count, const void* buf) {
    return FSWrite(ref, io_count, buf);
}

static short real_get_eof(short ref, long* out_size) {
    return GetEOF(ref, out_size);
}

static short real_set_eof(short ref, long size) {
    return SetEOF(ref, size);
}

static short real_set_fpos(short ref, short mode, long off) {
    return SetFPos(ref, mode, off);
}

static short real_fsp_create(const void* spec, unsigned long creator,
                             unsigned long type, short script) {
    return FSpCreate((const FSSpec*)spec, creator, type, script);
}

static void real_standard_get_file(void* out_spec, int* out_good) {
    StandardFileReply reply;
    StandardGetFile(0L, -1, 0L, &reply);
    if (out_spec && reply.sfGood) *(FSSpec*)out_spec = reply.sfFile;
    if (out_good) *out_good = reply.sfGood ? 1 : 0;
}

static void real_standard_put_file(const char* prompt, const char* defname,
                                   void* out_spec, int* out_good) {
    StandardFileReply reply;
    StandardPutFile((ConstStr255Param)prompt, (ConstStr255Param)defname, &reply);
    if (out_spec && reply.sfGood) *(FSSpec*)out_spec = reply.sfFile;
    if (out_good) *out_good = reply.sfGood ? 1 : 0;
}

static short real_gestalt(unsigned long selector, long* out_response) {
    return Gestalt((OSType)selector, out_response);
}

static short real_note_alert(short id, void* filter) {
    return NoteAlert(id, (ModalFilterUPP)filter);
}

static short real_stop_alert(short id, void* filter) {
    return StopAlert(id, (ModalFilterUPP)filter);
}

static const MacSyscalls g_real_syscalls = {
    real_tick_count,
    real_new_handle, real_dispose_handle, real_hlock, real_hunlock,
    real_set_handle_size, real_mem_error,
    real_fsp_open_df, real_fs_close, real_fs_read, real_fs_write,
    real_get_eof, real_set_eof, real_set_fpos, real_fsp_create,
    real_standard_get_file, real_standard_put_file,
    real_gestalt, real_note_alert, real_stop_alert
};
```

- [ ] **Step 2: Cross-build to verify the wrappers compile and the struct initializes**

```bash
docker run --rm -v "$PWD":/work -w /work ghcr.io/autc04/retro68:latest \
  bash -c "cd build && make ArmadilloEditor 2>&1 | tail -20"
```

Expected: clean compile. The `g_real_syscalls` struct is unreferenced for now (linker may warn — `-ffunction-sections + --gc-sections` strips the unused wrappers from the final binary). That's fine; the next step uses it.

- [ ] **Step 3: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add src/app.c
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "feat(app): add production MacSyscalls struct + Toolbox wrappers

Twenty thin static wrappers around the Toolbox routines that the
test seam covers (TickCount, NewHandle/HLock/SetHandleSize, FSp*,
StandardGetFile/StandardPutFile, Gestalt, NoteAlert/StopAlert).
g_real_syscalls is the file-scope static struct that gets passed to
every module needing OS access. This is the ONLY place in the code
that calls these Toolbox routines directly."
```

---

### Task 9: Main `WaitNextEvent` loop in `src/app.c`

**Files:**
- Modify: `src/app.c` (rewrite `main`)

- [ ] **Step 1: Add quit-flag state and replace `main`'s body with the event loop**

```c
/* Application-wide quit flag. Set when the user picks File→Quit; the
 * event loop breaks out and main() returns. */
static int g_quit_requested = 0;

static void event_loop(void) {
    EventRecord ev;
    const long sleep_ticks = 60;            /* 1 s — Plan 2b drops to 15 */
    while (!g_quit_requested) {
        if (WaitNextEvent(everyEvent, &ev, sleep_ticks, 0L)) {
            /* Step 10 wires the dispatch table here. */
        }
    }
}

int main(void) {
    toolbox_init();
    event_loop();
    return 0;
}
```

- [ ] **Step 2: Cross-build**

```bash
docker run --rm -v "$PWD":/work -w /work ghcr.io/autc04/retro68:latest \
  bash -c "cd build && make ArmadilloEditor 2>&1 | tail -10"
```

Expected: clean compile. Built `.APPL` will boot and sit in an event loop forever (no quit path yet — Step 10 wires that).

- [ ] **Step 3: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add src/app.c
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "feat(app): add main WaitNextEvent loop with quit flag

Standard cooperative-multitasking event loop with 1-second sleep ticks
(reduced to 250 ms in Plan 2b once debounce_poll runs on idle ticks).
No event dispatch yet; the loop currently just churns. Quit flag is
checked between WaitNextEvent calls so any future menu handler can
exit cleanly."
```

---

### Task 10: Menu install + File→Quit in `src/menus.c`

**Files:**
- Modify: `src/menus.c`
- Modify: `src/app.c` (call `menus_install`, dispatch `MenuSelect`)

- [ ] **Step 1: Implement `menus_install` and `menus_handle_command` in `src/menus.c`**

Replace the current `src/menus.c` with:

```c
/*
 * src/menus.c — menu-bar setup and command dispatch.
 *
 * Plan 2a wires File→Quit and File→Close. Apple→About lands in
 * Step 12; File→New in Step 13. All other items return
 * kMenuActionNone for now; Plan 2b fills them in.
 */
#include "menus.h"
#include "src/win_editor.h"
#include <Menus.h>
#include <Resources.h>

#define kMenuApple   128
#define kMenuFile    129
#define kMenuEdit    130
#define kMenuView    131

#define kFileNew      1
#define kFileOpen     2
#define kFileClose    4
#define kFileSave     5
#define kFileSaveAs   6
#define kFileQuit     8

void menus_install(void) {
    Handle mbar = GetNewMBar(128);
    if (!mbar) return;          /* missing MBAR resource — caller alerts */
    SetMenuBar(mbar);
    AppendResMenu(GetMenuHandle(kMenuApple), 'DRVR');  /* DA list */
    DrawMenuBar();
}

MenuAction menus_handle_command(long menu_select, WinEditor* win,
                                const MacSyscalls* sys) {
    short menu = HiWord(menu_select);
    short item = LoWord(menu_select);
    MenuAction action = kMenuActionNone;

    (void)win; (void)sys;       /* Step 12+ uses these */

    switch (menu) {
    case kMenuFile:
        switch (item) {
        case kFileQuit:  action = kMenuActionQuit;  break;
        case kFileClose: action = kMenuActionClose; break;
        default: break;         /* Open/Save/SaveAs land in Plan 2b */
        }
        break;
    default: break;             /* other menus land in later steps */
    }

    HiliteMenu(0);
    return action;
}
```

- [ ] **Step 2: Wire `menus_install` and `MenuSelect` dispatch in `src/app.c`**

In `src/app.c`, add `#include "src/menus.h"` near the other includes, then:

1. Inside `main()`, call `menus_install()` after `toolbox_init()` and before `event_loop()`.

2. Replace the empty event-handling body inside `event_loop` with the dispatch:

```c
static void event_loop(void) {
    EventRecord ev;
    const long sleep_ticks = 60;
    while (!g_quit_requested) {
        if (!WaitNextEvent(everyEvent, &ev, sleep_ticks, 0L)) continue;

        switch (ev.what) {
        case mouseDown: {
            WindowPtr wp;
            short part = FindWindow(ev.where, &wp);
            if (part == inMenuBar) {
                long sel = MenuSelect(ev.where);
                if (sel) {
                    MenuAction act = menus_handle_command(sel, 0,
                                                          &g_real_syscalls);
                    if (act == kMenuActionQuit) g_quit_requested = 1;
                }
            }
            /* Step 15 adds in-content / in-go-away dispatch. */
            break;
        }
        default: break;         /* Step 11 adds keyDown/⌘ handling */
        }
    }
}
```

- [ ] **Step 3: Cross-build**

```bash
docker run --rm -v "$PWD":/work -w /work ghcr.io/autc04/retro68:latest \
  bash -c "cd build && make ArmadilloEditor 2>&1 | tail -10"
```

Expected: clean compile. `.APPL` boots, shows the menu bar, and `File → Quit` exits cleanly.

- [ ] **Step 4: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add src/menus.c src/app.c
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "feat(menus): install MBAR 128 and dispatch File→Quit/Close

menus_install loads MBAR 128 from the resource fork, appends the DA
list to the Apple menu via AppendResMenu, and calls DrawMenuBar.
menus_handle_command returns MenuAction values for Quit and Close;
other items land in subsequent tasks. app.c's event loop dispatches
menu-bar mousedowns through MenuSelect → menus_handle_command and
exits when Quit is signalled."
```

---

### Task 11: ⌘-shortcut routing via `MenuKey` in `src/app.c`

**Files:**
- Modify: `src/app.c` (extend `event_loop`'s `switch`)

- [ ] **Step 1: Add the `keyDown` / `autoKey` case**

Inside the `switch (ev.what)` block, add:

```c
case keyDown:
case autoKey: {
    char ch = ev.message & charCodeMask;
    if (ev.modifiers & cmdKey) {
        long sel = MenuKey(ch);
        if (sel) {
            MenuAction act = menus_handle_command(sel, 0,
                                                  &g_real_syscalls);
            if (act == kMenuActionQuit) g_quit_requested = 1;
        }
    }
    /* Non-⌘ keys go to the front window's editor in Step 15. */
    break;
}
```

- [ ] **Step 2: Cross-build**

```bash
docker run --rm -v "$PWD":/work -w /work ghcr.io/autc04/retro68:latest \
  bash -c "cd build && make ArmadilloEditor 2>&1 | tail -10"
```

- [ ] **Step 3: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add src/app.c
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "feat(app): route ⌘-key shortcuts through MenuKey

Standard Mac ⌘-shortcut dispatch: every keyDown/autoKey with the
cmdKey modifier goes through MenuKey → menus_handle_command. Non-⌘
keys are dropped here; Step 15 forwards them to the front window's
editor pane. ⌘Q now quits."
```

---

### Task 12: About box (Apple → About Armadillo)

**Files:**
- Modify: `src/menus.c` (add About item handling)

- [ ] **Step 1: Extend `menus_handle_command` to show the About alert**

Update `src/menus.c`'s switch to add the Apple-menu case:

```c
    switch (menu) {
    case kMenuApple:
        if (item == 1) {
            (void)Alert(256, 0L);   /* About box */
        }
        /* item 2 is the separator; items 3+ are DAs (handled by OS). */
        break;
    case kMenuFile:
        switch (item) {
        case kFileQuit:  action = kMenuActionQuit;  break;
        case kFileClose: action = kMenuActionClose; break;
        default: break;
        }
        break;
    default: break;
    }
```

- [ ] **Step 2: Cross-build**

```bash
docker run --rm -v "$PWD":/work -w /work ghcr.io/autc04/retro68:latest \
  bash -c "cd build && make ArmadilloEditor 2>&1 | tail -10"
```

- [ ] **Step 3: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add src/menus.c
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "feat(menus): show About alert on Apple → About Armadillo

Apple menu item 1 invokes Alert(256, NULL), displaying the ALRT/DITL
256 pair from armadillo.r. Items 2+ are the separator and the desk-
accessory list (the OS handles DA invocation directly)."
```

---

### Task 13: Open `WIND` 128 + create `SrcPane` in `src/win_editor.c`

**Files:**
- Modify: `src/win_editor.c` (fill in `win_editor_new` + `win_editor_free`)
- Modify: `src/menus.c` (handle File → New)
- Modify: `src/app.c` (track and pass the front window)

- [ ] **Step 1: Implement `win_editor_new` and update `win_editor_free` in `src/win_editor.c`**

Replace the current `src/win_editor.c` with:

```c
/*
 * src/win_editor.c — editor window controller.
 *
 * Plan 2a wires:
 *   - win_editor_new opens WIND 128, builds an empty Doc, and creates
 *     the SrcPane sized to the window's content area.
 *   - win_editor_handle_event routes mouseDown / keyDown / updateEvt /
 *     activateEvt to the SrcPane (Step 14 fills the pane handlers).
 *   - win_editor_close disposes the window, the SrcPane, and the Doc.
 */
#include "win_editor.h"
#include "src_pane/src_pane.h"
#include "src/doc.h"
#include <Windows.h>
#include <Events.h>
#include <stdlib.h>

struct WinEditor {
    void*       window_ref;     /* WindowPtr, type-erased */
    Doc*        doc;
    SrcPane*    src_pane;
    MacSyscalls sys;
};

WinEditor* win_editor_new(const MacSyscalls* sys) {
    WinEditor*    w;
    WindowPtr     wp;
    SrcPaneParams params;
    Rect          content;

    if (!sys) return 0;

    w = (WinEditor*)calloc(1, sizeof(WinEditor));
    if (!w) return 0;
    w->sys = *sys;

    wp = GetNewWindow(128, 0L, (WindowPtr)-1L);
    if (!wp) { free(w); return 0; }
    w->window_ref = (void*)wp;
    SetPort(wp);
    ShowWindow(wp);

    w->doc = doc_new();
    if (!w->doc) { DisposeWindow(wp); free(w); return 0; }

    /* Source pane fills the entire content area for now (no toolbar yet). */
    content = wp->portRect;
    params.left      = content.left;
    params.top       = content.top;
    params.right     = content.right;
    params.bottom    = content.bottom;
    params.window_ref = (void*)wp;
    w->src_pane = src_pane_new(&params, sys);
    if (!w->src_pane) {
        doc_free(w->doc);
        DisposeWindow(wp);
        free(w);
        return 0;
    }

    return w;
}

void win_editor_free(WinEditor* w) {
    if (!w) return;
    if (w->src_pane) src_pane_free(w->src_pane);
    if (w->doc)      doc_free(w->doc);
    if (w->window_ref) DisposeWindow((WindowPtr)w->window_ref);
    free(w);
}

void* win_editor_window_ref(const WinEditor* w) {
    return w ? w->window_ref : 0;
}

void win_editor_handle_event(WinEditor* w, const void* event_record) {
    const EventRecord* ev = (const EventRecord*)event_record;
    if (!w || !ev) return;

    switch (ev->what) {
    case updateEvt:
        BeginUpdate((WindowPtr)w->window_ref);
        src_pane_on_update(w->src_pane);
        EndUpdate((WindowPtr)w->window_ref);
        break;
    case activateEvt:
        src_pane_on_activate(w->src_pane,
                             (ev->modifiers & activeFlag) != 0);
        break;
    case mouseDown:
        /* Step 15 dispatches in-content vs. in-go-away here. */
        break;
    case keyDown:
    case autoKey: {
        char ch = ev->message & charCodeMask;
        src_pane_on_key(w->src_pane, ch,
                        (ev->message & keyCodeMask) >> 8,
                        ev->modifiers);
        break;
    }
    default: break;
    }
}

int win_editor_close(WinEditor* w) {
    /* Plan 2a: no dirty guard. Plan 2b adds StopAlert(257). */
    win_editor_free(w);
    return 1;
}
```

- [ ] **Step 2: Wire File → New in `src/menus.c`**

Update `src/menus.c`'s File-menu switch to call back into the app for window creation. Since `menus.c` doesn't itself open windows, expose a callback the app installs at startup.

Add to `src/menus.h` after the `MenuAction` enum:

```c
/* Callback the app installs so menus.c can request a new editor
 * window without coupling menus to win_editor. */
typedef WinEditor* (*MenusNewWindowCb)(const MacSyscalls* sys);
void menus_set_new_window_cb(MenusNewWindowCb cb);
```

In `src/menus.c`, add a file-static:

```c
static MenusNewWindowCb g_new_window_cb = 0;

void menus_set_new_window_cb(MenusNewWindowCb cb) {
    g_new_window_cb = cb;
}
```

And update the File-menu switch:

```c
        case kFileNew:
            if (g_new_window_cb) (void)g_new_window_cb(sys);
            break;
        case kFileClose: action = kMenuActionClose; break;
        case kFileQuit:  action = kMenuActionQuit;  break;
```

(Order doesn't matter; new is item 1, close is item 4, quit is item 8.)

- [ ] **Step 3: Wire the callback in `src/app.c`**

Add `#include "src/win_editor.h"` and a thin wrapper:

```c
static WinEditor* g_front_window = 0;

static WinEditor* app_new_editor(const MacSyscalls* sys) {
    if (g_front_window) {
        /* Plan 2a is single-document; close the old before opening. */
        win_editor_free(g_front_window);
    }
    g_front_window = win_editor_new(sys);
    return g_front_window;
}
```

In `main`, install the callback after `menus_install`:

```c
    menus_install();
    menus_set_new_window_cb(app_new_editor);
```

- [ ] **Step 4: Cross-build**

```bash
docker run --rm -v "$PWD":/work -w /work ghcr.io/autc04/retro68:latest \
  bash -c "cd build && make ArmadilloEditor 2>&1 | tail -10"
```

Expected: clean compile. `.APPL` boots, shows menu bar; ⌘N (or File → New) opens the editor window.

- [ ] **Step 5: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add src/win_editor.c src/menus.h src/menus.c src/app.c
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "feat(win_editor): open WIND 128 with empty Doc + SrcPane

win_editor_new opens the editor window from WIND 128, creates an
empty Doc, and constructs a SrcPane sized to the window content area.
win_editor_handle_event dispatches updateEvt (BeginUpdate/EndUpdate
+ src_pane_on_update), activateEvt, and forwards keyDown to the
pane. mouseDown in-content/in-go-away dispatch is added in Step 15.

menus.c gains a MenusNewWindowCb callback so File → New can request
a new window without coupling the menus module to win_editor's
implementation. app.c installs the callback at startup."
```

---

### Task 14: TextEdit-backed `SrcPane` in `src_pane/src_pane.c`

**Files:**
- Modify: `src_pane/src_pane.c`

- [ ] **Step 1: Replace the skeleton with the TextEdit-backed implementation**

```c
/*
 * src_pane/src_pane.c — TextEdit-backed source pane.
 *
 * Plan 2a wires TENew + the basic event handlers (TEKey, TEClick,
 * TEActivate, TEDeactivate, TEUpdate). Style-run application is
 * Plan 2b (TESetStyle drives it once the scanner produces runs).
 */
#include "src_pane.h"
#include <TextEdit.h>
#include <Quickdraw.h>
#include <stdlib.h>
#include <string.h>

struct SrcPane {
    TEHandle    te;
    Rect        view_rect;
    void*       window_ref;
    MacSyscalls sys;
};

SrcPane* src_pane_new(const SrcPaneParams* params, const MacSyscalls* sys) {
    SrcPane* p;
    Rect     view, dest;
    GrafPtr  saved;

    if (!params || !sys) return 0;
    p = (SrcPane*)calloc(1, sizeof(SrcPane));
    if (!p) return 0;
    p->sys        = *sys;
    p->window_ref = params->window_ref;

    SetRect(&view,
            params->left, params->top,
            params->right, params->bottom);
    p->view_rect = view;
    dest = view;
    InsetRect(&dest, 4, 4);     /* small interior margin */

    GetPort(&saved);
    SetPort((WindowPtr)params->window_ref);
    p->te = TENew(&dest, &view);
    SetPort(saved);
    if (!p->te) { free(p); return 0; }

    return p;
}

void src_pane_free(SrcPane* p) {
    if (!p) return;
    if (p->te) TEDispose(p->te);
    free(p);
}

const char* src_pane_get_text(const SrcPane* p, unsigned short* out_len) {
    if (!p || !p->te) {
        if (out_len) *out_len = 0;
        return "";
    }
    {
        Handle  h   = (**p->te).hText;
        long    len = (**p->te).teLength;
        if (out_len) *out_len = (unsigned short)(len > 0xFFFF ? 0xFFFF : len);
        return h ? *(const char**)h : "";
    }
}

void src_pane_set_text(SrcPane* p, const char* bytes, unsigned short len) {
    if (!p || !p->te) return;
    TESetText((const Ptr)bytes, len, p->te);
    TECalText(p->te);
    InvalRect(&p->view_rect);
}

void src_pane_apply_runs(SrcPane* p, const StyleRun* runs, size_t count) {
    /* Plan 2b walks runs[] and calls TESetStyle. */
    (void)p; (void)runs; (void)count;
}

void src_pane_get_selection(const SrcPane* p,
                            unsigned short* out_start,
                            unsigned short* out_end) {
    if (!p || !p->te) {
        if (out_start) *out_start = 0;
        if (out_end)   *out_end   = 0;
        return;
    }
    if (out_start) *out_start = (unsigned short)(**p->te).selStart;
    if (out_end)   *out_end   = (unsigned short)(**p->te).selEnd;
}

void src_pane_set_selection(SrcPane* p,
                            unsigned short start,
                            unsigned short end) {
    if (!p || !p->te) return;
    TESetSelect(start, end, p->te);
}

void src_pane_on_mouse_down(SrcPane* p, short h, short v, short modifiers) {
    Point pt;
    if (!p || !p->te) return;
    pt.h = h; pt.v = v;
    TEClick(pt, (modifiers & shiftKey) != 0, p->te);
}

void src_pane_on_key(SrcPane* p, short char_code, short key_code,
                     short modifiers) {
    if (!p || !p->te) return;
    (void)key_code; (void)modifiers;
    TEKey((char)char_code, p->te);
}

void src_pane_on_activate(SrcPane* p, int is_active) {
    if (!p || !p->te) return;
    if (is_active) TEActivate(p->te); else TEDeactivate(p->te);
}

void src_pane_on_update(SrcPane* p) {
    if (!p || !p->te) return;
    EraseRect(&p->view_rect);
    TEUpdate(&p->view_rect, p->te);
}

void src_pane_on_idle(SrcPane* p) {
    if (!p || !p->te) return;
    TEIdle(p->te);
}

void src_pane_set_edit_callback(SrcPane* p, SrcPaneEditCallback cb,
                                void* ctx) {
    /* Plan 2b stores cb/ctx and fires after TEKey produces a doc
     * mutation, so debounce_on_edit can observe the keystroke. */
    (void)p; (void)cb; (void)ctx;
}
```

- [ ] **Step 2: Cross-build**

```bash
docker run --rm -v "$PWD":/work -w /work ghcr.io/autc04/retro68:latest \
  bash -c "cd build && make ArmadilloEditor 2>&1 | tail -10"
```

Expected: clean compile. `.APPL` opens an editor window via File → New, you can type into it (TextEdit handles keyboard input + drawing).

- [ ] **Step 3: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add src_pane/src_pane.c
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "feat(src_pane): TextEdit-backed implementation of SrcPane

TENew sized to the window's content area with a 4-px inset; standard
TEKey/TEClick/TEActivate/TEDeactivate/TEUpdate/TEIdle dispatch from
the on_* entry points. Selection getters/setters delegate to the TE
record. Style-run application and the edit callback remain stubs for
Plan 2b.

GetPort/SetPort guards around TENew so the call doesn't depend on
who set the port last."
```

---

### Task 15: `mouseDown` in-content / in-go-away dispatch

**Files:**
- Modify: `src/app.c` (extend the `mouseDown` case)
- Modify: `src/win_editor.c` (extend `win_editor_handle_event`'s `mouseDown` case)

- [ ] **Step 1: Extend `app.c`'s `mouseDown` case**

Replace the entire `mouseDown` case in `event_loop` with:

```c
        case mouseDown: {
            WindowPtr wp;
            short part = FindWindow(ev.where, &wp);
            switch (part) {
            case inMenuBar: {
                long sel = MenuSelect(ev.where);
                if (sel) {
                    MenuAction act = menus_handle_command(sel, g_front_window,
                                                          &g_real_syscalls);
                    if (act == kMenuActionQuit)  g_quit_requested = 1;
                    if (act == kMenuActionClose && g_front_window) {
                        win_editor_close(g_front_window);
                        g_front_window = 0;
                    }
                }
                break;
            }
            case inSysWindow:
                SystemClick(&ev, wp);   /* desk accessories */
                break;
            case inContent:
                if (g_front_window
                    && wp == (WindowPtr)win_editor_window_ref(g_front_window)) {
                    win_editor_handle_event(g_front_window, &ev);
                }
                break;
            case inGoAway:
                if (g_front_window
                    && wp == (WindowPtr)win_editor_window_ref(g_front_window)
                    && TrackGoAway(wp, ev.where)) {
                    win_editor_close(g_front_window);
                    g_front_window = 0;
                }
                break;
            case inDrag: {
                Rect bounds = (*GetGrayRgn())->rgnBBox;
                DragWindow(wp, ev.where, &bounds);
                break;
            }
            default: break;
            }
            break;
        }
```

- [ ] **Step 2: Forward content `mouseDown` to the pane in `win_editor_handle_event`**

Replace the `mouseDown` case in `src/win_editor.c`'s `win_editor_handle_event` with:

```c
    case mouseDown: {
        Point pt = ev->where;
        GlobalToLocal(&pt);
        src_pane_on_mouse_down(w->src_pane, pt.h, pt.v, ev->modifiers);
        break;
    }
```

- [ ] **Step 3: Forward `keyDown` (non-⌘) from the loop to the front window**

In `src/app.c`'s `keyDown`/`autoKey` case, after the `if (modifiers & cmdKey)` block, add the else branch:

```c
        } else if (g_front_window) {
            win_editor_handle_event(g_front_window, &ev);
        }
```

- [ ] **Step 4: Forward `updateEvt` and `activateEvt` for the editor window**

Add two more cases to the `event_loop` switch:

```c
        case updateEvt:
        case activateEvt:
            if (g_front_window
                && (WindowPtr)ev.message
                       == (WindowPtr)win_editor_window_ref(g_front_window)) {
                win_editor_handle_event(g_front_window, &ev);
            }
            break;
```

- [ ] **Step 5: Cross-build**

```bash
docker run --rm -v "$PWD":/work -w /work ghcr.io/autc04/retro68:latest \
  bash -c "cd build && make ArmadilloEditor 2>&1 | tail -10"
```

Expected: clean compile. `.APPL` now responds to clicks and keystrokes inside the editor window.

- [ ] **Step 6: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add src/app.c src/win_editor.c
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "feat(app): full mouseDown/keyDown/update/activate dispatch

mouseDown in the content area forwards through win_editor_handle_event
to src_pane_on_mouse_down. mouseDown in the go-away box runs
TrackGoAway then closes the window via win_editor_close. inDrag
runs DragWindow so the user can move the editor. inSysWindow runs
SystemClick (desk accessories). Non-⌘ keyDown/autoKey routes to
the front window's editor pane. updateEvt/activateEvt route the same
way when the event is for the editor window."
```

---

## Group D — QA + release

### Task 16: Cross-compile and verify the `.APPL` artifact

**Files:**
- Run-only: cross-compile via Docker

- [ ] **Step 1: Clean build**

```bash
docker run --rm -v "$PWD":/work -w /work ghcr.io/autc04/retro68:latest \
  bash -c "rm -rf build && mkdir build && cd build && \
    cmake .. -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake && \
    make ArmadilloEditor 2>&1 | tail -25"
```

Expected: clean configure + clean make; final lines list `ArmadilloEditor.APPL` and `ArmadilloEditor.dsk` produced.

- [ ] **Step 2: Verify artifact sizes are non-trivial**

```bash
ls -la build/ArmadilloEditor.APPL build/ArmadilloEditor.dsk
```

Expected: `.APPL` is at least 30 KB (Toolbox glue + our code is ~25–60 KB). `.dsk` is at least 800 KB (standard 1.4 MB floppy image is the minimum mountable size).

- [ ] **Step 3: Push the branch so CI builds run remotely**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git push origin HEAD
```

Wait for the GitHub Actions `release` job to finish (it cross-builds the `.APPL` + `.dsk` and uploads them as a workflow artifact). Verify:

```bash
env -u GITHUB_TOKEN -u GH_TOKEN gh run list --workflow release --limit 1 \
  --json status,conclusion -q '.[] | "\(.status)/\(.conclusion)"'
```

Expected: `completed/success`.

---

### Task 17: Write `qa-checklist-2a.md`

**Files:**
- Create: `openspec/changes/add-md-editor-mvp/qa-checklist-2a.md`

- [ ] **Step 1: Create the checklist file**

```markdown
# Plan 2a — QA Checklist (`v0.1.0`)

Run through this checklist on a Quadra 800 emulator (QEMU) before
tagging `v0.1.0`. Every item must pass.

**Setup:**
- [ ] Mount the `ArmadilloEditor.dsk` from the latest `release`
      workflow run (or build locally, copy the `.APPL` + `.dsk`
      into the QEMU shared folder, mount inside Mac OS).
- [ ] Cold-boot the Mac OS image so any prior state is gone.

**Boot:**
- [ ] Double-click `ArmadilloEditor.APPL`. The app launches with no
      error alerts.
- [ ] No window appears yet (intentional — empty doc state).
- [ ] The menu bar shows: ` `, File, Edit, View.

**Apple menu:**
- [ ] Apple → "About Armadillo Editor…" displays the About alert
      with the app name, tagline, and `Version 0.1.0 — Bare Shell`.
- [ ] Clicking OK dismisses the alert.
- [ ] Apple → desk accessories list contains the standard DAs
      (Calculator, Note Pad, etc.) and they launch normally.

**File menu:**
- [ ] File → New (⌘N) opens an editor window titled `Untitled.md`
      in the Quadra default size (~360×500 px).
- [ ] The window's content area is empty.
- [ ] Type a few characters — they appear immediately.
- [ ] Backspace deletes characters.
- [ ] Click inside the editor — caret moves to the click position.
- [ ] Click-drag to select — selection highlight follows the drag.
- [ ] Edit → Cut, Copy, Paste are disabled or no-op (no real
      selection-aware menu enable yet — Plan 2b adds it).
- [ ] File → Close (⌘W) closes the window. App stays running.
- [ ] File → New again — a fresh empty editor opens.
- [ ] Click the window's go-away box — window closes.
- [ ] File → Open / Save / Save As are present in the menu but
      do nothing yet (Plan 2b wires these).

**View menu:**
- [ ] View → Source / Read items are present but do nothing yet
      (Plan 2b wires the toggle).

**Window dragging:**
- [ ] Drag the window's title bar — window follows. No graphical
      glitches in the moved area.
- [ ] Activate / deactivate by clicking another window — the
      editor's title bar greys out when inactive, returns to
      focused appearance when reactivated.

**Quit:**
- [ ] File → Quit (⌘Q) exits the app cleanly.
- [ ] After quit, no zombie processes, no leaked windows visible.

**Build artifacts:**
- [ ] CI's most recent `release` run on `main` is green.
- [ ] `ArmadilloEditor.APPL` and `ArmadilloEditor.dsk` are
      attached to the workflow run as artifacts.

If every box is checked, proceed to Task 18 (tag `v0.1.0`).
```

- [ ] **Step 2: Commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add openspec/changes/add-md-editor-mvp/qa-checklist-2a.md
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "docs(qa): add Plan 2a QEMU manual-QA checklist

Pre-tag verification list run through QEMU before pushing v0.1.0.
Covers boot, menu bar, About, File→New/Close/Quit, ⌘-shortcuts,
window dragging, activate/deactivate, and build-artifact sanity.
Plan 2b items (Open/Save/View toggle) are explicitly noted as
\"present but do nothing yet\" so the checklist matches reality."
```

---

### Task 18: Run QEMU manual QA

**Files:**
- Run-only: open the `.APPL` in QEMU, walk through the checklist.

- [ ] **Step 1: Boot the emulator with the latest build**

Use whatever your local QEMU + Mac OS setup looks like (likely the existing one referenced at `~/Documents/Projects/QemuMac/`). Mount the `.dsk` produced in Task 16 (or the artifact from CI's `release` workflow). Cold-boot so cached state from prior versions doesn't interfere.

- [ ] **Step 2: Walk through every item in `qa-checklist-2a.md`**

If any item fails, stop, file it, fix the underlying bug (typically as a small follow-up commit on this branch), re-run the checklist. Don't tag `v0.1.0` with any failing item.

- [ ] **Step 3: When all items pass, append a "QA pass" entry to the change folder**

Append a new dated line to `openspec/changes/add-md-editor-mvp/qa-checklist-2a.md` at the bottom:

```markdown
---

## QA passes

- 2026-MM-DD — `<short-sha>` — all items pass on QEMU Quadra 800 / System 7.5.5
```

- [ ] **Step 4: Commit the QA-pass entry**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git add openspec/changes/add-md-editor-mvp/qa-checklist-2a.md
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git commit -m "qa(2a): record passing run for v0.1.0 candidate"
```

---

### Task 19: Tag `v0.1.0` and verify GitHub Release

**Files:**
- Run-only: tag + push.

- [ ] **Step 1: Tag the QA-passed commit**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git tag -a -s v0.1.0 -m "Armadillo Editor v0.1.0 — Bare Shell

Plan 2a milestone. Boots a Quadra-targeted .APPL, opens an editor
window with a TextEdit-backed source pane, accepts typing, closes
cleanly via the go-away box / File → Close, and quits via ⌘Q. No
file I/O, no view toggle, no parse cycle. Ships as the foundation
for v0.2.0 (Plan 2b) which layers the real features on this shell."
```

- [ ] **Step 2: Push the tag**

```bash
SSH_AUTH_SOCK="$HOME/Library/Group Containers/2BUA8C4S2C.com.1password/t/agent.sock" \
  git push origin v0.1.0
```

- [ ] **Step 3: Wait for the `release` workflow to publish the GitHub Release**

```bash
env -u GITHUB_TOKEN -u GH_TOKEN gh run list --workflow release --limit 3 \
  --json status,conclusion,headBranch,event \
  -q '.[] | "\(.event)  \(.headBranch)  \(.status)/\(.conclusion // "-")"'
```

Expected: a row with `event=push  headBranch=(refs/tags/v0.1.0)  completed/success`.

- [ ] **Step 4: Verify the Release page on GitHub**

```bash
env -u GITHUB_TOKEN -u GH_TOKEN gh release view v0.1.0 \
  --json name,tagName,assets,body \
  -q '"name=\(.name)  tag=\(.tagName)  assets=\(.assets | length)"'
```

Expected: a release named `v0.1.0` exists with at least 2 assets (the `.APPL` and `.dsk`).

- [ ] **Step 5: Confirm asset downloads work**

```bash
mkdir -p /tmp/v0.1.0-verify && cd /tmp/v0.1.0-verify && \
env -u GITHUB_TOKEN -u GH_TOKEN gh release download v0.1.0 \
  --pattern 'ArmadilloEditor.*' && \
ls -la ArmadilloEditor.APPL ArmadilloEditor.dsk
```

Expected: both files download with non-zero size.

Plan 2a complete. Move to `plan-2b-mvp-features.md` for the v0.2.0 milestone (file I/O, view toggle, parse cycle, syntax coloring, smoke test).
