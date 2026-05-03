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
- [ ] Edit → Cut, Copy, Paste are present in the menu but their
      behavior is not yet wired through win_editor in 2a; verify
      they don't crash if invoked (Plan 2b wires them).
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
- [ ] CI's most recent `release` run on `main` (post-merge) is green.
- [ ] `ArmadilloEditor.APPL` and `ArmadilloEditor.dsk` are
      attached to the workflow run as artifacts.

If every box is checked, proceed to tag `v0.1.0`.

---

## QA passes

<!-- Append `- YYYY-MM-DD — <short-sha> — all items pass on QEMU Quadra 800 / System 7.5.5` per successful run. -->
