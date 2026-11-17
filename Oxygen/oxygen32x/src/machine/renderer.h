// Software renderer: composes the Mega Drive VDP layers and the 32X VDP into an
// XRGB8888 picture. It only reads machine state; nothing is written back.
// Derived from ChaotixRecompiled's VDP/compositor code (MIT), see
// LICENSE-ChaotixRecompiled, with all game-specific features removed.
#pragma once
#include "machine/machine.h"
#include <cstdint>

namespace o32x {

constexpr int kFrameWidth = 320;
constexpr int kFrameMaxHeight = 240;

struct Frame {
    int width = kFrameWidth;
    int height = 224;
    uint32_t pixels[kFrameWidth * kFrameMaxHeight];  // XRGB8888 (alpha 0xFF)
};

// Renders the picture the machine shows right now.
void render_frame(const Machine& m, Frame& out);

// One Mega Drive line, as palette indices (0-63, bit 6 = backdrop) already
// resolved for priorities (and shadow/highlight, folded into the 0x80/0x100
// variants of `rgb`). Exposed for tests/tools.
void render_md_line(const Vdp& v, int line, uint32_t* rgb, uint8_t* top);

} // namespace o32x
