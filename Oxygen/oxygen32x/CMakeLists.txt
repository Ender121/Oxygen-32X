cmake_minimum_required(VERSION 3.20)
project(oxygen32x_cpu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CPU cores for the Sega 32X: Motorola 68000, Hitachi SH-2 (SH7604) and Zilog Z80.
# Decoders, shared instruction semantics and reference interpreters.
# Imported from ChaotixRecompiled (MIT), see README.md and LICENSE-ChaotixRecompiled.
add_library(oxygen32x_cpu STATIC
    src/cpu/m68k/m68k_decode.cpp
    src/cpu/m68k/m68k_disasm.cpp
    src/cpu/m68k/m68k_interp.cpp
    src/cpu/sh2/sh2_decode.cpp
    src/cpu/sh2/sh2_interp.cpp
    src/cpu/z80/z80.cpp
)

# Machine layer: Sega 32X + Mega Drive buses, registers, boot HLE, scheduler,
# VDP state and sound chips. No rendering.
add_library(oxygen32x_machine STATIC
    src/audio/ym2612.cpp
    src/audio/sn76489.cpp
    src/machine/log.cpp
    src/machine/rom.cpp
    src/machine/vdp.cpp
    src/machine/renderer.cpp
    src/machine/machine.cpp
    src/machine/md_bus.cpp
    src/machine/sh2_bus.cpp
    src/machine/sh2_onchip.cpp
)
target_link_libraries(oxygen32x_machine PUBLIC oxygen32x_cpu)
if(NOT MSVC)
    target_compile_options(oxygen32x_machine PRIVATE -Wall -Wextra -Wno-unused-label -Wno-unused-parameter)
endif()

# Headless runner: load a ROM, run N frames, print stats / dump the 32X frame buffer
add_executable(oxygen32x_run tools/run.cpp)
target_link_libraries(oxygen32x_run PRIVATE oxygen32x_machine)
target_include_directories(oxygen32x_cpu PUBLIC src)
if(NOT MSVC)
    target_compile_options(oxygen32x_cpu PRIVATE -Wall -Wextra -Wno-unused-label -Wno-unused-parameter)
endif()

# Unit tests (no ROM needed)
enable_testing()
add_executable(oxygen32x_tests
    tests/test_main.cpp
    tests/test_m68k.cpp
    tests/test_sh2.cpp
    tests/test_machine.cpp
)
target_link_libraries(oxygen32x_tests PRIVATE oxygen32x_machine)
add_test(NAME oxygen32x_cpu_tests COMMAND oxygen32x_tests)
