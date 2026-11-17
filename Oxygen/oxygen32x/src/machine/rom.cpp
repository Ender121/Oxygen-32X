// Derived from ChaotixRecompiled (MIT), see LICENSE-ChaotixRecompiled.
#include "machine/rom.h"
#include <cstdio>
#include <cstring>

namespace o32x {

bool Rom::load(const std::string& path, std::string* error) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { if (error) *error = "cannot open ROM: " + path; return false; }
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 8 * 1024 * 1024) { std::fclose(f); if (error) *error = "unexpected ROM size"; return false; }
    std::vector<uint8_t> bytes(static_cast<size_t>(sz));
    size_t got = std::fread(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    if (got != bytes.size()) { if (error) *error = "short read"; return false; }
    return load_memory(bytes.data(), bytes.size(), error);
}

bool Rom::load_memory(const uint8_t* bytes, size_t n, std::string* error) {
    if (n < 0x800) { if (error) *error = "ROM too small"; return false; }
    if (std::memcmp(bytes + 0x100, "SEGA 32X", 8) != 0) {
        if (error) *error = "not a 32X ROM (missing 'SEGA 32X' header)";
        return false;
    }
    file_size = n;
    size_t cap = 0x80000;
    while (cap < n) cap <<= 1;
    if (cap < 0x400000) cap = 0x400000;
    data.assign(cap, 0xFF);
    std::memcpy(data.data(), bytes, n);
    // Mirror smaller images across the 4 MiB window.
    for (size_t off = n; off < 0x400000 && (n & (n - 1)) == 0; off += n)
        std::memcpy(data.data() + off, bytes, n);

    std::memcpy(domestic_name, bytes + 0x120, 48);
    header_checksum = read16(0x18E);
    std::memcpy(mars.module_name, bytes + 0x3C0, 16);
    mars.version = read32(0x3D0);
    mars.source = read32(0x3D4);
    mars.dest = read32(0x3D8);
    mars.size = read32(0x3DC);
    mars.master_entry = read32(0x3E0);
    mars.slave_entry = read32(0x3E4);
    mars.master_vbr = read32(0x3E8);
    mars.slave_vbr = read32(0x3EC);
    return true;
}

uint16_t Rom::compute_checksum() const {
    uint16_t sum = 0;
    for (size_t i = 0x200; i + 1 < file_size; i += 2) sum = uint16_t(sum + read16(uint32_t(i)));
    return sum;
}

} // namespace o32x
