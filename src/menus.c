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
#include <Dialogs.h>

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

static MenusNewWindowCb g_new_window_cb = 0;
static MenusFileCmdCb   g_file_cmd_cb   = 0;

void menus_set_new_window_cb(MenusNewWindowCb cb) {
    g_new_window_cb = cb;
}

void menus_set_file_cmd_cb(MenusFileCmdCb cb) {
    g_file_cmd_cb = cb;
}

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
    case kMenuApple:
        if (item == 1) {
            (void)Alert(256, 0L);   /* About box */
        }
        /* item 2 is the separator; items 3+ are DAs (handled by OS). */
        break;
    case kMenuFile:
        switch (item) {
        case kFileNew:
            if (g_new_window_cb) (void)g_new_window_cb(sys);
            break;
        case kFileOpen:
            if (g_file_cmd_cb) g_file_cmd_cb(kMenusFileOpen, win, sys);
            break;
        case kFileSave:
            if (g_file_cmd_cb) g_file_cmd_cb(kMenusFileSave, win, sys);
            break;
        case kFileSaveAs:
            if (g_file_cmd_cb) g_file_cmd_cb(kMenusFileSaveAs, win, sys);
            break;
        case kFileClose: action = kMenuActionClose; break;
        case kFileQuit:  action = kMenuActionQuit;  break;
        default: break;
        }
        break;
    default: break;             /* other menus land in later steps */
    }

    HiliteMenu(0);
    return action;
}
