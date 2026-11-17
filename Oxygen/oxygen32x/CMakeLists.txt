cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
project(oxygen32x_cpu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()
# The CPU cores are interpreters: LTO gives a measurable speed-up.
include(CheckIPOSupported)
check_ipo_supported(RESULT O32X_IPO OUTPUT O32X_IPO_MSG LANGUAGES CXX)
if(O32X_IPO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

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

# SDL2 player (desktop and Android). Uses the SDL2 bundled with the repo unless
# a system SDL2 is requested with -DOXYGEN32X_SYSTEM_SDL=ON.
option(OXYGEN32X_BUILD_PLAYER "Build the SDL2 player" ON)
option(OXYGEN32X_SYSTEM_SDL "Use the system SDL2 instead of the bundled one" OFF)
if(OXYGEN32X_BUILD_PLAYER)
    if(OXYGEN32X_SYSTEM_SDL)
        find_package(SDL2 REQUIRED)
    elseif(NOT TARGET SDL2::SDL2)
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../framework/external/sdl/SDL2 sdl2 EXCLUDE_FROM_ALL)
    endif()
    if(ANDROID)
        add_library(main SHARED player/player.cpp)
    else()
        add_executable(oxygen32x_player player/player.cpp)
    endif()
    set(O32X_PLAYER_TARGET ${CMAKE_PROJECT_NAME})
    if(ANDROID)
        target_link_libraries(main PRIVATE oxygen32x_machine SDL2::SDL2 SDL2::SDL2main)
    else()
        target_link_libraries(oxygen32x_player PRIVATE oxygen32x_machine SDL2::SDL2)
        if(TARGET SDL2::SDL2main)
            target_link_libraries(oxygen32x_player PRIVATE SDL2::SDL2main)
        endif()
    endif()
endif()
