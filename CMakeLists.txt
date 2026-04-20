cmake_minimum_required(VERSION 3.15)
project(ArmadilloEditor C)

#
# Armadillo Editor — Retro68 cross-compile targets.
#
# This CMakeLists builds the .APPL for classic Mac (68030+). Host unit
# tests are built via a separate Makefile.hosttests to keep the two
# toolchains from interfering.
#
# The Retro68 toolchain file is expected in
# ~/Documents/Projects/Retro68-build/toolchain/m68k-apple-macos/cmake/
# retro68.toolchain.cmake. Invoke with:
#
#   cd build && cmake .. \
#     -DCMAKE_TOOLCHAIN_FILE=$HOME/Documents/Projects/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
#
# The toolchain file provides add_application() and the Rez resource
# pipeline.
#

# md4c vendored source
set(MD4C_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/md4c/src)
set(MD4C_SOURCES ${MD4C_DIR}/md4c.c)

# The app target. Source list is incremental — modules are added as
# they're implemented across Plan 1 and Plan 2.
add_application(ArmadilloEditor
    # Phase 1 stub main (replaced by src/app.c in Plan 2)
    src/stub_main.c

    # Renderer + pipeline (added by their Plan 1 phases)
    # render/arena.c
    # render/render.c
    # mdparse/mdparse.c
    # scanner/scanner.c
    # src/doc.c
    # src/debounce.c
    # src/file_io.c

    # App shell (Plan 2 replaces stub_main.c with these)
    # src/app.c
    # src/menus.c
    # src/win_editor.c
    # src/draw_qd_real.c
    # src_pane/src_pane.c

    # Third-party
    ${MD4C_SOURCES}

    # Resources
    armadillo.r
)

target_include_directories(ArmadilloEditor PRIVATE
    src
    src_pane
    render
    mdparse
    scanner
    third_party/md4c/src
)

# md4c defines.
# MD4C_USE_ASCII=1 — disables md4c's Unicode word-class tables
# (trades ~few-KB lookups for a byte compare per char on a 25 MHz
# target). Input is still parsed byte-for-byte; it's only word-
# boundary detection inside spans that becomes ASCII-only.
# MacRoman / UTF-8 documents still round-trip cleanly.
target_compile_definitions(ArmadilloEditor PRIVATE
    MD4C_USE_ASCII=1
)

# Compile options.
# -ffunction-sections + -Wl,-gc-sections lets the linker drop
# unreferenced functions (smaller .APPL).
# -Os optimizes for size — critical on 25 MHz 68030 with tight RAM.
# -Wall catches common bugs; md4c gets -w below because we don't
# own its source and its warnings are not our problem.
# TODO (Plan 2): decide whether to add -mcpu=68020 or -mcpu=68030.
# Default (no -mcpu) compiles for gcc's 68000 baseline, which
# loses any 68020+ optimizations but is maximally conservative.
# Add -mcpu=... once we want to commit to the CPU floor in code.
target_compile_options(ArmadilloEditor PRIVATE
    -ffunction-sections
    -Os
    -Wall
)

# Suppress warnings on vendored md4c — we don't own its source.
set_source_files_properties(${MD4C_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-w"
)

target_link_options(ArmadilloEditor PRIVATE
    "LINKER:-gc-sections"
    "LINKER:--mac-single"
)
