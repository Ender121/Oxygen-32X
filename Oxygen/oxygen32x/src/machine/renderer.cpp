// Derived from ChaotixRecompiled (MIT), see LICENSE-ChaotixRecompiled.
#include "machine/renderer.h"
#include <algorithm>
#include <cstring>

namespace o32x {

namespace {

constexpr int kMaxW = 320;
// Layer pixel: palette index in bits 0-5 (0 = transparent), priority in bit 7.
using LinePix = uint8_t;
constexpr uint8_t kColor = 0x3F;
constexpr uint8_t kPrio = 0x80;

inline LinePix mk(unsigned color, unsigned prio) { return uint8_t((color & kColor) | (prio ? kPrio : 0)); }

inline uint8_t tile_px(const uint8_t* vram, uint32_t tile, int px, int py) {
    uint32_t a = ((tile & 0x7FF) << 5) + uint32_t(py) * 4 + uint32_t(px >> 1);
    uint8_t b = vram[a & 0xFFFF];
    return (px & 1) ? (b & 15) : (b >> 4);
}

inline uint16_t rd16(const uint8_t* vram, uint32_t a) {
    return uint16_t((vram[a & 0xFFFF] << 8) | vram[(a + 1) & 0xFFFF]);
}

void render_plane(const Vdp& v, int line, bool planeA, LinePix* out, int win_x0, int win_x1) {
    static const int sizes[4] = {32, 64, 32, 128};
    const int width = v.width();
    const int pw = sizes[v.reg[16] & 3];
    int ph = sizes[(v.reg[16] >> 4) & 3];
    if ((v.reg[16] & 3) == 3 && ph > 32) ph = 32;
    const uint32_t nt = planeA ? (uint32_t(v.reg[2] & 0x38) << 10) : (uint32_t(v.reg[4] & 0x07) << 13);
    const uint32_t hs_base = uint32_t(v.reg[13] & 0x3F) << 10;
    uint32_t hs_addr;
    switch (v.reg[11] & 3) {
    case 2: hs_addr = hs_base + uint32_t(line & ~7) * 4; break;
    case 3: hs_addr = hs_base + uint32_t(line) * 4; break;
    default: hs_addr = hs_base; break;
    }
    if (!planeA) hs_addr += 2;
    const int hscroll = rd16(v.vram, hs_addr) & 0x3FF;
    const bool col_vs = (v.reg[11] & 4) != 0;
    const int lane = planeA ? 0 : 1;
    for (int x = 0; x < width; ++x) {
        if (planeA && x >= win_x0 && x < win_x1) continue;
        const int col = std::min(x >> 4, 19);
        const int vs = v.vsram[col_vs ? col * 2 + lane : lane];
        const int py = (line + vs) & (ph * 8 - 1);
        const int px = (x - hscroll) & (pw * 8 - 1);
        const uint32_t ea = nt + uint32_t(((py >> 3) * pw + (px >> 3)) * 2);
        const uint16_t e = rd16(v.vram, ea);
        int tx = px & 7, ty = py & 7;
        if (e & 0x0800) tx = 7 - tx;
        if (e & 0x1000) ty = 7 - ty;
        const uint8_t c = tile_px(v.vram, e, tx, ty);
        out[x] = mk(c ? ((e >> 9) & 0x30) | c : 0, (e >> 15) & 1);
    }
}

void render_window(const Vdp& v, int line, LinePix* out, int x0, int x1) {
    const bool h40 = (v.reg[12] & 1) != 0;
    const uint32_t nt = h40 ? (uint32_t(v.reg[3] & 0x3C) << 10) : (uint32_t(v.reg[3] & 0x3E) << 10);
    const int pw = h40 ? 64 : 32;
    const int ty0 = line >> 3, ty = line & 7;
    for (int x = x0; x < x1; ++x) {
        const uint16_t e = rd16(v.vram, nt + uint32_t((ty0 * pw + (x >> 3)) * 2));
        int tx = x & 7, yy = ty;
        if (e & 0x0800) tx = 7 - tx;
        if (e & 0x1000) yy = 7 - yy;
        const uint8_t c = tile_px(v.vram, e, tx, yy);
        out[x] = mk(c ? ((e >> 9) & 0x30) | c : 0, (e >> 15) & 1);
    }
}

uint32_t blend_half(uint32_t rgb) { return 0xFF000000u | ((rgb & 0xFEFEFE) >> 1); }

} // namespace

// Resolves one MD line. `top` gets the winning palette index (bit 6: backdrop);
// `rgb` the final colour, with shadow/highlight applied.
namespace {
void render_md_line_pal(const Vdp& v, int line, const uint32_t* pal, uint32_t* rgbout, uint8_t* top);
}

void render_md_line(const Vdp& v, int line, uint32_t* rgbout, uint8_t* top) {
    uint32_t pal[64];
    for (int i = 0; i < 64; ++i) pal[i] = Vdp::cram_to_rgb(v.cram[i]) | 0xFF000000u;
    render_md_line_pal(v, line, pal, rgbout, top);
}

namespace {
void render_md_line_pal(const Vdp& v, int line, const uint32_t* pal, uint32_t* rgbout, uint8_t* top) {
    const int width = v.width();
    const uint8_t bgidx = v.reg[7] & 0x3F;
    if (!v.display_enabled()) {
        for (int x = 0; x < width; ++x) { top[x] = bgidx | 0x40; rgbout[x] = pal[bgidx]; }
        return;
    }
    LinePix a[kMaxW], b[kMaxW], s[kMaxW];
    std::memset(a, 0, sizeof a);
    std::memset(b, 0, sizeof b);
    std::memset(s, 0, sizeof s);

    int win_x0 = 0, win_x1 = 0;
    {
        const int wvp = (v.reg[18] & 0x1F) * 8;
        const bool down = (v.reg[18] & 0x80) != 0;
        const bool vwin = down ? (line >= wvp) : (line < wvp);
        if (vwin) { win_x0 = 0; win_x1 = width; }
        else {
            int whp = (v.reg[17] & 0x1F) * 16;
            if (whp > width) whp = width;
            if (v.reg[17] & 0x80) { win_x0 = whp; win_x1 = width; }
            else { win_x0 = 0; win_x1 = whp; }
        }
    }
    render_plane(v, line, false, b, 0, 0);
    render_plane(v, line, true, a, win_x0, win_x1);
    if (win_x1 > win_x0) render_window(v, line, a, win_x0, win_x1);

    // Sprites
    {
        const bool h40 = (v.reg[12] & 1) != 0;
        const uint32_t sat = h40 ? (uint32_t(v.reg[5] & 0x7E) << 9) : (uint32_t(v.reg[5] & 0x7F) << 9);
        const int max_sprites = h40 ? 80 : 64;
        const int max_per_line = h40 ? 20 : 16;
        const int max_pixels = h40 ? 320 : 256;
        int link = 0, count = 0, on_line = 0, pixels = 0;
        bool nonzero_x_seen = false, masked = false;
        uint8_t filled[kMaxW] = {};
        while (count < max_sprites) {
            const uint8_t* p = v.vram + ((sat + uint32_t(link) * 8) & 0xFFFF);
            const int sy = ((p[0] << 8 | p[1]) & 0x3FF) - 128;
            const uint16_t sz = uint16_t(p[2] << 8 | p[3]);
            const uint16_t pat = uint16_t(p[4] << 8 | p[5]);
            const int sxr = (p[6] << 8 | p[7]) & 0x1FF;
            const int hs = ((sz >> 10) & 3) + 1, vsz = ((sz >> 8) & 3) + 1;
            if (line >= sy && line < sy + vsz * 8) {
                if (++on_line > max_per_line) break;
                if (sxr == 0) { if (nonzero_x_seen) masked = true; }
                else nonzero_x_seen = true;
                const int sx = sxr - 128;
                int row = line - sy;
                if (pat & 0x1000) row = vsz * 8 - 1 - row;
                for (int cx = 0; cx < hs * 8; ++cx) {
                    if (pixels >= max_pixels) break;
                    ++pixels;
                    if (masked) continue;
                    const int x = sx + cx;
                    if (x < 0 || x >= width) continue;
                    const int col = (pat & 0x0800) ? (hs * 8 - 1 - cx) : cx;
                    const uint32_t tile = (pat & 0x7FF) + uint32_t((col >> 3) * vsz + (row >> 3));
                    const uint8_t c = tile_px(v.vram, tile, col & 7, row & 7);
                    if (!c) continue;
                    if (filled[x]) continue;
                    filled[x] = 1;
                    s[x] = mk(((pat >> 9) & 0x30) | c, (pat >> 15) & 1);
                }
            }
            link = p[3] & 0x7F;
            ++count;
            if (link == 0 || link >= max_sprites) break;
        }
    }

    const bool shi = (v.reg[12] & 0x08) != 0;
    for (int x = 0; x < width; ++x) {
        const uint8_t sc = s[x] & kColor, ac = a[x] & kColor, bc = b[x] & kColor;
        const bool sh = (s[x] & kPrio) != 0, ah = (a[x] & kPrio) != 0, bh = (b[x] & kPrio) != 0;
        uint8_t c;
        bool isbg = false;
        if (sc && sh) c = sc;
        else if (ac && ah) c = ac;
        else if (bc && bh) c = bc;
        else if (sc) c = sc;
        else if (ac) c = ac;
        else if (bc) c = bc;
        else { c = bgidx; isbg = true; }
        uint32_t rgb = pal[c];
        if (shi) {
            bool shadow = !(ah || bh);
            if (sc == 0x3E || sc == 0x3F) {
                uint8_t u;
                if (ac && ah) u = ac;
                else if (bc && bh) u = bc;
                else if (ac) u = ac;
                else if (bc) u = bc;
                else u = bgidx;
                rgb = pal[u];
                if (sc == 0x3E) {
                    if (!shadow) rgb = 0xFF000000u | (((rgb & 0xFEFEFE) >> 1) + 0x808080);
                    shadow = false;
                } else {
                    shadow = true;
                }
            } else if (sc && sh) {
                shadow = false;
            }
            if (shadow) rgb = blend_half(rgb);
        }
        rgbout[x] = rgb;
        top[x] = isbg ? uint8_t(c | 0x40) : c;
    }
}
} // namespace

namespace {
inline uint32_t rgb555(uint16_t c) {
    uint32_t r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    r = (r << 3) | (r >> 2); g = (g << 3) | (g >> 2); b = (b << 3) | (b >> 2);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}
} // namespace

void render_frame(const Machine& m, Frame& out) {
    const Vdp& v = m.vdp;
    const int mdw = v.width();
    out.width = kFrameWidth;
    out.height = v.height();
    const Mars& mars = m.mars;
    const int mode = mars.bitmap_mode & 3;
    const bool show32 = mode != 0 && (mars.adapter_ctrl & 1);
    const bool pri = (mars.bitmap_mode & 0x80) != 0;
    const uint8_t* fb = mars.fb[mars.fb_display];

    uint32_t mdpal[64];
    for (int i = 0; i < 64; ++i) mdpal[i] = Vdp::cram_to_rgb(v.cram[i]) | 0xFF000000u;
    uint32_t pal32[256];  // 32X palette as XRGB, with bit 15 kept apart
    uint8_t thr32[256];
    for (int i = 0; i < 256; ++i) { pal32[i] = rgb555(mars.pal[i]); thr32[i] = (mars.pal[i] & 0x8000) != 0; }

    uint32_t mdrgb[kMaxW];
    uint8_t mdtop[kMaxW];
    uint32_t l32[kFrameWidth];
    bool wins[kFrameWidth];  // the 32X pixel is shown regardless of the Mega Drive
    for (int ln = 0; ln < out.height; ++ln) {
        uint32_t* dst = out.pixels + ln * kFrameWidth;
        bool need_md = true;
        if (show32) {
            const int lt = ln >= 224 ? 223 : ln;
            const uint32_t lineaddr = (uint32_t(fb[lt * 2]) << 8) | fb[lt * 2 + 1];
            const uint32_t base = (lineaddr * 2) & 0x1FFFF;
            uint32_t rle_o = base;
            int rle_left = 0;
            uint16_t rle_px = 0;
            bool all = true;
            for (int x = 0; x < kFrameWidth; ++x) {
                uint32_t rgb;
                bool through;
                if (mode == 1) {
                    const uint8_t idx = fb[(base + (mars.screen_shift & 1) + uint32_t(x)) & 0x1FFFF];
                    rgb = pal32[idx]; through = thr32[idx] != 0;
                } else if (mode == 2) {
                    const uint32_t o = (base + uint32_t(x) * 2) & 0x1FFFF;
                    const uint16_t c = uint16_t((fb[o] << 8) | fb[o + 1]);
                    rgb = rgb555(c); through = (c & 0x8000) != 0;
                } else {
                    if (rle_left == 0) {
                        rle_left = fb[rle_o & 0x1FFFF] + 1;
                        rle_px = mars.pal[fb[(rle_o + 1) & 0x1FFFF]];
                        rle_o += 2;
                    }
                    --rle_left;
                    rgb = rgb555(rle_px); through = (rle_px & 0x8000) != 0;
                }
                l32[x] = rgb;
                wins[x] = through != pri;
                all = all && wins[x];
            }
            if (all) {  // the 32X covers the whole line: no need to draw the Mega Drive
                std::memcpy(dst, l32, sizeof l32);
                need_md = false;
            }
        }
        if (!need_md) continue;
        render_md_line_pal(v, ln, mdpal, mdrgb, mdtop);
        for (int x = 0; x < kFrameWidth; ++x) dst[x] = mdrgb[mdw == 320 ? x : x * 256 / 320];
        if (!show32) continue;
        for (int x = 0; x < kFrameWidth; ++x) {
            const bool md_backdrop = (mdtop[mdw == 320 ? x : x * 256 / 320] & 0x40) != 0;
            if (wins[x] || md_backdrop) dst[x] = l32[x];
        }
    }
}

} // namespace o32x
