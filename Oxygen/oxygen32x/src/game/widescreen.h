// Widescreen: the Mega Drive planes are drawn `margin` pixels wider on each
// side (renderer.h RenderState::margin) and the game's tile streaming is told to
// keep that much more of the level loaded (hooks, runtime/patches.h).
#pragma once
#include "machine/machine.h"
#include "machine/renderer.h"

namespace o32x {

// margin: extra pixels on each side (a multiple of 16, 0 = the original screen).
// Call after install_recompiled_code, before running.
void install_widescreen(Machine& m, int margin);

// Sets what the picture needs to know about the game (where the level starts, so
// that what lies left of it stays empty). Call on the snapshot, before drawing it.
void widescreen_prepare_render(RenderState& rs);

} // namespace o32x
