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
