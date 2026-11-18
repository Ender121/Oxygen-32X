// Reference SH-2 interpreter (validation + fallback for code not covered by
// the static recompiler).
#include "sh2_interp.h"
#include "sh2_ops.h"
#include <cstddef>
#include <cstring>

namespace sh2 {

void reset(State* c, uint32_t pc, uint32_t sp, uint32_t vbr) {
    Bus bus = c->bus;
    auto ack = c->irq_ack;
    auto user = c->irq_user;
    auto hook = c->hook;
    auto hook_user = c->hook_user;
    auto hook_pcs = c->hook_pcs;
    auto hook_count = c->hook_count;
    uint8_t id = c->id;
    *c = State{};
    c->bus = bus;
    c->irq_ack = ack;
    c->irq_user = user;
    c->hook = hook;
    c->hook_user = hook_user;
    c->hook_pcs = hook_pcs;
    c->hook_count = hook_count;
    c->id = id;
    c->pc = pc;
    c->r[15] = sp;
    c->vbr = vbr;
    c->sr = SR_MASK & 0xF0;  // interrupts masked
}

// The decoder is a pure function of (pc, opcode), so decoded instructions can
// be cached by address and checked against the opcode just fetched: code that
// changes (copied into SDRAM, overlays) simply misses.
namespace {
constexpr uint32_t kDecodeCacheSize = 4096;
struct DecodeCache {
    Insn e[kDecodeCacheSize];
    DecodeCache() { for (auto& i : e) i.pc = 1; }  // odd: never matches
};
DecodeCache g_decode_cache;

inline const Insn& decode_cached(uint32_t pc, uint16_t op) {
    Insn& e = g_decode_cache.e[(pc >> 1) & (kDecodeCacheSize - 1)];
    if (e.pc != pc || e.opcode != op) e = decode(pc, op);
    return e;
}
} // namespace

void exec_branch(State* c, const Insn& in) {
    uint32_t target = branch_target(c, in);
    c->cycles += branch_cycle_adjust(in, target);
    if (in.flags & IF_DELAYED) {
        uint32_t spc = in.pc + 2;
        const Insn& slot = decode_cached(spc, uint16_t(rd16(c, spc)));
        c->cycles -= slot.cycles;
        exec_simple(c, slot);
    }
    c->pc = target;
}

// The loop is instantiated twice so that, without a hook installed, it is
// exactly the loop it was: the test is made once a block, not once an
// instruction.
template <bool kHooked>
static void run_block(State* c) {
    for (;;) {
        uint32_t pc = c->pc;
        if (kHooked && is_hook_pc(c, pc)) c->hook(c, pc);
        const Insn& in = decode_cached(pc, uint16_t(rd16(c, pc)));
        c->cycles -= in.cycles;
        if (in.flags & IF_BRANCH) {
            exec_branch(c, in);
            return;
        }
        c->pc = pc + 2;
        exec_simple(c, in);
        if (ends_block(in)) return;
    }
}

// A block that jumps back to its own start and leaves registers, memory and
// device state exactly as it found them makes no progress until something
// outside this CPU changes (which cannot happen while the CPU runs its time
// slice: the other CPUs and the devices run between slices). Running it
// again for the rest of the slice is then a no-op, so the remaining cycles
// are charged at once: the same result and the same cycle count as running
// every iteration (including the overshoot past the slice end).
void interp_block(State* c) {
    if (c->hook) { run_block<true>(c); return; }
    const uint32_t start = c->pc;
    const int32_t entry_cycles = c->cycles;
    run_block<false>(c);
    if (c->pc != start || c->cycles <= 0 || c->sleeping) return;
    // Countdown delay loop "dt Rn / bf self": the iterations that fit in the
    // slice and leave Rn non-zero are charged at once (same Rn, T, cycles).
    {
        const uint32_t op0 = rd16(c, start), op1 = rd16(c, start + 2);
        if ((op0 & 0xF0FF) == 0x4010 && (op1 & 0xFF00) == 0x8B00 &&
            start + 6 + uint32_t(int32_t(int8_t(op1 & 0xFF)) * 2) == start) {
            const uint32_t n = (op0 >> 8) & 15;
            const int32_t k = entry_cycles - c->cycles;
            const uint32_t v = c->r[n];  // after one full iteration, still looping
            if (k > 0 && v > 1) {
                const uint32_t by_cycles = uint32_t((c->cycles + k - 1) / k);
                const uint32_t m = by_cycles < v - 1 ? by_cycles : v - 1;
                c->r[n] = v - m;
                c->cycles -= int32_t(m) * k;
                return;
            }
        }
    }
    // Registers r[0..15], pc, pr, gbr, vbr, mach, macl, sr are contiguous.
    static_assert(offsetof(State, sr) == offsetof(State, r) + 22 * sizeof(uint32_t), "State layout");
    uint32_t snap[23];
    std::memcpy(snap, c->r, sizeof snap);
    const uint32_t w = c->wcount, u = c->unsafe_reads;
    const int32_t before = c->cycles;
    c->stable_floor = INT32_MIN;
    run_block<false>(c);
    if (c->wcount != w || c->unsafe_reads != u || c->sleeping) return;
    if (std::memcmp(snap, c->r, sizeof snap) != 0) return;
    const int32_t k = before - c->cycles;
    if (k <= 0 || c->cycles <= 0) return;
    int64_t n = (int64_t(c->cycles) + k - 1) / k;
    if (c->stable_floor != INT32_MIN) {  // a polled value changes at a known time: stop short of it
        const int64_t n_max = (int64_t(c->cycles) - int64_t(c->stable_floor) - 1) / k;
        if (n_max < n) n = n_max;
    }
    if (n < 1) return;
    c->cycles -= int32_t(n * k);
}

} // namespace sh2
