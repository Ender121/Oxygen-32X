#include "game/widescreen.h"
#include "runtime/patches.h"
#include <cstdint>

namespace o32x {

namespace {

int g_margin = 0;
int g_cam_a = 0;             // the foreground camera x, as of its last update
unsigned g_tick = 0;         // counts those updates
unsigned g_seen_tick = 0, g_stale = 0;
bool g_row_clamped = false;  // the row being queued was started at x = 0 instead of cam - margin

// The layers keep the world from camera x to x + 512 in their 64-cell plane
// (new columns come in at x + 512 when it scrolls right, at x when it scrolls
// left, whole rows start at x). Starting everything `margin` pixels earlier
// keeps margin pixels left of the screen loaded too, and the 512 - 320 spare
// ones on the right, which the game loaded anyway. d0 is the x position.
void m68k_hook(m68k::State* c, uint32_t pc) {
    if (pc == 0x889A20) {  // end of the camera update (a1 = the layer)
        if (c->a[1] == 0xFFFFC1DEu) {
            const int32_t cycles = c->cycles;
            g_cam_a = m68k::rd16(c, c->a[1]);
            c->cycles = cycles;
            ++g_tick;
        }
        return;
    }
    if (pc == 0x8F4F26) {  // the camera x, from which the wrap counts of a queued row/column come
        const uint32_t start = g_row_clamped ? 0u : uint32_t(c->d[2] - uint32_t(g_margin));
        g_row_clamped = false;
        c->d[2] = (c->d[2] & 0xFFFF0000u) | (start & 0xFFFFu);
        return;
    }
    uint32_t x = (c->d[0] & 0xFFFFu) - uint32_t(g_margin);
    if (int32_t(x) < 0) {
        if (pc == 0x8F4E82) {
            // This row loop does not wrap at the level width: start the row at the
            // level's first column (what lies left of it is not drawn anyway).
            x = 0;
            g_row_clamped = true;
        } else {
            // Left of the level's start: wraps around the level width, as the game
            // does itself for positions it masks (a1 -> the layer's width mask).
            // (the read must not cost cycles: timing has to stay the game's own)
            const int32_t cycles = c->cycles;
            x &= m68k::rd16(c, c->a[1] + 4);
            c->cycles = cycles;
        }
    }
    c->d[0] = (c->d[0] & 0xFFFF0000u) | (x & 0xFFFFu);
}

} // namespace

void install_widescreen(Machine& m, int margin) {
    g_margin = margin & ~15;
    if (g_margin <= 0) {
        g_margin = 0;
        m.m68k.hook = nullptr;
        m.m68k.hook_pcs = nullptr;
        m.m68k.hook_count = 0;
        return;
    }
    m.m68k.hook = m68k_hook;
    m.m68k.hook_pcs = patches::kWidescreenM68kHooks;
    m.m68k.hook_count = uint32_t(sizeof patches::kWidescreenM68kHooks / sizeof patches::kWidescreenM68kHooks[0]);
}

void widescreen_prepare_render(RenderState& rs) {
    rs.margin = g_margin;
    rs.blank_left[0] = rs.blank_left[1] = 0;
    if (g_margin <= 0) return;
    // Not in a level (menus, ...): the camera is not updated, so keep what was
    // last known for a couple of seconds (a pause), then stop trusting it.
    if (g_tick != g_seen_tick) { g_seen_tick = g_tick; g_stale = 0; }
    else if (g_stale < 180) ++g_stale;
    if (g_stale >= 180) return;
    if (g_cam_a >= 0 && g_cam_a < g_margin) rs.blank_left[0] = g_margin - g_cam_a;
}

} // namespace o32x
