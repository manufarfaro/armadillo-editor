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

/* Callback the app installs so menus.c can request a new editor
 * window without coupling menus to win_editor. */
typedef WinEditor* (*MenusNewWindowCb)(const MacSyscalls* sys);
void menus_set_new_window_cb(MenusNewWindowCb cb);

/* File-operation commands that need the front window's state.
 * Values are arbitrary — internal to the menus ↔ app contract. */
typedef enum {
    kMenusFileOpen   = 0,
    kMenusFileSave   = 1,
    kMenusFileSaveAs = 2
} MenusFileCmd;

/* Callback for File → Open / Save / Save As. menus.c hands the
 * current front window (may be NULL for Open) and the syscalls
 * vtable; app.c does the file-io work. */
typedef void (*MenusFileCmdCb)(MenusFileCmd cmd, WinEditor* win,
                               const MacSyscalls* sys);
void menus_set_file_cmd_cb(MenusFileCmdCb cb);

/* Dispatch a packed (menu, item) selector returned from MenuSelect or
 * MenuKey. `win` may be NULL (e.g., About item with no window open).
 * Returns kMenuActionQuit when the user picked File→Quit so the event
 * loop can exit; kMenuActionClose for File→Close (caller decides
 * whether the close is allowed). */
MenuAction menus_handle_command(long menu_select, WinEditor* win,
                                const MacSyscalls* sys);

#endif /* ARMA_MENUS_H */
