/*
 * armadillo.r — Rez resource file for Armadillo Editor
 *
 * This is a minimal stub. Plan 2 expands it with full MBAR/MENU/ALRT/
 * STR#/ICN# entries per the design doc §7.5.
 */

#include "Types.r"

/* Size flags the app needs — small heap + accepts suspend/resume events.
 * 2MB is a safe MVP stack + heap floor; tune later if needed. */
resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    canBackground,
    multiFinderAware,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    not32BitCompatible,     /* set via size flags; Retro68 respects 32-bit clean code */
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
