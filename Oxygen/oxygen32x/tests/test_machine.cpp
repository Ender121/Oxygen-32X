// Machine smoke tests with a tiny synthetic 32X ROM (no commercial ROM needed).
#include "machine/machine.h"
#include "test_framework.h"
#include <cstring>
#include <memory>
#include <vector>

namespace {
std::vector<uint8_t> make_rom() {
    std::vector<uint8_t> r(0x1000, 0);
    auto w32 = [&](size_t o, uint32_t v) { r[o] = v >> 24; r[o + 1] = v >> 16; r[o + 2] = v >> 8; r[o + 3] = v; };
    w32(0, 0xFF0000);  // initial SP
    w32(4, 0x200);     // initial PC
    std::memcpy(&r[0x100], "SEGA 32X        ", 16);
    // 0x200: move.w #$5678,$FF0000 ; bra.s *
    const uint8_t code[] = {0x33, 0xFC, 0x56, 0x78, 0x00, 0xFF, 0x00, 0x00, 0x60, 0xFE};
    std::memcpy(&r[0x200], code, sizeof code);
    return r;
}
} // namespace

TEST(machine, rejects_non_32x_rom) {
    std::vector<uint8_t> r(0x1000, 0);
    o32x::Rom rom;
    std::string err;
    CHECK(!rom.load_memory(r.data(), r.size(), &err));
}

TEST(machine, runs_68k_code_for_frames) {
    auto bytes = make_rom();
    o32x::Rom rom;
    std::string err;
    CHECK(rom.load_memory(bytes.data(), bytes.size(), &err));
    auto m = std::make_unique<o32x::Machine>();
    m->load_rom(std::move(rom));
    m->reset();
    for (int i = 0; i < 3; ++i) m->run_frame();
    CHECK_EQ(m->frame_count, 3u);
    CHECK_EQ((m->wram[0] << 8) | m->wram[1], 0x5678);
    CHECK(m->stats.m68k_cycles > 0);
}

TEST(machine, deterministic_frames) {
    auto bytes = make_rom();
    uint8_t first[0x10000];
    for (int run = 0; run < 2; ++run) {
        o32x::Rom rom;
        rom.load_memory(bytes.data(), bytes.size(), nullptr);
        auto m = std::make_unique<o32x::Machine>();
        m->load_rom(std::move(rom));
        m->reset();
        for (int i = 0; i < 5; ++i) m->run_frame();
        if (run == 0) std::memcpy(first, m->wram, sizeof first);
        else CHECK(std::memcmp(first, m->wram, sizeof first) == 0);
    }
}
