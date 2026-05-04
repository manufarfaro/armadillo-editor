# Delta Spec: app-shell

## ADDED Requirements


### Requirement: Toolbox initialization

On launch, the application SHALL initialize the Mac Toolbox in the standard System 7 order: `InitGraf(&qd.thePort)`, `InitFonts`, `InitWindows`, `InitMenus`, `TEInit`, `InitDialogs(nil)`, `InitCursor`. No Toolbox call SHALL precede `InitGraf`. On failure of any init step, the application SHALL exit gracefully without partial initialization (`ExitToShell` path; no alert because the dialog manager may not be up yet).

#### Scenario: Clean initialization succeeds
- GIVEN a Quadra 800 running System 7.1 with sufficient memory
- WHEN the application is launched
- THEN the Toolbox is initialized in order and the main window appears

#### Scenario: Initialization on insufficient-RAM machine
- GIVEN a machine with insufficient free memory for Toolbox init
- WHEN the application is launched
- THEN it exits to the Finder without crashing

### Requirement: Menu bar

The application SHALL install a menu bar (`'MBAR'` 128) containing Apple, File, Edit, and View menus. Menu items and keyboard shortcuts:

- **Apple** menu: About Armadillo Editor…
- **File** menu: New (⌘N), Open… (⌘O), —, Close (⌘W), Save (⌘S), Save As…, —, Quit (⌘Q)
- **Edit** menu: Undo (⌘Z), —, Cut (⌘X), Copy (⌘C), Paste (⌘V), Clear, Select All (⌘A)
- **View** menu: Source, Read (mutually exclusive; currently active is checked)

The application SHALL call `AppendResMenu(appleMenu, 'DRVR')` to populate the Apple menu with desk accessories per System 7 convention.

#### Scenario: Menu bar draws on launch
- GIVEN the application has completed Toolbox init
- WHEN the main event loop begins
- THEN the menu bar shows "Apple File Edit View" at the top of the screen

#### Scenario: Keyboard shortcut dispatches command
- GIVEN the main event loop is running and the editor window is active
- WHEN the user presses ⌘S
- THEN the Save command is dispatched (equivalent to File → Save)

### Requirement: Main event loop

The application SHALL run a main event loop based on `WaitNextEvent(everyEvent, &event, kEventLoopSleepTicks, nil)`. On each iteration the loop SHALL:

1. Handle the event via `handle_event(&event)` (mouseDown, keyDown, updateEvt, activateEvt, nullEvent, etc.)
2. Call `check_debounce(&editor_state, &clock)` to drive the parse pipeline (see §`app-shell.Debounce poll` below)
3. Continue until the quit flag is set

The sleep parameter (`kEventLoopSleepTicks`) is a compile-time constant, default 6 ticks (~100 ms). It MAY be overridden at build time via `-DkEventLoopSleepTicks=N`.

#### Scenario: Event loop processes events and null events
- GIVEN the app is running with the editor window frontmost
- WHEN the user is idle
- THEN the loop receives null events at ~100 ms cadence and calls `check_debounce` each time

#### Scenario: Quit command exits the loop
- GIVEN the app is running
- WHEN the user selects File → Quit (or presses ⌘Q)
- THEN the quit flag is set, the loop exits, resources are released, and the app returns to the Finder

### Requirement: Debounce poll

The application SHALL poll a debounce check on every event loop iteration. If the source pane's text has been edited since the last parse, and `TickCount() - last_keystroke_tick >= kParseDebounceTicks`, the app SHALL run a parse cycle (see `mdparse`, `scanner`, `render` specs) and clear the dirty flag. The debounce logic SHALL access the current tick through `MacSyscalls.tick_count`, never calling `TickCount` directly — this enables host-side testing with `FakeClock`.

`kParseDebounceTicks` is a compile-time constant, default 15 ticks (~250 ms). It MAY be overridden via `-DkParseDebounceTicks=N`.

#### Scenario: Edit followed by idle triggers parse
- GIVEN the user types a character in the source pane at tick T
- WHEN the event loop iterates at tick T+20 (greater than kParseDebounceTicks=15)
- THEN a parse cycle runs and the render model is refreshed

#### Scenario: Rapid typing defers parse until idle
- GIVEN the user types every 5 ticks for 30 ticks
- WHEN the event loop iterates during that burst
- THEN no parse runs until the user pauses for kParseDebounceTicks consecutive ticks

### Requirement: Error alerts

The application SHALL surface errors to the user via Mac OS alerts (`NoteAlert` or `StopAlert`) driven by numeric error codes from per-module enums, mapped to `'ALRT'` resource IDs and `'STR#'` string indices per the error table in the design doc §5.2. Library modules (`render/`, `mdparse/`, `scanner/`, `src_pane/`) SHALL NOT call `DebugStr`, `printf`, or alert functions directly; the app layer owns all user-facing output.

#### Scenario: Parse OOM surfaces an alert
- GIVEN a parse cycle fails with `kMdParseErrArenaOOM`
- WHEN `run_parse_cycle` returns
- THEN `NoteAlert(258, nil)` is called, the user sees "Not enough memory to parse this document.", and the application remains usable

### Requirement: Graceful quit

On quit, the application SHALL:

1. Check if the current document is dirty; if so, prompt via a standard Mac "Save changes?" dialog with Save / Don't Save / Cancel buttons
2. On Save or Don't Save, release the doc, render model, arena, source pane, and editor window in that order
3. On Cancel, abort the quit and return to the main loop
4. `ExitToShell()` when cleanup is complete

#### Scenario: Clean quit with no unsaved changes
- GIVEN the current document is clean
- WHEN the user selects File → Quit
- THEN all resources are released and the app exits to the Finder

#### Scenario: Quit with dirty document, user saves
- GIVEN the current document has unsaved changes
- WHEN the user selects File → Quit and clicks Save
- THEN the document is saved, all resources are released, and the app exits

#### Scenario: Quit with dirty document, user cancels
- GIVEN the current document has unsaved changes
- WHEN the user selects File → Quit and clicks Cancel
- THEN no save occurs, no resources are released, and the app returns to the main loop

### Requirement: `MacSyscalls` production wiring

The app shell SHALL define the production `MacSyscalls` struct populated with function pointers that call the real Toolbox. These wrappers MAY be trivial forwarders (`static short app_new_handle(Size s) { return NewHandle(s); }`) or MAY include minor adaptation (e.g., casting, A5 context setup). Every other module that touches the OS SHALL accept a `const MacSyscalls*` parameter, never calling the Toolbox directly.

#### Scenario: Real syscalls forward to Toolbox
- GIVEN the production `MacSyscalls` struct is constructed
- WHEN any module calls `syscalls->tick_count()`
- THEN the return value equals `TickCount()` at that moment
