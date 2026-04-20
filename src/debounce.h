/*
 * src/debounce.h — pure state machine driving the parse debounce.
 *
 * Tracks "dirty since last parse" and "tick of last keystroke." Polled
 * from the event loop; fires a parse cycle when kParseDebounceTicks
 * elapse after the last keystroke.
 *
 * Uses MacSyscalls.tick_count for the clock so tests can inject a
 * FakeClock. Never calls TickCount directly.
 */
#ifndef ARMA_DEBOUNCE_H
#define ARMA_DEBOUNCE_H

#include "mac_syscalls.h"

#ifndef kParseDebounceTicks
#define kParseDebounceTicks 15    /* 60 ticks/sec * 0.25 s */
#endif

typedef struct DebounceState {
    int           dirty;
    unsigned long last_keystroke_tick;
} DebounceState;

/* Record a keystroke. Sets dirty=1, captures current tick. */
void debounce_on_edit(DebounceState* s, const MacSyscalls* sys);

/* Poll the state machine. Returns 1 if the caller should run a parse
 * cycle NOW (clears dirty before returning), 0 otherwise. */
int debounce_poll(DebounceState* s, const MacSyscalls* sys);

#endif /* ARMA_DEBOUNCE_H */
