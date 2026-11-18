// 32X cartridge image: loading and the MARS header. The ROM is never part of
// the repository; the user supplies it.
// Derived from ChaotixRecompiled (MIT), see LICENSE-ChaotixRecompiled.
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace o32x {

// The SH-2 image the 32X boot code copies into SDRAM, and where the two
// SH-2s start (header at cartridge 0x3C0).
struct MarsHeader {
    char module_name[17] = {};
    uint32_t version = 0;
    uint32_t source = 0;       // ROM offset of the SH-2 image
    uint32_t dest = 0;         // SDRAM offset
    uint32_t size = 0;
    uint32_t master_entry = 0;
    uint32_t slave_entry = 0;
    uint32_t master_vbr = 0;
    uint32_t slave_vbr = 0;
};

enum class RomVersion { Unknown, ChaotixJUE };

struct Rom {
    std::vector<uint8_t> data;   // big-endian image, padded to a power of two (at least 4 MiB)
    size_t file_size = 0;
    std::string sha1;
    RomVersion version = RomVersion::Unknown;  // ChaotixJUE: the image the recompiled code is made from
    // No run-time ROM patches here: the image is always the original, and
    // nothing is listed as changed.
    std::vector<std::pair<uint32_t, uint32_t>> patched;
    const std::vector<uint8_t>& unpatched() const { return data; }
    MarsHeader mars;
    uint16_t header_checksum = 0;
    char domestic_name[49] = {};

    bool load(const std::string& path, std::string* error);
    bool load_memory(const uint8_t* bytes, size_t n, std::string* error);
    uint16_t compute_checksum() const;  // Mega Drive style (sum of words from 0x200)
    uint32_t read32(uint32_t off) const {
        return (uint32_t(data[off]) << 24) | (uint32_t(data[off + 1]) << 16) | (uint32_t(data[off + 2]) << 8) | data[off + 3];
    }
    uint16_t read16(uint32_t off) const { return uint16_t((data[off] << 8) | data[off + 1]); }
};

std::string sha1_hex(const uint8_t* data, size_t n);

} // namespace o32x
