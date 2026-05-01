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
