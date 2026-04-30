/*
 * armadillo.r — Rez resource file for Armadillo Editor
 *
 * This is a minimal stub. Plan 2 expands it with full MBAR/MENU/ALRT/
 * STR#/ICN# entries per the design doc §7.5.
 */

#include "Types.r"

/* Size Manager resource.
 * 2 MB preferred / 1 MB minimum — MVP stub sizing; Plan 2 tunes
 * against real arena + TE footprints. 32-bit clean for 68030+ hosts
 * (no 24-bit addressing assumed). */
resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    canBackground,
    multiFinderAware,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    is32BitCompatible,     /* set via size flags; Retro68 respects 32-bit clean code */
    isHighLevelEventAware,
    localAndRemoteHLEvents,
    notStationeryAware,
    dontUseTextEditServices,
    reserved,
    reserved,
    reserved,
    2 * 1024 * 1024,        /* preferred size: 2 MB */
    1 * 1024 * 1024         /* minimum size:   1 MB */
};
