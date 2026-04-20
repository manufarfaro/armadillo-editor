cmake_minimum_required(VERSION 3.9)
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
    # Renderer + pipeline (Plan 1 modules land here)
    # render/arena.c
    # render/render.c
    # mdparse/mdparse.c
    # scanner/scanner.c
    # src/doc.c
    # src/debounce.c
    # src/file_io.c

    # App shell (Plan 2 adds these)
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

# 68030+ target floor; no 68040-only instructions.
target_compile_definitions(ArmadilloEditor PRIVATE
    kArmadilloTargetFloor=68030
)

# md4c defines — keep parser conservative for 68030 perf.
target_compile_definitions(ArmadilloEditor PRIVATE
    MD4C_USE_ASCII=1
)

set_target_properties(ArmadilloEditor PROPERTIES
    COMPILE_OPTIONS -ffunction-sections
    LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-single"
)
