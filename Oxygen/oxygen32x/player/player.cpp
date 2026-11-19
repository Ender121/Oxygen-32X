// oxygen32x player: SDL2 front end (video, audio, keyboard / gamepad / touch).
// Same code for desktop and Android. Usage:
//   oxygen32x_player <rom.32x> [--frames N] [--shot file.bmp] [--press MASK]
// On Android the ROM is read from <internal storage>/rom.32x.
#include "machine/machine.h"
#include "game/recomp_dispatch.h"
#include "game/widescreen.h"
#include "machine/renderer.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

using namespace o32x;

namespace {

struct Layout {
    SDL_Rect game;
    float r;                 // button radius
    float dpad_x, dpad_y, dpad_r;
    float bx[3], by[3];      // A, B, C
    float start_x, start_y;
    bool touch_visible;
};

Layout make_layout(int w, int h, int fw) {
    Layout L{};
    const bool portrait = h > w;
    if (portrait) {
        const int gh = w * 224 / fw;
        L.game = {0, 0, w, gh};
        const float area_top = float(gh), area_h = float(h - gh);
        L.r = std::min(w * 0.08f, area_h * 0.12f);
        L.dpad_r = L.r * 2.3f;
        L.dpad_x = L.dpad_r + w * 0.04f;
        L.dpad_y = area_top + area_h * 0.5f;
        L.bx[0] = w - L.r * 5.4f; L.by[0] = L.dpad_y + L.r * 1.0f;
        L.bx[1] = w - L.r * 3.2f; L.by[1] = L.dpad_y - L.r * 0.2f;
        L.bx[2] = w - L.r * 1.1f; L.by[2] = L.dpad_y - L.r * 1.4f;
        L.start_x = w * 0.5f;
        L.start_y = area_top + area_h * 0.9f;
    } else {
        int gw = h * fw / 224;
        if (gw > w) gw = w;
        L.game = {(w - gw) / 2, (h - gw * 224 / fw) / 2, gw, gw * 224 / fw};
        L.r = h * 0.075f;
        L.dpad_r = L.r * 2.3f;
        L.dpad_x = L.dpad_r + w * 0.02f;
        L.dpad_y = h - L.dpad_r - h * 0.04f;
        L.bx[0] = w - L.r * 5.4f; L.by[0] = h - L.r * 1.8f;
        L.bx[1] = w - L.r * 3.2f; L.by[1] = h - L.r * 3.0f;
        L.bx[2] = w - L.r * 1.1f; L.by[2] = h - L.r * 4.2f;
        L.start_x = w * 0.5f;
        L.start_y = h - L.r * 0.8f;
    }
    L.touch_visible = true;
    return L;
}

uint16_t pad_from_touch(const Layout& L, const std::map<SDL_FingerID, SDL_FPoint>& fingers, int w, int h) {
    uint16_t pad = 0;
    for (auto& kv : fingers) {
        const float x = kv.second.x * w, y = kv.second.y * h;
        // D-pad (generous radius so thumbs can slide)
        const float dx = x - L.dpad_x, dy = y - L.dpad_y;
        if (dx * dx + dy * dy < L.dpad_r * L.dpad_r * 1.5f) {
            const float dead = L.dpad_r * 0.25f;
            if (dx < -dead) pad |= PAD_LEFT;
            if (dx > dead) pad |= PAD_RIGHT;
            if (dy < -dead) pad |= PAD_UP;
            if (dy > dead) pad |= PAD_DOWN;
            continue;
        }
        static const uint16_t bt[3] = {PAD_A, PAD_B, PAD_C};
        for (int i = 0; i < 3; ++i) {
            const float bx = x - L.bx[i], by = y - L.by[i];
            if (bx * bx + by * by < L.r * L.r * 1.7f) pad |= bt[i];
        }
        const float sx = x - L.start_x, sy = y - L.start_y;
        if (std::fabs(sx) < L.r * 1.6f && std::fabs(sy) < L.r * 0.9f) pad |= PAD_START;
    }
    return pad;
}

void fill_circle(SDL_Renderer* r, float cx, float cy, float rad) {
    for (int y = int(-rad); y <= int(rad); ++y) {
        const int half = int(std::sqrt(std::max(0.f, rad * rad - float(y * y))));
        SDL_RenderDrawLine(r, int(cx) - half, int(cy) + y, int(cx) + half, int(cy) + y);
    }
}

void draw_controls(SDL_Renderer* r, const Layout& L, uint16_t pad) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    auto col = [&](bool on) { SDL_SetRenderDrawColor(r, on ? 255 : 255, on ? 255 : 255, on ? 255 : 255, on ? 120 : 55); };
    col(false);
    fill_circle(r, L.dpad_x, L.dpad_y, L.dpad_r);
    // direction marks
    const float a = L.dpad_r * 0.55f, t = L.dpad_r * 0.22f;
    struct M { uint16_t b; float x, y; } marks[4] = {{PAD_UP, 0, -a}, {PAD_DOWN, 0, a}, {PAD_LEFT, -a, 0}, {PAD_RIGHT, a, 0}};
    for (auto& m : marks) { col(pad & m.b); fill_circle(r, L.dpad_x + m.x, L.dpad_y + m.y, t); }
    static const uint16_t bt[3] = {PAD_A, PAD_B, PAD_C};
    for (int i = 0; i < 3; ++i) { col(pad & bt[i]); fill_circle(r, L.bx[i], L.by[i], L.r); }
    col(pad & PAD_START);
    SDL_Rect s{int(L.start_x - L.r * 1.4f), int(L.start_y - L.r * 0.5f), int(L.r * 2.8f), int(L.r)};
    SDL_RenderFillRect(r, &s);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

// Tiny 3x5 font for the fps counter: digits, 'F', 'P', 'S', ' ', '.'
void draw_text(SDL_Renderer* r, int x, int y, int scale, const std::string& text) {
    static const char* glyph[] = {
        "111101101101111", "010110010010111", "111001111100111", "111001111001111", "101101111001001",
        "111100111001111", "111100111101111", "111001010010010", "111101111101111", "111101111001111",
    };
    static const char* gF = "111100110100100"; static const char* gP = "111101111100100";
    static const char* gS = "111100111001111"; static const char* gDot = "000000000000010";
    for (char ch : text) {
        const char* g = nullptr;
        if (ch >= '0' && ch <= '9') g = glyph[ch - '0'];
        else if (ch == 'F') g = gF; else if (ch == 'P') g = gP; else if (ch == 'S') g = gS; else if (ch == '.') g = gDot;
        if (g) for (int i = 0; i < 15; ++i) if (g[i] == '1') {
            SDL_Rect q{x + (i % 3) * scale, y + (i / 3) * scale, scale, scale};
            SDL_RenderFillRect(r, &q);
        }
        x += 4 * scale;
    }
}

uint16_t pad_from_keyboard() {
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    uint16_t p = 0;
    if (k[SDL_SCANCODE_UP]) p |= PAD_UP;
    if (k[SDL_SCANCODE_DOWN]) p |= PAD_DOWN;
    if (k[SDL_SCANCODE_LEFT]) p |= PAD_LEFT;
    if (k[SDL_SCANCODE_RIGHT]) p |= PAD_RIGHT;
    if (k[SDL_SCANCODE_Z]) p |= PAD_A;
    if (k[SDL_SCANCODE_X]) p |= PAD_B;
    if (k[SDL_SCANCODE_C]) p |= PAD_C;
    if (k[SDL_SCANCODE_A]) p |= PAD_X;
    if (k[SDL_SCANCODE_S]) p |= PAD_Y;
    if (k[SDL_SCANCODE_D]) p |= PAD_Z;
    if (k[SDL_SCANCODE_RETURN]) p |= PAD_START;
    if (k[SDL_SCANCODE_TAB]) p |= PAD_MODE;
    return p;
}

uint16_t pad_from_controller(SDL_GameController* c) {
    if (!c) return 0;
    auto b = [&](SDL_GameControllerButton x) { return SDL_GameControllerGetButton(c, x) != 0; };
    uint16_t p = 0;
    if (b(SDL_CONTROLLER_BUTTON_DPAD_UP)) p |= PAD_UP;
    if (b(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) p |= PAD_DOWN;
    if (b(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) p |= PAD_LEFT;
    if (b(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) p |= PAD_RIGHT;
    if (b(SDL_CONTROLLER_BUTTON_A)) p |= PAD_B;   // south = B (jump in most MD games)
    if (b(SDL_CONTROLLER_BUTTON_B)) p |= PAD_C;
    if (b(SDL_CONTROLLER_BUTTON_X)) p |= PAD_A;
    if (b(SDL_CONTROLLER_BUTTON_Y)) p |= PAD_Y;
    if (b(SDL_CONTROLLER_BUTTON_START)) p |= PAD_START;
    if (b(SDL_CONTROLLER_BUTTON_BACK)) p |= PAD_MODE;
    const int lx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
    const int ly = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
    if (lx < -12000) p |= PAD_LEFT;
    if (lx > 12000) p |= PAD_RIGHT;
    if (ly < -12000) p |= PAD_UP;
    if (ly > 12000) p |= PAD_DOWN;
    return p;
}

} // namespace

int main(int argc, char** argv) {
    std::string rom_path, shot_path;
    int max_frames = -1;
    uint16_t forced_pad = 0;
    bool force_touch_ui = false;
#ifdef __ANDROID__
    bool show_fps = true;   // on until the speed on real devices is known
#else
    bool show_fps = false;
#endif
    int win_w = 960, win_h = 672;
#ifdef __ANDROID__
    int widescreen = 48;  // phones are ~16:9: 416x224 fills the screen (--widescreen 0 for the original 320)
#else
    int widescreen = 0;   // extra Mega Drive pixels on each side of the 320 pixel picture
#endif
    std::string win_shot;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--frames" && i + 1 < argc) max_frames = std::atoi(argv[++i]);
        else if (a == "--shot" && i + 1 < argc) shot_path = argv[++i];
        else if (a == "--press" && i + 1 < argc) forced_pad = uint16_t(std::strtol(argv[++i], nullptr, 0));
        else if (a == "--fps") show_fps = true;
        else if (a == "--widescreen" && i + 1 < argc) widescreen = std::atoi(argv[++i]);
        else if (a == "--touch-ui") force_touch_ui = true;
        else if (a == "--size" && i + 1 < argc) std::sscanf(argv[++i], "%dx%d", &win_w, &win_h);
        else if (a == "--window-shot" && i + 1 < argc) win_shot = argv[++i];
        else rom_path = a;
    }
#ifdef __ANDROID__
    if (rom_path.empty()) {
        const char* dir = SDL_AndroidGetInternalStoragePath();
        rom_path = std::string(dir ? dir : ".") + "/rom.32x";
    }
#endif
    if (rom_path.empty()) { std::fprintf(stderr, "usage: %s rom.32x [--frames N] [--shot out.bmp]\n", argv[0]); return 2; }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    auto machine = std::make_unique<Machine>();
    std::string err;
    if (!machine->load_rom(rom_path, &err)) {
        SDL_Log("ROM error: %s (%s)", err.c_str(), rom_path.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "oxygen32x", err.c_str(), nullptr);
        SDL_Quit();
        return 1;
    }
    machine->reset();
    { auto rs = install_recompiled_code(*machine, !std::getenv("O32X_NO_RECOMP")); SDL_Log("exec: %s", rs.description.c_str()); install_widescreen(*machine, widescreen); }
    machine->audio_enabled = true;

    SDL_Window* win = SDL_CreateWindow("Oxygen-32X", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h,
                                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) { std::fprintf(stderr, "window: %s\n", SDL_GetError()); return 1; }
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { std::fprintf(stderr, "renderer: %s\n", SDL_GetError()); return 1; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kFrameMaxWidth, kFrameMaxHeight);

    // Audio: the machine makes interleaved stereo S16 at ~53.27 kHz; SDL resamples.
    SDL_AudioSpec want{}, have{};
    want.freq = 48000; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024;
    SDL_AudioDeviceID adev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    SDL_AudioStream* stream = nullptr;
    if (adev) {
        stream = SDL_NewAudioStream(AUDIO_S16SYS, 2, int(kAudioRate), have.format, have.channels, have.freq);
        SDL_PauseAudioDevice(adev, 0);
    }
    const int bytes_per_sec = adev ? have.freq * have.channels * int(SDL_AUDIO_BITSIZE(have.format) / 8) : 0;

    std::vector<SDL_GameController*> pads;
    std::map<SDL_FingerID, SDL_FPoint> fingers;
    bool touch_used = false;
    auto frame = std::make_unique<Frame>();
    // The picture is drawn on a worker thread from a copy of the machine state
    // (renderer.h RenderState), overlapping the next emulated frame.
    // job: 0 idle, 1 requested, 2 done (the frame is ready to upload), 3 quit.
    auto rstate = std::make_unique<RenderState>();
    auto wframe = std::make_unique<Frame>();
    std::atomic<int> job{0};
    std::mutex jm;
    std::condition_variable jcv;
    std::atomic<double> worker_ms{0};
    std::thread worker([&] {
        for (;;) {
            {
                std::unique_lock<std::mutex> lk(jm);
                jcv.wait(lk, [&] { return job.load() == 1 || job.load() == 3; });
                if (job.load() == 3) return;
            }
            const Uint64 t = SDL_GetPerformanceCounter();
            render_state(*rstate, *wframe);
            worker_ms = double(SDL_GetPerformanceCounter() - t) * 1000.0 / double(SDL_GetPerformanceFrequency());
            std::lock_guard<std::mutex> lk(jm);
            job = 2;
        }
    });
    bool quit = false;
    int frames = 0;
    int fps_frames = 0;
    int skipped = 0;
    double fps_value = 0, ms_value = 0, rms_value = 0, pms_value = 0;
    double rend_ms_acc = 0, pres_ms_acc = 0;
    auto ms_since = [](Uint64 t) { return double(SDL_GetPerformanceCounter() - t) * 1000.0 / double(SDL_GetPerformanceFrequency()); };
    double run_ms_acc = 0;
    Uint64 fps_t0 = SDL_GetPerformanceCounter();
    Uint64 next_tick = SDL_GetPerformanceCounter();
    const double frame_sec = 1.0 / 59.922;

    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: quit = true; break;
            case SDL_CONTROLLERDEVICEADDED: {
                SDL_GameController* c = SDL_GameControllerOpen(e.cdevice.which);
                if (c) pads.push_back(c);
                break;
            }
            case SDL_CONTROLLERDEVICEREMOVED: {
                for (auto it = pads.begin(); it != pads.end();) {
                    if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(*it)) == e.cdevice.which) {
                        SDL_GameControllerClose(*it); it = pads.erase(it);
                    } else ++it;
                }
                break;
            }
            case SDL_FINGERDOWN: case SDL_FINGERMOTION:
                touch_used = true;
                fingers[e.tfinger.fingerId] = SDL_FPoint{e.tfinger.x, e.tfinger.y};
                break;
            case SDL_FINGERUP:
                fingers.erase(e.tfinger.fingerId);
                break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_AC_BACK) quit = true;
                if (e.key.keysym.sym == SDLK_F11) {
                    Uint32 f = SDL_GetWindowFlags(win);
                    SDL_SetWindowFullscreen(win, (f & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                }
                break;
            default: break;
            }
        }
        int ww, wh;
        SDL_GetRendererOutputSize(ren, &ww, &wh);
        const Layout L = make_layout(ww, wh, frame->width);
        uint16_t pad = pad_from_keyboard() | forced_pad;
        for (auto* c : pads) pad |= pad_from_controller(c);
        const uint16_t touch_pad = pad_from_touch(L, fingers, ww, wh);
        pad |= touch_pad;
        machine->input.pad[0] = pad;

        // Run one frame (audio-paced: keep ~3 frames of sound queued).
        bool run = true;
        if (stream && adev) {
            const int queued = int(SDL_GetQueuedAudioSize(adev));
            run = queued < bytes_per_sec / 60 * 3;
        }
        if (run) {
            const Uint64 t_a = SDL_GetPerformanceCounter();
            machine->run_frame();
            run_ms_acc += double(SDL_GetPerformanceCounter() - t_a) * 1000.0 / double(SDL_GetPerformanceFrequency());
            ++frames;
            ++fps_frames;
            if (stream) {
                auto& a = machine->audio_out;
                if (!a.empty()) {
                    SDL_AudioStreamPut(stream, a.data(), int(a.size() * sizeof(int16_t)));
                    a.clear();
                    uint8_t buf[16384];
                    int got;
                    while ((got = SDL_AudioStreamGet(stream, buf, sizeof buf)) > 0) SDL_QueueAudio(adev, buf, Uint32(got));
                }
            } else {
                machine->audio_out.clear();
            }
            // When the sound queue is running dry the machine is too slow: skip
            // drawing this picture (never more than two in a row) to catch up.
            const bool behind = adev && int(SDL_GetQueuedAudioSize(adev)) < bytes_per_sec / 60;
            if (behind && skipped < 2) {
                ++skipped;
            } else {
                skipped = 0;
                const Uint64 t_r = SDL_GetPerformanceCounter();
                // Hand the machine state to the worker if it is free (otherwise this
                // picture is dropped; emulation never waits for drawing).
                if (job.load() == 0) {
                    snapshot_render_state(*machine, *rstate);
                    widescreen_prepare_render(*rstate);
                    { std::lock_guard<std::mutex> lk(jm); job = 1; }
                    jcv.notify_one();
                }
                rend_ms_acc += ms_since(t_r);
            }
        }
        if (job.load() == 2) {
            const Uint64 t_u = SDL_GetPerformanceCounter();
            void* px; int pitch;
            if (SDL_LockTexture(tex, nullptr, &px, &pitch) == 0) {
                for (int y = 0; y < wframe->height; ++y)
                    std::memcpy(static_cast<uint8_t*>(px) + y * pitch, wframe->pixels + y * wframe->width, wframe->width * 4);
                SDL_UnlockTexture(tex);
            }
            frame->height = wframe->height;
            frame->width = wframe->width;
            job = 0;
            rend_ms_acc += ms_since(t_u);
        }
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_Rect src{0, 0, frame->width, frame->height};
        SDL_RenderCopy(ren, tex, &src, &L.game);
        {
            const Uint64 now = SDL_GetPerformanceCounter();
            const double dt = double(now - fps_t0) / double(SDL_GetPerformanceFrequency());
            if (dt >= 1.0) {
                fps_value = fps_frames / dt;
                ms_value = fps_frames ? run_ms_acc / fps_frames : 0;
                rms_value = fps_frames ? rend_ms_acc / fps_frames : 0;
                pms_value = fps_frames ? pres_ms_acc / fps_frames : 0;
                rend_ms_acc = pres_ms_acc = 0;
                fps_frames = 0; run_ms_acc = 0; fps_t0 = now;
            }
        }
        if (show_fps) {
            // "FPS 60.0" = frames emulated per second; second line: emulation ms per frame (budget 16.7).
            char b1[32], b2[32], b3[32], b4[32];
            std::snprintf(b1, sizeof b1, "FPS %.1f", fps_value);
            std::snprintf(b2, sizeof b2, "%.1f", ms_value);
            std::snprintf(b3, sizeof b3, "%.1f", rms_value);
            std::snprintf(b4, sizeof b4, "%.1f", pms_value);
            const int sc = std::max(2, ww / 160);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_Rect bg{4, 4, 4 * sc * 9, 7 * sc * 4 + 4};
            SDL_RenderFillRect(ren, &bg);
            SDL_SetRenderDrawColor(ren, fps_value >= 55 ? 80 : 255, fps_value >= 55 ? 255 : 80, 80, 255);
            draw_text(ren, 8, 6, sc, b1);
            draw_text(ren, 8, 6 + 6 * sc, sc, b2);
            draw_text(ren, 8, 6 + 12 * sc, sc, b3);
            draw_text(ren, 8, 6 + 18 * sc, sc, b4);
        }
        if (force_touch_ui || touch_used || SDL_GetNumTouchDevices() > 0) draw_controls(ren, L, touch_pad);
        if (!win_shot.empty() && max_frames >= 0 && frames >= max_frames) {
            SDL_Surface* ws = SDL_CreateRGBSurfaceWithFormat(0, ww, wh, 32, SDL_PIXELFORMAT_ARGB8888);
            SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ARGB8888, ws->pixels, ws->pitch);
            SDL_SaveBMP(ws, win_shot.c_str());
            SDL_FreeSurface(ws);
        }
        {
            const Uint64 t_p = SDL_GetPerformanceCounter();
            SDL_RenderPresent(ren);
            pres_ms_acc += ms_since(t_p);
        }

        if (!stream) {  // no audio device: pace by the clock
            next_tick += Uint64(frame_sec * double(SDL_GetPerformanceFrequency()));
            const Uint64 now = SDL_GetPerformanceCounter();
            if (next_tick > now) SDL_Delay(Uint32(double(next_tick - now) * 1000.0 / double(SDL_GetPerformanceFrequency())));
            else next_tick = now;
        } else if (!run) {
            SDL_Delay(1);
        }
        if (max_frames >= 0 && frames >= max_frames) quit = true;
    }
    { std::lock_guard<std::mutex> lk(jm); job = 3; }
    jcv.notify_one();
    worker.join();
    if (!shot_path.empty()) {
        snapshot_render_state(*machine, *rstate);
        widescreen_prepare_render(*rstate);
        render_state(*rstate, *frame);
        SDL_Surface* s = SDL_CreateRGBSurfaceFrom(frame->pixels, frame->width, frame->height, 32, frame->width * 4,
                                                  0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        SDL_SaveBMP(s, shot_path.c_str());
        SDL_FreeSurface(s);
    }
    for (auto* c : pads) SDL_GameControllerClose(c);
    if (stream) SDL_FreeAudioStream(stream);
    if (adev) SDL_CloseAudioDevice(adev);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
