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
        "About Armadillo Editor\xC9",
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
        "Open\xC9",     noicon, "O", nomark, plain;
        "-",            noicon, nokey, nomark, plain;
        "Close",        noicon, "W", nomark, plain;
        "Save",         noicon, "S", nomark, plain;
        "Save As\xC9",  noicon, nokey, nomark, plain;
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
    "Untitled.md",
    noAutoCenter                /* required by Retro68 multiversal Types.r */
};

/* ---------- About + unsaved-changes alerts ---------- */

resource 'DLOG' (256, "About", purgeable) {
    {80, 100, 220, 412},
    dBoxProc,
    invisible,
    noGoAway,
    0x0,
    256,                        /* shared DITL with the alert */
    "About Armadillo Editor",
    noAutoCenter                /* required by Retro68 multiversal Types.r */
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
        { 72,  16,  92, 296}, StaticText   { disabled, "Version 0.1.0 \xD1 Bare Shell" }
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
    "0.1.0 \xD1 Bare Shell"
};
