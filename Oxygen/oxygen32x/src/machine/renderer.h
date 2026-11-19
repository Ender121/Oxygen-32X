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
// Widescreen: up to this many pixels across (the 32X picture stays 320 wide, centred).
constexpr int kFrameMaxWidth = 480;

struct Frame {
    int width = kFrameWidth;
    int height = 224;
    uint32_t pixels[kFrameMaxWidth * kFrameMaxHeight];  // XRGB8888 (alpha 0xFF), rows of `width`
};

// Everything the renderer reads, copied out of the machine: a picture can be
// drawn from this on another thread while the machine runs on.
struct RenderState {
    Vdp vdp;
    uint16_t bitmap_mode = 0, screen_shift = 0, adapter_ctrl = 0;
    uint16_t pal[256];
    uint8_t fb[0x20000];  // the displayed 32X frame buffer (not copied when unused)
    bool has_fb = false;
    int margin = 0;  // widescreen: Mega Drive pixels added on each side
    int blank_left[2] = {0, 0};  // plane A, B: pixels (from the left edge of the widened picture) before the level's start
};
void snapshot_render_state(const Machine& m, RenderState& out);
void render_state(const RenderState& s, Frame& out);

// Renders the picture the machine shows right now.
void render_frame(const Machine& m, Frame& out);

// One Mega Drive line, as palette indices (0-63, bit 6 = backdrop) already
// resolved for priorities (and shadow/highlight, folded into the 0x80/0x100
// variants of `rgb`). Exposed for tests/tools.
void render_md_line(const Vdp& v, int line, uint32_t* rgb, uint8_t* top);

} // namespace o32x
