// World of Illusion Starring Mickey Mouse and Donald Duck - PS Vita frontend.
//
// Uses the Mickey & Donald Collection's Mega Drive core and its World of Illusion
// enhancement module (widescreen patches, HUD re-anchoring, cheats), with a small
// Vita-specific shell: ROM auto-detection, padded 16:9 presentation, save states
// and an in-game pause menu. No game data is included; the user supplies the
// USA ROM (CRC32 921EBD1C) or its Russian translation (CRC32 16DDF5CA).

#include <vita2d.h>
#include <psp2/ctrl.h>
#include <psp2/power.h>
#include <psp2/kernel/processmgr.h>

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "audio.h"
#include "session.h"
#include "settings.h"
#include "sys.h"
#include "romfind.h"
#include "../game/game.h"

namespace {

constexpr int kScreenW = 960;
constexpr int kScreenH = 544;
// A Mega Drive frame is at most 320 + 2 * 108 pixels wide and 224 lines high.
// World of Illusion tops out at 2:1 (448 x 224), but the texture is sized for
// every mode so a width change never has to recreate it.
constexpr int kTexW = 536;
constexpr int kTexH = 256;

constexpr uint32_t kWhite = RGBA8(255, 255, 255, 255);
constexpr uint32_t kDimWhite = RGBA8(190, 200, 215, 255);
constexpr uint32_t kAccent = RGBA8(105, 200, 255, 255);
constexpr uint32_t kPanel = RGBA8(10, 18, 34, 238);
constexpr uint32_t kSel = RGBA8(40, 95, 145, 255);
constexpr uint32_t kBlack = RGBA8(0, 0, 0, 255);

uint64_t now_us() { return (uint64_t)sceKernelGetSystemTimeWide(); }

struct PadState {
    uint32_t held = 0;
    uint32_t pressed = 0;
};

PadState polling_;

PadState poll_pad() {
    SceCtrlData d;
    memset(&d, 0, sizeof(d));
    d.lx = d.ly = 128;   // a failed read must not look like "left + up held"
    uint32_t h = 0;
    if (sceCtrlPeekBufferPositive(0, &d, 1) >= 0) {
        h = d.buttons;
        // Analogue stick as a d-pad (dead zone), for people who prefer it.
        const int dz = 40;
        if (d.lx < 128 - dz) h |= SCE_CTRL_LEFT;
        if (d.lx > 128 + dz) h |= SCE_CTRL_RIGHT;
        if (d.ly < 128 - dz) h |= SCE_CTRL_UP;
        if (d.ly > 128 + dz) h |= SCE_CTRL_DOWN;
    }
    PadState s;
    s.held = h;
    s.pressed = h & ~polling_.held;
    polling_.held = h;
    return s;
}

uint16_t md_buttons(uint32_t held) {
    uint16_t b = 0;
    if (held & SCE_CTRL_UP) b |= MD_UP;
    if (held & SCE_CTRL_DOWN) b |= MD_DOWN;
    if (held & SCE_CTRL_LEFT) b |= MD_LEFT;
    if (held & SCE_CTRL_RIGHT) b |= MD_RIGHT;
    if (held & SCE_CTRL_SQUARE) b |= MD_B;                       // attack (cape / magic)
    if (held & SCE_CTRL_CROSS) b |= MD_C;                        // jump
    if (held & SCE_CTRL_TRIANGLE) b |= MD_A;
    if (held & SCE_CTRL_CIRCLE) b |= MD_C;                       // jump as well
    if (held & SCE_CTRL_START) b |= MD_START;                    // the game's own pause / start
    return b;
}

class App {
public:
    Settings settings;
    std::unique_ptr<GameModule> module;
    std::unique_ptr<Session> session;
    Audio audio;
    vita2d_pgf* pgf = nullptr;   // system font (used when available)
    vita2d_pvf* pvf = nullptr;   // fallback if the PGF font library fails
    // Three game textures: the display queue holds up to two frames, so by the time a
    // texture comes round again its scene has long finished. The GPU draws the previous
    // frame while the CPU emulates the next one.
    vita2d_texture* tex[3] = {nullptr, nullptr, nullptr};
    int tex_index = 0;
    int current_tex = 0;
    int frame_w = 320, frame_h = 224;
    bool running = false;
    bool paused = false;
    int menu_sel = 0;
    int cheat_sel = 0;
    bool cheat_page = false;
    std::string toast;
    uint64_t toast_until = 0;
    FILE* log = nullptr;

    // performance counters
    uint64_t perf_start = 0;
    int perf_frames = 0;
    double perf_work = 0, perf_max = 0;
    double perf_qmin = 1e9;
    double perf_run = 0, perf_conv = 0, perf_pres = 0, perf_aud = 0;
    double perf_base = 0, perf_adj = 0;

    // The emulator generates sound per emulated frame. When a frame takes longer than
    // the Mega Drive's 16.69 ms (the console runs in slow motion), the resampler has to
    // stretch that sound over the real time the frame took, or the queue drains and the
    // output crackles. Track the real frame interval and derive the base resample ratio
    // from it; the queue fill level only trims the last fraction of a percent.
    double frame_dt = 1.0 / 59.9227;   // smoothed real seconds per emulated frame
    uint64_t last_frame_us = 0;
    double audio_base = 1.0;

    void logf(const char* fmt, ...) {
        if (!log) return;
        va_list ap;
        va_start(ap, fmt);
        vfprintf(log, fmt, ap);
        va_end(ap);
        fputc('\n', log);
        fflush(log);
    }

    void open_log() {
        make_dir(data_dir());
        log = open_file(path_join(data_dir(), L"log.txt"), "w");
    }

    void toast_now(const std::string& s, double seconds = 1.6) {
        toast = s;
        toast_until = now_us() + (uint64_t)(seconds * 1000000.0);
    }

    std::wstring save_path() const { return path_join(data_dir(), L"saves/woi.state"); }
    std::wstring settings_path() const { return path_join(data_dir(), L"settings.ini"); }

    void save_settings() {
        if (settings.save(settings_path())) logf("settings saved");
        else logf("settings save failed");
    }

    // World of Illusion tops out at 2:1 (64 px per side); offer no wider mode than that.
    WideMode max_wide_mode() const {
        const int limit = module ? module->max_ext() : 0;
        WideMode best = WideMode::Off;
        for (int m = 0; m < (int)WideMode::Count; m++)
            if (wide_ext((WideMode)m) <= limit && wide_ext((WideMode)m) >= wide_ext(best)) best = (WideMode)m;
        return best;
    }

    bool init() {
        open_log();
        logf("World of Illusion Vita starting");

        make_dir(data_dir());
        make_dir(path_join(data_dir(), L"saves"));
        make_dir(path_join(data_dir(), L"roms"));

        module.reset(create_woi_module());
        if (!module) { logf("create_woi_module failed"); return false; }

        // Find an accepted ROM anywhere in the usual places. The port never ships it.
        std::vector<RomSpec> specs = {{module->rom_crc32(), module->rom_size(), module->rom_names()}};
        if (module->rom_crc32_alt())
            specs.push_back({module->rom_crc32_alt(), module->rom_size(), module->rom_names()});
        std::vector<RomMatch> found(specs.size());
        find_roms(specs, found);
        int vi = -1;
        for (size_t i = 0; i < found.size(); i++)
            if (found[i].found) { vi = (int)i; break; }
        if (vi < 0) {
            rom_error = std::string("World of Illusion ROM not found.");
            rom_error += "\n\nPut the ROM file here:\nux0:data/MickeyAndDonald/roms/";
            rom_error += "\n\nAccepted dumps:\nWorld of Illusion (USA) [!].bin";
            rom_error += "\n1 MB, CRC32 921EBD1C";
            rom_error += "\nor the Russian translation of the EU release";
            rom_error += "\n1 MB, CRC32 16DDF5CA";
            std::wstring other;
            std::string other_desc;
            for (const RomMatch& m : found)
                if (!m.other_file.empty()) { other = m.other_file; other_desc = m.other_desc; break; }
            if (!other.empty()) {
                rom_error += "\n\nFound instead: " + to_utf8(file_name(other));
                if (!other_desc.empty()) rom_error += "\n(" + other_desc + ")";
            }
            logf("ROM not found (%s)", rom_error.c_str());
            running = true;   // keep the error screen up until the user exits
            return true;
        }
        if (vi > 0) module->set_rom_variant(vi);
        logf("ROM: %s", to_utf8(found[vi].path).c_str());

        session.reset(new Session);
        // Whatever the SDL device actually opened at: the mixer must match it.
        session->set_audio_rate(audio.sample_rate() > 0 ? audio.sample_rate() : 48000);
        if (!session->open(module.get(), found[vi].data)) {
            rom_error = "The ROM could not be loaded.";
            logf("%s", rom_error.c_str());
            running = true;
            return true;
        }

        // The Vita shell does not implement achievements, so leave them off:
        // that also enables the ROM's cheats (see the pause menu).
        settings.achievements = false;
        if (!settings.load(settings_path())) {
            settings.widescreen = WideMode::W16x9;
            settings.color = ColorMode::Accurate;
            settings.hud_anchor = true;
            settings.no_sprite_limit = true;
        }
        settings.achievements = false;
        settings.fullscreen = true;
        settings.vsync = true;
        settings.volume = std::clamp(settings.volume, 0, 100);
        if (settings.volume == 0) settings.volume = 80;
        if ((int)settings.widescreen > (int)max_wide_mode()) settings.widescreen = max_wide_mode();
        apply_settings();
        session->power_on();

        toast_now("X: jump    Square: attack    Select: menu", 4.0);
        running = true;
        return true;
    }

    void apply_settings() {
        if (session) session->apply_settings(settings);
    }

    void handle_menu(const PadState& in) {
        auto& s = settings;
        const bool up = (in.pressed & SCE_CTRL_UP) != 0;
        const bool down = (in.pressed & SCE_CTRL_DOWN) != 0;
        const bool left = (in.pressed & SCE_CTRL_LEFT) != 0;
        const bool right = (in.pressed & SCE_CTRL_RIGHT) != 0;
        const bool ok = (in.pressed & SCE_CTRL_CROSS) != 0;
        const bool back = (in.pressed & (SCE_CTRL_CIRCLE | SCE_CTRL_SELECT)) != 0;

        if (cheat_page) {
            const int n = 5;
            if (up) cheat_sel = (cheat_sel + n - 1) % n;
            if (down) cheat_sel = (cheat_sel + 1) % n;
            if (back) { cheat_page = false; menu_sel = 5; return; }
            if (ok) {
                if (cheat_sel == 4) { cheat_page = false; menu_sel = 5; return; }
                static const char* keys[4] = {"invincible", "inf_health", "inf_lives", "max_hearts"};
                auto& c = s.cheats[module->id()];
                if (cheat_sel >= 0 && cheat_sel < 4) {
                    const char* k = keys[cheat_sel];
                    c[k] = !c[k];
                    apply_settings();
                    save_settings();
                }
            }
            return;
        }

        const int n = 7;
        if (up) menu_sel = (menu_sel + n - 1) % n;
        if (down) menu_sel = (menu_sel + 1) % n;

        if (left || right) {
            if (menu_sel == 4) {   // widescreen
                const int top = (int)max_wide_mode();
                int w = (int)s.widescreen;
                if (w > top) w = top;
                w = left ? (w + top + 1 - 1) % (top + 1) : (w + 1) % (top + 1);
                s.widescreen = (WideMode)w;
                apply_settings();
                save_settings();
                toast_now(std::string("Widescreen: ") + wide_mode_name(s.widescreen));
            }
        }

        if (back) { paused = false; audio.set_paused(false); return; }
        if (!ok) return;

        switch (menu_sel) {
        case 0:   // resume
            paused = false;
            audio.set_paused(false);
            break;
        case 1: {   // save state
            std::vector<uint8_t> st = session->save_state();
            bool okw = !st.empty() && write_file(save_path(), st.data(), st.size());
            toast_now(okw ? "State saved" : "Could not save the state", 2.0);
            logf("save state: %d bytes, %s", (int)st.size(), okw ? "ok" : "failed");
            break;
        }
        case 2: {   // load state
            std::vector<uint8_t> st;
            if (!read_file(save_path(), st) || st.empty()) {
                toast_now("No saved state", 2.0);
            } else if (!session->load_state(st)) {
                toast_now("State is not for this game", 2.0);
            } else {
                apply_settings();
                toast_now("State loaded", 2.0);
            }
            break;
        }
        case 3:   // reset
            session->power_on();
            apply_settings();
            toast_now("Game reset");
            break;
        case 4:   // widescreen (also changed with left/right)
            s.widescreen = (WideMode)(((int)s.widescreen + 1) % ((int)max_wide_mode() + 1));
            apply_settings();
            save_settings();
            toast_now(std::string("Widescreen: ") + wide_mode_name(s.widescreen));
            break;
        case 5:   // cheats
            cheat_page = true;
            cheat_sel = 0;
            break;
        case 6:   // exit
            save_settings();
            running = false;
            break;
        default: break;
        }
    }

    void draw_text(int x, int y, float scale, uint32_t color, const char* text) {
        if (pgf) vita2d_pgf_draw_text(pgf, x, y, color, scale, text);
        else if (pvf) vita2d_pvf_draw_text(pvf, x, y, color, scale, text);
    }

    void draw_text_ls(int x, int y, float linespace, float scale, uint32_t color, const char* text) {
        if (pgf) vita2d_pgf_draw_text_ls(pgf, x, y, linespace, color, scale, text);
        else if (pvf) vita2d_pvf_draw_text_ls(pvf, x, y, linespace, color, scale, text);
    }

    void draw_game() {
        const float sx = std::min((float)kScreenW / frame_w, (float)kScreenH / frame_h);
        const float dw = frame_w * sx, dh = frame_h * sx;
        const float x = std::floor((kScreenW - dw) * 0.5f);
        const float y = std::floor((kScreenH - dh) * 0.5f);
        vita2d_draw_texture_part_scale(tex[current_tex], x, y, 0, 0, (float)frame_w, (float)frame_h, sx, sx);
    }

    void draw_menu() {
        vita2d_draw_rectangle(0, 0, kScreenW, kScreenH, RGBA8(0, 0, 0, 170));
        vita2d_draw_rectangle(110, 42, 740, 460, kPanel);

        if (cheat_page) {
            draw_text(150, 70, 1.45f, kAccent, "CHEATS");
            draw_text(150, 108, 0.72f, kDimWhite, "Achievements are off: cheats can be used.");

            static const char* names[4] = {"Invincibility", "Infinite Hearts", "Infinite Tries", "Maximum Hearts"};
            static const char* keys[4] = {"invincible", "inf_health", "inf_lives", "max_hearts"};
            const auto& c = settings.cheats[module->id()];
            for (int i = 0; i < 5; i++) {
                const float ry = 142.0f + i * 52.0f;
                const bool sel = i == cheat_sel;
                if (sel) vita2d_draw_rectangle(140, ry, 680, 42, kSel);
                std::string label = i == 4 ? "Back" : names[i];
                bool on = false;
                if (i < 4) { auto it = c.find(keys[i]); on = it != c.end() && it->second; }
                if (i < 4) label = (on ? "[X]  " : "[  ]  ") + label;
                draw_text(170, (int)ry + 29, 1.0f, sel ? kWhite : kDimWhite, label.c_str());
            }
            draw_text(150, 458, 0.72f, kDimWhite, "X: toggle    O: back");
            return;
        }

        draw_text(150, 68, 1.5f, kAccent, "PAUSED");
        draw_text(150, 104, 0.72f, kDimWhite, "World of Illusion - PS Vita");

        char wide[96];
        snprintf(wide, sizeof(wide), "Widescreen: %s   < >", wide_mode_name(settings.widescreen));
        const char* items[7] = {"Resume", "Save State", "Load State", "Reset Game", wide, "Cheats", "Exit"};
        for (int i = 0; i < 7; i++) {
            const float ry = 140.0f + i * 46.0f;
            const bool sel = i == menu_sel;
            if (sel) vita2d_draw_rectangle(140, ry, 680, 38, kSel);
            draw_text(170, (int)ry + 28, 0.95f, sel ? kWhite : kDimWhite, items[i]);
        }
        draw_text(150, 492, 0.72f, kDimWhite, "X: select    O: resume    Select: menu");
    }

    void draw_toast() {
        if (toast.empty() || now_us() > toast_until) return;
        const float scale = 0.85f;
        int tw = 0;
        if (pgf) tw = vita2d_pgf_text_width(pgf, scale, toast.c_str());
        else if (pvf) tw = vita2d_pvf_text_width(pvf, scale, toast.c_str());
        const float w = std::min((float)kScreenW - 40.0f, (float)tw + 60.0f);
        const float x = (kScreenW - w) * 0.5f;
        vita2d_draw_rectangle(x, kScreenH - 80, w, 52, RGBA8(0, 0, 0, 200));
        draw_text((int)((kScreenW - tw) * 0.5f), kScreenH - 44, scale, kWhite, toast.c_str());
    }

    void present() {
        vita2d_start_drawing();
        vita2d_clear_screen();
        if (session) draw_game();
        if (paused) draw_menu();
        draw_toast();
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

    void run() {
        perf_start = now_us();
        while (running) {
            const PadState in = poll_pad();

            if (rom_error.empty() && (in.pressed & SCE_CTRL_SELECT)) {
                paused = !paused;
                if (paused) {
                    menu_sel = 0;
                    cheat_page = false;
                    audio.set_paused(true);
                } else {
                    audio.set_paused(false);
                }
                // the time spent in the menu is not a frame-rate sample
                perf_start = now_us();
                perf_frames = 0;
                perf_work = perf_max = 0;
                perf_qmin = 1e9;
                perf_run = perf_conv = perf_pres = perf_aud = 0;
                perf_base = perf_adj = 0;
                last_frame_us = 0;
                present();
                continue;
            }

            if (paused) {
                handle_menu(in);
                present();
                continue;
            }

            if (!rom_error.empty()) {
                // Static error screen: draw the instructions, exit on X / Start.
                vita2d_start_drawing();
                vita2d_clear_screen();
                draw_text(60, 84, 1.35f, kAccent, "World of Illusion - ROM not found");
                draw_text_ls(60, 118, 8, 0.8f, kWhite, rom_error.c_str());
                draw_text(60, 516, 0.8f, kDimWhite, "Press X to exit.");
                vita2d_end_drawing();
                vita2d_swap_buffers();
                if (in.pressed & (SCE_CTRL_CROSS | SCE_CTRL_START)) running = false;
                continue;
            }

            const uint64_t frame_t0 = now_us();
            if (last_frame_us != 0) {
                const double dt = (frame_t0 - last_frame_us) * 1e-6;
                // ignore menu/loading gaps: only the steady frame cadence should move the ratio
                if (dt > 0.0 && dt < 0.25) {
                    const double k = std::min(1.0, dt / 0.35);   // ~0.35 s time constant
                    frame_dt += (dt - frame_dt) * k;
                }
            }
            last_frame_us = frame_t0;

            session->run_frame(md_buttons(in.held), 0);
            const uint64_t run_done = now_us();

            // Audio immediately after emulation: feed the queue before rendering so a
            // slow frame cannot starve it.
            audio_buf.clear();
            const double rate = audio.sample_rate() > 0 ? audio.sample_rate() : 48000.0;
            const MegaDrive& md = session->md();
            const double emu_dt = md.audio_frames > 0 ? md.audio_frames / md.sample_rate() : 1.0 / 59.9227;
            audio_base = std::clamp(frame_dt / emu_dt, 1.0, 2.0);
            const double target = rate * 0.1;   // ~100 ms cushion
            const double queued = (double)audio.queued_frames();
            const double trim = std::clamp((target - queued) / target, -1.0, 1.0) * 0.02;
            const double adjust = std::clamp(audio_base + trim, 1.0, 2.0);
            session->mix_audio(audio_buf, adjust);
            if (!audio_buf.empty()) audio.push(audio_buf.data(), (int)(audio_buf.size() / 2));
            const uint64_t aud_done = now_us();

            // The scene that used this texture was submitted two frames ago: the GPU has
            // had all of the last frame's emulation time to finish it, so this rarely waits.
            vita2d_wait_rendering_done();
            uint32_t* dst = (uint32_t*)vita2d_texture_get_datap(tex[tex_index]);
            const int stride = (int)(vita2d_texture_get_stride(tex[tex_index]) / 4);
            int w = 0, h = 0;
            if (dst && stride > 0) {
                session->frame_into(dst, stride, kTexW, w, h);
            } else {
                session->frame(w, h);
            }
            frame_w = w;
            frame_h = h;
            const uint64_t conv_done = now_us();

            current_tex = tex_index;
            present();
            tex_index = (tex_index + 1) % 3;
            const uint64_t pres_done = now_us();

            const uint64_t done = pres_done;

            perf_frames++;
            const double qms = audio.queued_frames() * 1000.0 / rate;
            if (qms < perf_qmin) perf_qmin = qms;
            perf_work += (done - frame_t0) / 1e6;
            perf_max = std::max(perf_max, (done - frame_t0) / 1e6);
            perf_run += (run_done - frame_t0) / 1e6;
            perf_aud += (aud_done - run_done) / 1e6;
            perf_conv += (conv_done - aud_done) / 1e6;
            perf_pres += (pres_done - conv_done) / 1e6;
            perf_base += audio_base;
            perf_adj += adjust;
            if (done - perf_start >= 5000000ull) {
                const double secs = (done - perf_start) / 1e6;
                logf("%.1f fps, work %.2f ms avg / %.2f ms max (run %.2f conv %.2f pres %.2f aud %.2f), audio base %.3f adj %.3f, queue min %.1f ms",
                     perf_frames / secs,
                     perf_work / perf_frames * 1000.0, perf_max * 1000.0,
                     perf_run / perf_frames * 1000.0, perf_conv / perf_frames * 1000.0,
                     perf_pres / perf_frames * 1000.0, perf_aud / perf_frames * 1000.0,
                     perf_base / perf_frames, perf_adj / perf_frames, perf_qmin);
                perf_start = done;
                perf_frames = 0;
                perf_work = perf_max = 0;
                perf_run = perf_conv = perf_pres = perf_aud = 0;
                perf_base = perf_adj = 0;
                perf_qmin = 1e9;
            }
        }
    }

    std::string rom_error;
    std::vector<int16_t> audio_buf;

    void shutdown() {
        if (session) save_settings();   // do not write a default file when the ROM was missing
        audio.shutdown();
        if (session) session.reset();
        module.reset();
        for (auto*& t : tex) if (t) { vita2d_free_texture(t); t = nullptr; }
        if (pgf) { vita2d_free_pgf(pgf); pgf = nullptr; }
        if (pvf) { vita2d_free_pvf(pvf); pvf = nullptr; }
        if (log) { fclose(log); log = nullptr; }
    }
};

}  // namespace

int main() {
    scePowerSetArmClockFrequency(444);
    scePowerSetGpuClockFrequency(222);
    scePowerSetBusClockFrequency(222);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);

    App app;
    if (vita2d_init() < 0) return 1;
    vita2d_set_clear_color(kBlack);
    vita2d_set_vblank_wait(1);
    // The system fonts: PGF is the one vita2d homebrew normally uses; PVF is a fallback.
    app.pgf = vita2d_load_default_pgf();
    if (!app.pgf) app.pvf = vita2d_load_default_pvf();

    app.tex[0] = vita2d_create_empty_texture_format(kTexW, kTexH, SCE_GXM_TEXTURE_FORMAT_A8R8G8B8);
    app.tex[1] = vita2d_create_empty_texture_format(kTexW, kTexH, SCE_GXM_TEXTURE_FORMAT_A8R8G8B8);
    app.tex[2] = vita2d_create_empty_texture_format(kTexW, kTexH, SCE_GXM_TEXTURE_FORMAT_A8R8G8B8);
    for (auto* t : app.tex)
        if (t) vita2d_texture_set_filters(t, SCE_GXM_TEXTURE_FILTER_LINEAR, SCE_GXM_TEXTURE_FILTER_LINEAR);

    app.audio.init(48000);
    app.init();
    app.run();
    app.shutdown();

    vita2d_fini();
    SDL_Quit();
    return 0;
}
