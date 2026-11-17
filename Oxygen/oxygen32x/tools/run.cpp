// Headless runner: oxygen32x_run <rom.32x> [frames] [dump.ppm]
#include "machine/machine.h"
#include <cstdio>
#include <cstdlib>
#include <memory>

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: %s rom [frames] [dump.ppm]\n", argv[0]); return 2; }
    auto m = std::make_unique<o32x::Machine>();
    std::string err;
    if (!m->load_rom(argv[1], &err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    m->reset();
    int frames = argc > 2 ? std::atoi(argv[2]) : 60;
    for (int i = 0; i < frames; ++i) m->run_frame();
    std::printf("frames=%d m68k=%llu msh2=%llu ssh2=%llu comm0=%04X\n", frames,
        (unsigned long long)m->stats.m68k_cycles, (unsigned long long)m->stats.msh2_cycles,
        (unsigned long long)m->stats.ssh2_cycles, m->mars.comm[0]);
    if (argc > 3) {
        FILE* f = std::fopen(argv[3], "wb");
        if (!f) return 1;
        std::fprintf(f, "P6\n320 224\n255\n");
        const uint8_t* fb = m->mars.fb[m->mars.fb_display];
        for (int y = 0; y < 224; ++y) for (int x = 0; x < 320; ++x) {
            uint16_t c = m->mars.pal[fb[(y * 320 + x) ^ 1 & 0x1FFFF]];
            uint8_t px[3] = { uint8_t((c & 31) << 3), uint8_t(((c >> 5) & 31) << 3), uint8_t(((c >> 10) & 31) << 3) };
            std::fwrite(px, 1, 3, f);
        }
        std::fclose(f);
    }
    return 0;
}
