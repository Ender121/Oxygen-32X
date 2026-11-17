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
)
target_link_libraries(oxygen32x_tests PRIVATE oxygen32x_cpu)
add_test(NAME oxygen32x_cpu_tests COMMAND oxygen32x_tests)
