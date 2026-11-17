// Derived from ChaotixRecompiled (MIT), see LICENSE-ChaotixRecompiled.
#include "machine/vdp.h"
#include "machine/log.h"
#include <cstring>

namespace o32x {

void Vdp::reset() {
    std::memset(vram, 0, sizeof vram);
    std::memset(cram, 0, sizeof cram);
    std::memset(vsram, 0, sizeof vsram);
    std::memset(reg, 0, sizeof reg);
    cmd_pending = false;
    cmd_first = 0;
    addr = 0;
    code = 0;
    fill_pending = false;
    vint_pending = hint_pending = false;
    hint_counter = 0;
    in_vblank = false;
    odd_frame = false;
    sprite_overflow = sprite_collision = false;
    dma_stall_cycles = 0;
}

uint32_t Vdp::cram_to_rgb(uint16_t c) {
    static const uint8_t lv[8] = {0, 36, 73, 109, 146, 182, 219, 255};
    uint32_t r = lv[(c >> 1) & 7], g = lv[(c >> 5) & 7], b = lv[(c >> 9) & 7];
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

uint32_t Vdp::dma_length() const {
    uint32_t len = reg[19] | (uint32_t(reg[20]) << 8);
    return len ? len : 0x10000;
}
uint32_t Vdp::dma_source() const {
    return (uint32_t(reg[21]) << 1) | (uint32_t(reg[22]) << 9) | (uint32_t(reg[23] & 0x7F) << 17);
}
void Vdp::set_dma_source(uint32_t words) {
    // Source address increments within a 128 KiB window (low 16 bits of word address).
    uint32_t src = (reg[21] | (uint32_t(reg[22]) << 8)) + words;
    reg[21] = uint8_t(src);
    reg[22] = uint8_t(src >> 8);
}
void Vdp::set_dma_length_zero() { reg[19] = 0; reg[20] = 0; }

void Vdp::write_vram_word(uint32_t a, uint16_t v) {
    a &= 0xFFFF;
    if (on_vram_write) on_vram_write(on_vram_write_user, a, v);
    vram[a] = uint8_t(v >> 8);
    vram[a ^ 1] = uint8_t(v);
}

void Vdp::write_ctrl(uint16_t v) {
    if (!cmd_pending) {
        if ((v & 0xC000) == 0x8000) {
            int r = (v >> 8) & 0x1F;
            if (r < 24) reg[r] = uint8_t(v);
            // Register writes also clear the code register's low bits (hardware quirk).
            code = 0;
            return;
        }
        cmd_first = v;
        cmd_pending = true;
        code = uint8_t((code & 0x3C) | ((v >> 14) & 3));
        addr = (addr & 0xC000) | (v & 0x3FFF);
        return;
    }
    cmd_pending = false;
    code = uint8_t((code & 0x03) | ((v >> 2) & 0x3C));
    addr = (addr & 0x3FFF) | (uint32_t(v & 3) << 14);
    if ((code & 0x20) && (reg[1] & 0x10)) {  // DMA requested and enabled
        int mode = reg[23] >> 6;
        uint32_t len = dma_length();
        if (mode <= 1) do_dma_68k(len);
        else if (mode == 3) do_dma_copy(len);
        else fill_pending = true;  // fill starts on next data port write
    }
}

void Vdp::do_dma_68k(uint32_t len) {
    uint32_t src = dma_source();
    const uint32_t inc = reg[15];
    for (uint32_t i = 0; i < len; ++i) {
        uint32_t a = (src & 0xFE0000) | ((src + i * 2) & 0x1FFFF);
        uint16_t w = host.dma_read16(host.user, a);
        switch (code & 0x0F) {
        case 1: write_vram_word(addr, w); break;
        case 3: cram[(addr >> 1) & 63] = w & 0x0EEE; break;
        case 5: if (((addr >> 1) & 63) < 40) vsram[(addr >> 1) & 63] = w & 0x07FF; break;
        default: break;
        }
        addr = (addr + inc) & 0xFFFF;
    }
    set_dma_source(len);
    set_dma_length_zero();
    code &= ~0x20;
    // Approximate bus stall: ~205 words/line in blanking, ~18 during display.
    bool blank = in_vblank || !display_enabled();
    dma_stall_cycles += int(len * (blank ? 488 / 205 + 1 : 488 / 18));
}

void Vdp::do_dma_copy(uint32_t len) {
    uint32_t src = reg[21] | (uint32_t(reg[22]) << 8);
    const uint32_t inc = reg[15];
    for (uint32_t i = 0; i < len; ++i) {
        vram[addr & 0xFFFF] = vram[(src + i) & 0xFFFF];
        addr = (addr + inc) & 0xFFFF;
    }
    set_dma_source(len);
    set_dma_length_zero();
    code &= ~0x20;
}

void Vdp::do_dma_fill(uint16_t data, uint32_t len) {
    const uint32_t inc = reg[15];
    switch (code & 0x0F) {
    case 1:
        for (uint32_t i = 0; i < len; ++i) {
            vram[(addr ^ 1) & 0xFFFF] = uint8_t(data >> 8);
            addr = (addr + inc) & 0xFFFF;
        }
        break;
    case 3:
        for (uint32_t i = 0; i < len; ++i) { cram[(addr >> 1) & 63] = data & 0x0EEE; addr = (addr + inc) & 0xFFFF; }
        break;
    case 5:
        for (uint32_t i = 0; i < len; ++i) {
            if (((addr >> 1) & 63) < 40) vsram[(addr >> 1) & 63] = data & 0x07FF;
            addr = (addr + inc) & 0xFFFF;
        }
        break;
    default: break;
    }
    set_dma_source(len);
    set_dma_length_zero();
    code &= ~0x20;
}

void Vdp::write_data(uint16_t v) {
    cmd_pending = false;
    switch (code & 0x0F) {
    case 1: write_vram_word(addr, v); break;
    case 3: cram[(addr >> 1) & 63] = v & 0x0EEE; break;
    case 5: if (((addr >> 1) & 63) < 40) vsram[(addr >> 1) & 63] = v & 0x07FF; break;
    default: O32X_LOG_LIMITED(LogLevel::Debug, "vdp", 8, "data write with code %X", code); break;
    }
    addr = (addr + reg[15]) & 0xFFFF;
    if (fill_pending) {
        fill_pending = false;
        do_dma_fill(v, dma_length());
    }
}

uint16_t Vdp::read_data() {
    cmd_pending = false;
    uint16_t v = 0;
    switch (code & 0x0F) {
    case 0: v = uint16_t((vram[addr & 0xFFFE] << 8) | vram[(addr & 0xFFFE) | 1]); break;
    case 4: v = vsram[((addr >> 1) & 63) % 40]; break;
    case 8: v = cram[(addr >> 1) & 63]; break;
    default: break;
    }
    addr = (addr + reg[15]) & 0xFFFF;
    return v;
}

uint16_t Vdp::read_status(bool hblank) {
    uint16_t s = 0x3400 | 0x0200;  // FIFO empty
    if (vint_pending) s |= 0x80;
    if (sprite_overflow) s |= 0x40;
    if (sprite_collision) s |= 0x20;
    if (odd_frame) s |= 0x10;
    if (in_vblank || !display_enabled()) s |= 0x08;
    if (hblank) s |= 0x04;
    cmd_pending = false;
    sprite_overflow = sprite_collision = false;
    return s;
}

uint16_t Vdp::read_hv(int line, int hpos_mclk) const {
    // H40: 420 pixel clocks per line (3420 mclk / 8.14). Map to the internal
    // counter sequence 0x00..0xB6, 0xE4..0xFF.
    int pix = hpos_mclk * 420 / 3420;
    int h;
    if (pix <= 0xB6) h = pix; else h = 0xE4 + (pix - 0xB7);
    if (h > 0xFF) h = 0xFF;
    int v = line;
    if (v > 0xEA) v = v - 0xEB + 0xE5;
    return uint16_t(((v & 0xFF) << 8) | (h & 0xFF));
}

void Vdp::on_line_start(int line, int active_lines) {
    if (line <= active_lines) {
        if (line == 0) hint_counter = reg[10];
        else if (hint_counter-- == 0) {
            hint_counter = reg[10];
            hint_pending = true;
        }
    } else {
        hint_counter = reg[10];
    }
    if (line == active_lines) {
        in_vblank = true;
        vint_pending = true;
    }
    if (line == 0) {
        in_vblank = false;
        if (reg[12] & 0x02) odd_frame = !odd_frame;
    }
}

} // namespace o32x
