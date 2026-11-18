# Integration of the statically recompiled game code.
#
# The recompiler (oxygen32x_recomp) reads the user's ROM and coverage traces and
# writes C++ into O32X_GENERATED_DIR. The file names are fixed (sharded) so
# they can be declared as build outputs.
#
#  * O32X_ROM set          -> generate during the build (host builds)
#  * files already generated  -> use them (e.g. cross builds for Android/iOS,
#                                generated on the host beforehand)
#  * neither                  -> interpreter-only build

set(_m68k_shards 16)
set(_sh2_shards 8)
set(O32X_GENERATED_SOURCES "")
math(EXPR _m68k_last "${_m68k_shards} - 1")
math(EXPR _sh2_last "${_sh2_shards} - 1")
foreach(i RANGE 0 ${_m68k_last})
    if(i LESS 10)
        set(n "00${i}")
    else()
        set(n "0${i}")
    endif()
    list(APPEND O32X_GENERATED_SOURCES "${O32X_GENERATED_DIR}/recomp_m68k_${n}.cpp")
endforeach()
foreach(i RANGE 0 ${_sh2_last})
    list(APPEND O32X_GENERATED_SOURCES "${O32X_GENERATED_DIR}/recomp_sh2_00${i}.cpp")
endforeach()
list(APPEND O32X_GENERATED_SOURCES "${O32X_GENERATED_DIR}/recomp_tables.cpp")
set(O32X_GENERATED_HEADER "${O32X_GENERATED_DIR}/recomp_decls.h")

file(GLOB O32X_COVERAGE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/coverage/*.cov")

set(O32X_HAVE_GENERATED OFF)
if(O32X_ROM AND EXISTS "${O32X_ROM}" AND TARGET oxygen32x_recomp)
    set(_cov_args "")
    foreach(c ${O32X_COVERAGE_FILES})
        list(APPEND _cov_args --coverage "${c}")
    endforeach()
    add_custom_command(
        OUTPUT ${O32X_GENERATED_SOURCES} ${O32X_GENERATED_HEADER}
        COMMAND oxygen32x_recomp --rom "${O32X_ROM}" ${_cov_args} --out "${O32X_GENERATED_DIR}"
        DEPENDS oxygen32x_recomp "${O32X_ROM}" ${O32X_COVERAGE_FILES}
        COMMENT "Recompiling the game (68000 + SH-2) to C++"
        VERBATIM)
    set(O32X_HAVE_GENERATED ON)
elseif(EXISTS "${O32X_GENERATED_DIR}/recomp_tables.cpp")
    set(O32X_HAVE_GENERATED ON)
    message(STATUS "Oxygen32X: using pre-generated code in ${O32X_GENERATED_DIR}")
else()
    set(O32X_GENERATED_SOURCES "")
    message(STATUS "Oxygen32X: no ROM given (-DO32X_ROM=...) and no generated code: building interpreter-only runtime")
endif()
