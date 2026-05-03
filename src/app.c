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
#include "src/mac_syscalls.h"
#include "src/menus.h"
#include "src/win_editor.h"
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
    /* FSpOpenDF's prototype takes non-const FSSpecPtr; same const-cast
     * pattern as real_fsp_create. */
    return FSpOpenDF((FSSpec*)(void*)spec, perm, out_ref);
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
    /* FSpCreate's prototype takes a non-const FSSpecPtr. The const-cast
     * away here matches every other Mac codebase — the Toolbox routine
     * doesn't actually mutate *spec; the API is just const-incorrect. */
    return FSpCreate((FSSpec*)(void*)spec, creator, type, script);
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

static void toolbox_init(void) {
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(0L);
    FlushEvents(everyEvent, 0);
}

/* Application-wide quit flag. Set when the user picks File→Quit; the
 * event loop breaks out and main() returns. */
static int g_quit_requested = 0;

/* MVP is single-document — one editor window at a time. */
static WinEditor* g_front_window = 0;

static WinEditor* app_new_editor(const MacSyscalls* sys) {
    if (g_front_window) {
        win_editor_free(g_front_window);
    }
    g_front_window = win_editor_new(sys);
    return g_front_window;
}

static void event_loop(void) {
    EventRecord ev;
    const long sleep_ticks = 60;            /* 1 s — Plan 2b drops to 15 */
    while (!g_quit_requested) {
        if (!WaitNextEvent(everyEvent, &ev, sleep_ticks, 0L)) continue;

        switch (ev.what) {
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
                /* LMGetGrayRgn is the multiversal/low-memory accessor for
                 * the desktop region; classic GetGrayRgn() doesn't exist
                 * in this Retro68 header set. */
                Rect bounds = (*LMGetGrayRgn())->rgnBBox;
                DragWindow(wp, ev.where, &bounds);
                break;
            }
            default: break;
            }
            break;
        }
        case keyDown:
        case autoKey: {
            char ch = ev.message & charCodeMask;
            if (ev.modifiers & cmdKey) {
                long sel = MenuKey(ch);
                if (sel) {
                    MenuAction act = menus_handle_command(sel, g_front_window,
                                                          &g_real_syscalls);
                    if (act == kMenuActionQuit)  g_quit_requested = 1;
                    if (act == kMenuActionClose && g_front_window) {
                        win_editor_close(g_front_window);
                        g_front_window = 0;
                    }
                }
            } else if (g_front_window) {
                win_editor_handle_event(g_front_window, &ev);
            }
            break;
        }
        case updateEvt:
        case activateEvt:
            if (g_front_window
                && (WindowPtr)ev.message
                       == (WindowPtr)win_editor_window_ref(g_front_window)) {
                win_editor_handle_event(g_front_window, &ev);
            }
            break;
        default: break;
        }
    }
}

int main(void) {
    toolbox_init();
    menus_install();
    menus_set_new_window_cb(app_new_editor);
    event_loop();
    return 0;
}
