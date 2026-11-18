// Headless runner: oxygen32x_run <rom.32x> [frames] [dump.ppm]
#include "machine/machine.h"
#include "machine/renderer.h"
#include "game/recomp_dispatch.h"
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <memory>

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: %s rom [frames] [dump.ppm]\n", argv[0]); return 2; }
    auto m = std::make_unique<o32x::Machine>();
    std::string err;
    if (!m->load_rom(argv[1], &err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    m->reset();
    m->audio_enabled = true;
    int frames = argc > 2 ? std::atoi(argv[2]) : 60;
    if (std::getenv("O32X_NO_Z80SKIP")) z80::g_idle_skip = false;
    const bool recomp = std::getenv("O32X_RECOMP") != nullptr;
    auto rs = o32x::install_recompiled_code(*m, recomp);
    std::printf("exec: %s\n", rs.description.c_str());
    const bool hashes = std::getenv("O32X_HASH") != nullptr;
    auto fr0 = std::make_unique<o32x::Frame>();
    uint64_t h = 1469598103934665603ull;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < frames; ++i) {
        m->run_frame();
        if (hashes && i % 50 == 49) {
            o32x::render_frame(*m, *fr0);
            for (int k = 0; k < fr0->width * fr0->height; ++k) h = (h ^ fr0->pixels[k]) * 1099511628211ull;
            std::printf("f%d %016llx comm0=%04X\n", i + 1, (unsigned long long)h, m->mars.comm[0]);
        }
    }
    { uint64_t ah = 1469598103934665603ull; for (int16_t v : m->audio_out) ah = (ah ^ uint16_t(v)) * 1099511628211ull; std::printf("audio %zu %016llx\n", m->audio_out.size(), (unsigned long long)ah); }
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("%.1f fps (%.3f ms/frame)\n", frames / secs, 1000 * secs / frames);
    std::printf("frames=%d m68k=%llu msh2=%llu ssh2=%llu comm0=%04X\n", frames,
        (unsigned long long)m->stats.m68k_cycles, (unsigned long long)m->stats.msh2_cycles,
        (unsigned long long)m->stats.ssh2_cycles, m->mars.comm[0]);
    if (argc > 3) {
        FILE* f = std::fopen(argv[3], "wb");
        if (!f) return 1;
        auto fr = std::make_unique<o32x::Frame>();
        o32x::render_frame(*m, *fr);
        std::fprintf(f, "P6\n%d %d\n255\n", fr->width, fr->height);
        for (int i = 0; i < fr->width * fr->height; ++i) {
            uint32_t c = fr->pixels[i];
            uint8_t px[3] = { uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c) };
            std::fwrite(px, 1, 3, f);
        }
        std::fclose(f);
    }
    return 0;
}
