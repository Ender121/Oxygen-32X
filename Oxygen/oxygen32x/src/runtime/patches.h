// Game patches. The recompiler asks which code addresses are hooked (a hook runs
// before the instruction at that address, in interpreted and generated code
// alike), so the list lives here as constants; src/game/widescreen.cpp has what
// the hooks do.
#pragma once
#include <cstdint>

namespace o32x::patches {

// Widescreen: the routines that stream the level's tile columns/rows into the
// Mega Drive planes (shared by the foreground and background layers); the last
// one is where the row/column wrap counts are derived from the camera x (d2); 889A20
// is the end of the foreground camera update (reports the camera x).
constexpr uint32_t kWidescreenM68kHooks[] = {0x889934, 0x889976, 0x889A20, 0x8F4E82, 0x8F4EAA, 0x8F4F26};  // sorted

constexpr bool is_m68k_hook(uint32_t pc) {
    for (uint32_t h : kWidescreenM68kHooks)
        if (h == pc) return true;
    return false;
}
constexpr bool is_sh2_hook(uint32_t) { return false; }
constexpr bool is_patched_sh2_immediate(uint32_t) { return false; }

} // namespace o32x::patches
