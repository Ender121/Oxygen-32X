// The Chaotix runtime patches game code and data at run time (widescreen and
// the like); this module runs the game as it is, so no code is ever treated as
// patched or hooked. The recompiler asks, so the answers live here.
#pragma once
#include <cstdint>

namespace o32x::patches {

constexpr bool is_m68k_hook(uint32_t) { return false; }
constexpr bool is_sh2_hook(uint32_t) { return false; }
constexpr bool is_patched_sh2_immediate(uint32_t) { return false; }

} // namespace o32x::patches
