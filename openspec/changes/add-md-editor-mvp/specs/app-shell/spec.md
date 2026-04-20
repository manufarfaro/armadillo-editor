# Delta Spec: app-shell

## ADDED Requirements

All requirements in `openspec/specs/app-shell/spec.md` are new additions in this change:

- **Toolbox initialization** — standard System 7 init order (InitGraf, InitFonts, InitWindows, InitMenus, TEInit, InitDialogs, InitCursor); graceful exit on failure.
- **Menu bar** — Apple / File / Edit / View menus with keyboard shortcuts (⌘N, ⌘O, ⌘W, ⌘S, ⌘Q, standard Edit shortcuts, Source/Read in View).
- **Main event loop** — `WaitNextEvent` with 6-tick null sleep, per-iteration debounce poll, quit flag.
- **Debounce poll** — polled from event loop; fires parse cycle 15 ticks after last keystroke; uses `MacSyscalls.tick_count` for testability.
- **Error alerts** — per-module error codes mapped to `'ALRT'` IDs 256..265 and `'STR#'` 128 entries 1..10; library modules do not call alert functions directly.
- **Graceful quit** — dirty-check with Save/Don't Save/Cancel dialog; resource release in reverse construction order; `ExitToShell` on clean exit.
- **`MacSyscalls` production wiring** — real function-pointer struct defined in `src/app.c` with trivial Toolbox forwarders.
