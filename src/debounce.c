/*
 * src/debounce.c — pure state machine driving the parse debounce.
 *
 * See design.md §3 for the pipeline context. Two functions:
 *   - debounce_on_edit: record a keystroke (sets dirty, captures tick).
 *   - debounce_poll: if dirty AND enough ticks have elapsed since the
 *     last keystroke, clear dirty and return 1; otherwise 0.
 *
 * Never calls TickCount directly — always dispatches through the
 * MacSyscalls.tick_count vtable slot so tests can drive time.
 */
#include "debounce.h"

void debounce_on_edit(DebounceState* s, const MacSyscalls* sys) {
    /* sys is per-call; not retained. */
    if (!s || !sys) return;
    s->dirty = 1;
    s->last_keystroke_tick = sys->tick_count();
}

int debounce_poll(DebounceState* s, const MacSyscalls* sys) {
    /* sys is per-call; not retained. */
    unsigned long now;
    if (!s || !sys) return 0;
    if (!s->dirty) return 0;
    now = sys->tick_count();
    if (now - s->last_keystroke_tick < kParseDebounceTicks) return 0;
    s->dirty = 0;
    return 1;
}
