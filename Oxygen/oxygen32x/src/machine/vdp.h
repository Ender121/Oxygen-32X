// Mega Drive VDP (YM7101) state: registers, VRAM / CRAM / VSRAM, control and
// data ports, DMA, status and interrupts. It draws nothing: the rendering is
// left to the host, which reads the state below (planned: map it onto the
// Oxygen render parts).
// Derived from ChaotixRecompiled (MIT), see LICENSE-ChaotixRecompiled.
#pragma once
#include <cstdint>

namespace o32x {

struct VdpHost {
    void* user = nullptr;
    // Word read from the 68K bus for 68K->VDP DMA.
    uint16_t (*dma_read16)(void* user, uint32_t addr) = nullptr;
};

class Vdp {
public:
    uint8_t vram[0x10000];
    uint16_t cram[64];
    uint16_t vsram[40];
    uint8_t reg[32];

    // control port state
    bool cmd_pending = false;
    uint16_t cmd_first = 0;
    uint32_t addr = 0;
    uint8_t code = 0;
    bool fill_pending = false;

    // interrupt state
    bool vint_pending = false;
    bool hint_pending = false;
    int hint_counter = 0;

    // status bits maintained by timing
    bool in_vblank = false;
    bool odd_frame = false;
    bool sprite_overflow = false;
    bool sprite_collision = false;

    VdpHost host;
    // Tooling hook: called for every VRAM word write (address, value).
    void (*on_vram_write)(void* user, uint32_t addr, uint16_t v) = nullptr;
    void* on_vram_write_user = nullptr;
    int dma_stall_cycles = 0;   // 68K cycles consumed by the last DMA (caller drains)

    void reset();

    void write_ctrl(uint16_t v);
    void write_data(uint16_t v);
    uint16_t read_data();
    uint16_t read_status(bool hblank);
    uint16_t read_hv(int line, int hpos_mclk) const;

    bool display_enabled() const { return (reg[1] & 0x40) != 0; }
    bool vint_enabled() const { return (reg[1] & 0x20) != 0; }
    bool hint_enabled() const { return (reg[0] & 0x10) != 0; }
    int width() const { return (reg[12] & 0x01) ? 320 : 256; }
    int height() const { return (reg[1] & 0x08) ? 240 : 224; }

    // Timing hook called by the scheduler at the start of every line.
    void on_line_start(int line, int active_lines);

    // A CRAM colour (0000BBB0GGG0RRR0) as XRGB8888 with alpha 0xFF.
    static uint32_t cram_to_rgb(uint16_t c);

private:
    void do_dma_68k(uint32_t len);
    void do_dma_copy(uint32_t len);
    void do_dma_fill(uint16_t data, uint32_t len);
    void write_vram_word(uint32_t a, uint16_t v);
    uint32_t dma_length() const;
    uint32_t dma_source() const;
    void set_dma_source(uint32_t words);
    void set_dma_length_zero();
};

} // namespace o32x
