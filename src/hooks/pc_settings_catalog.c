/*
 * Every player-facing setting, and the settings file, with nothing of the running game.
 *
 * The game (pc_settings.c) and the app that sets it up and starts it (src/app) both register this
 * catalogue: the display and texture table here, the input tables (pc_input.c, whose defaults come
 * from the toolkit's input map), the launcher's and overlay's own (pc_ui_settings.c) and [debug]
 * (here). The settings library keeps lines it does not know, so an app and a game of different
 * versions can share one file without losing each other's values.
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>

#include "recomp_settings.h"
#include "pc_settings_catalog.h"
#include "pc_settings.h"
#include "pc_input.h"
#include "pc_ui.h"
#include "pc_debug.h"
#include "pc_language.h"
#include "pc_songs.h"
#include "patches/cheats.h"

/* Picture shape: the console's 4:3, stretched, or widescreen (docs/widescreen); the widescreen
 * choices are indices 2.. and are read once at start-up (src/patches/widescreen.c). */
static const char *const k_aspect[] = { "4:3", "stretch", "widescreen (auto)", "16:9", "16:10",
                                        "21:9 (experimental)", "32:9 (experimental)", NULL };
static const char *const k_filter[] = { "smooth", "sharp", "smooth (high quality)", NULL };
static const char *const k_aniso[] = { "off", "2x", "4x", "8x", "16x", NULL };
static const char *const k_msaa[] = { "off", "2x", "4x", "8x", NULL };
static const char *const k_ws_hud[] = { "auto", "edges", "centered", NULL };
static const char *const k_ws_cutscenes[] = { "authored", "fill", NULL };
static const char *const k_ws_fill[] = { "blur", "black", NULL };

/* [android] contatg's Android 1.4 defaults: 1x (480p) render scale, no vsync pacing, plus its
 * Android-only graphics/scheduler keys (read by the Android renderer and the launcher). */
#if defined(__ANDROID__)
#define RENDER_DEFAULT145 1
#define VSYNC_DEFAULT145 0
#else
#define RENDER_DEFAULT145 2
#define VSYNC_DEFAULT145 1
#endif
static const char *const k_android_profile[] = {"auto","low-end","normal",NULL};
static RecompSetting g_display_settings[] = {
#if defined(__ANDROID__)
    {"graphics","preset145",RECOMP_SETTING_INT,0,0,3,NULL,NULL,0,"In-game graphics preset:0 custom,1 performance,2 balanced,3 quality. Does not change simulation."},
    {"graphics","fxaa145",RECOMP_SETTING_INT,-1,-1,1,NULL,NULL,0,"-1 inherits previous Android FXAA choice;0 off,1 on. Live in-game graphics menu."},
    {"graphics","hud145",RECOMP_SETTING_INT,-1,-1,1,NULL,NULL,0,"-1 inherits previous Android FPS counter choice;0 off,1 on."},
    {"graphics","hud_position145",RECOMP_SETTING_INT,0,0,2,NULL,NULL,0,"0 top-right,1 top-center,2 top-left."},
    {"graphics","precise_barriers144",RECOMP_SETTING_BOOL,1,0,0,NULL,"RECOMP_VK_BARRIERS144",RECOMP_SETTING_RESTART,"Layout-specific Vulkan barriers; conservative GPU mode retains broad dependencies."},
    {"graphics","fsr144",RECOMP_SETTING_INT,0,0,1,NULL,"RECOMP_VK_FSR144",RECOMP_SETTING_RESTART,"AMD FSR1 EASU+RCAS presentation upscaling. Optional GPU cost; no frame generation."},
    {"graphics","screen_filter144",RECOMP_SETTING_INT,0,0,5,NULL,"RECOMP_VK_FILTER144",RECOMP_SETTING_RESTART,"0 original,1 sharpen,2 vivid,3 scanlines,4 CRT,5 tone mapping (SDR)."},
    {"graphics","spatial34",RECOMP_SETTING_INT,0,0,2,NULL,"RECOMP_VK_SPATIAL34",RECOMP_SETTING_RESTART,"Presentation upscaler: 0 original, 1 Catmull-Rom bicubic, 2 Lanczos-2; independent of guest. Restart required."},
    {"graphics","anisotropy144",RECOMP_SETTING_INT,1,1,16,NULL,"RECOMP_VK_ANISO144",RECOMP_SETTING_RESTART,"Optional linear mipmapped texture filtering, limited to device support."},
    {"graphics","shader_preload144",RECOMP_SETTING_INT,1,0,1,NULL,"RECOMP_VK_PRELOAD144",RECOMP_SETTING_RESTART,"Bounded startup warm-up of prior-run shaders/pipelines; first run learns."},
    {"graphics","fsr_sharpness144",RECOMP_SETTING_INT,80,0,200,NULL,"RECOMP_VK_SHARPNESS144",RECOMP_SETTING_RESTART,"RCAS attenuation in hundredths of a stop:0 strongest,200 softer."},
    {"vksched","profile",RECOMP_SETTING_ENUM,0,0,0,k_android_profile,NULL,RECOMP_SETTING_RESTART,"Presentation preset. No guest simulation changes."},
    {"vksched","present_limit",RECOMP_SETTING_INT,-1,-1,60,NULL,NULL,RECOMP_SETTING_RESTART,"-1 auto/launcher,0 original,30/60 host ceiling."},
    {"vksched","menus_limit",RECOMP_SETTING_INT,60,0,120,NULL,NULL,RECOMP_SETTING_RESTART,"Auto profile: menu cap. Movies keep their authored cadence."},
    {"vksched","exclude_prime34",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Weak-phone preset: guest and native threads avoid the fastest CPU where supported."},
    {"vksched","kernel_log26",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"0: disable xbox_log before timestamp/format/locking and do not create xbox_kernel.log;1 restore diagnostic logger;restart required."},
    {"graphics","fg_engine39",RECOMP_SETTING_INT,0,0,2,NULL,NULL,RECOMP_SETTING_RESTART,"0 off, 1 Win-FG v37, 2 Lossless LSFG. Combat only."},
    {"graphics","framegen34",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Lossless Scaling 3.1 Performance frame generation; presentation only."},
    {"graphics","framegen27",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Optional native LSFG3.1 Performance x2/x3/x4 from user-imported Lossless shaders. Default OFF; adds video latency; no extra simulation steps."},
    {"graphics","lsfg32_height",RECOMP_SETTING_INT,480,240,960,NULL,"DEFJAM_LSFG32_HEIGHT",RECOMP_SETTING_RESTART,"Private LSFG snapshot height limit; preserves game render resolution. Default480."},
    {"graphics","lsfg30_multiplier",RECOMP_SETTING_INT,2,2,4,NULL,"DEFJAM_LSFG30_MULTIPLIER",RECOMP_SETTING_RESTART,"LSFG generated presentation multiplier:2/3/4. GPU and display can limit effective ratio."},
    {"graphics","lsfg30_refresh",RECOMP_SETTING_INT,60,60,120,NULL,"DEFJAM_LSFG30_REFRESH",RECOMP_SETTING_RESTART,"LSFG output ceiling:60/90/120Hz, if supported by the display."},
    {"graphics","lsfg27_flow",RECOMP_SETTING_INT,25,25,100,NULL,NULL,RECOMP_SETTING_RESTART,"LSFG optical-flow resolution percentage;25 low-cost default,50/100 optional. Does not change game resolution."},
    {"graphics","lsfg27_fp16",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Request FP16 LSFG shaders only if the Turnip device supports the feature; FP32 fallback. Restart required."},
    {"graphics","lsfg27_gpu_budget_us",RECOMP_SETTING_INT,4000,2000,8000,NULL,NULL,RECOMP_SETTING_RESTART,"Auto-disable framegen after12 GPU EWMA samples above this budget in microseconds; default4000."},
    {"graphics","lsfg27_budget_guard",RECOMP_SETTING_BOOL,1,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Pause FG when GPU budget is exceeded; 0 disables the budget guard."},
    {"graphics","lsfg27_warmup_samples",RECOMP_SETTING_INT,60,0,600,NULL,NULL,RECOMP_SETTING_RESTART,"Generated-frame GPU samples ignored after startup; default 60."},
    {"graphics","lsfg27_budget_retry_s",RECOMP_SETTING_INT,45,0,600,NULL,NULL,RECOMP_SETTING_RESTART,"Seconds before retry after GPU budget pause; 0 makes it permanent."},
    {"graphics","lsfg33_slow_guard",RECOMP_SETTING_BOOL,1,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Suspend FG below 20 fps; 0 allows low-source generation down to about 10 fps."},
    {"graphics","framegen26",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Experimental Vulkan image-based optical flow, combat only,60Hz presentation; adds20-60ms buffering; default OFF. Real FPS/simulation unchanged."},
    {"vksched","power25",RECOMP_SETTING_BOOL,1,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Power management and bounded1Hz observations."},
    {"vksched","adpf25",RECOMP_SETTING_BOOL,1,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Request Android performance hints; APIs may be unavailable; original simulation unchanged."},
    {"vksched","power_log25",RECOMP_SETTING_BOOL,1,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Monitor-thread CPU,temperature,cooling,headroom1Hz; no per-frame proc reads."},
    {"vksched","sustained25",RECOMP_SETTING_BOOL,1,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Request sustained performance when the device supports it; OS selects clocks."},
    {"vksched","display_rate25",RECOMP_SETTING_BOOL,1,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Request60Hz for60fps;120Hz for40fps when compatible display modes exist."},
    {"vksched","affinity25",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Opt-in fixed native placement:render6,helpers4/5,mixer2or3; original allowed mask respected."},
    {"vksched","guest_worker_affinity",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Experimental guest workers5,IRQ4; timing not validated; restart required."},
    {"vksched","eco_resolution25",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Opt-in low-end boot at480p,aniso1,FSRoff,aspect preserved."},
    {"vksched","dynamic_resolution25",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Headroom>=.85 for5s requests480p active region; requires direct presentation; original allocation retained."},
    {"vksched","thermal_cap40_25",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Opt-in40fps after480p and persistent heat; existing cadence guard remains; cannot meet17ms p95 at40fps."},
    {"vksched","present_mode25",RECOMP_SETTING_INT,0,0,2,NULL,NULL,RECOMP_SETTING_RESTART,"0 preserve selection,1 FIFO,2 MAILBOX if available; device decides image count."},
    {"vksched","swap_images25",RECOMP_SETTING_INT,0,0,5,NULL,NULL,RECOMP_SETTING_RESTART,"0 preserve previous minimum,3 or5 requested images, clamped to driver capabilities."},
    {"vksched","mixer_cpu25",RECOMP_SETTING_INT,3,2,3,NULL,NULL,RECOMP_SETTING_RESTART,"Native placement only, no audio buffer/algorithm change."},
    {"vksched","adaptive_pack24",RECOMP_SETTING_BOOL,1,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Bypass repeatedly missing packed-vertex buckets; exact validation on cache hits; original gather on bypass."},
    {"vksched","program_index24",RECOMP_SETTING_BOOL,1,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Fixed exact-key program index; rebuild only on program creation/replacement."},
    {"vksched","fine_scopes22",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Exclusive wall scopes; instrumentation overhead included."},
    {"vksched","resources22",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Opt-in per-present /proc lane counters; measurable file IO overhead."},
    {"vksched","telemetry22",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"One-second hardware telemetry on existing monitor thread."},
    {"vksched","fifo22",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Force FIFO, request three images; restart, compare on device."},
    {"vksched","pin_render22",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Opt-in render CPU6, helpers CPU5, only if in original allowed mask."},
    {"vksched","vertex_min_batch",RECOMP_SETTING_INT,512,96,2048,NULL,NULL,RECOMP_SETTING_RESTART,"96/128/256/512/1024/2048. Default512; measure target-device cost."},
    {"vksched","vertex_participants",RECOMP_SETTING_INT,4,0,8,NULL,NULL,RECOMP_SETTING_RESTART,"0/4: dispatcher plus three helpers; 1 serial; 2..8 total participants. Sleeping condition-variable waits."},
    {"vksched","native_clock_hz140",RECOMP_SETTING_INT,60,60,60,NULL,NULL,RECOMP_SETTING_RESTART,"Legacy key retained: original guest clock only; native120 experiment removed."},
    {"vksched","thermal_adaptive",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Legacy ignored key. No application thermal ceiling or thermal worker."},
    {"vksched","vertex_cache",RECOMP_SETTING_BOOL,1,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Bounded exact-input packed vertex cache; verifies raw bytes on every hit."},
    {"vksched","audio_buffer_ms",RECOMP_SETTING_INT,60,43,85,NULL,NULL,RECOMP_SETTING_RESTART,"Bounded SDL audio preroll. More buffer adds output latency, not missing samples."},
    {"vksched","diagnostics",RECOMP_SETTING_BOOL,1,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Two-second frame windows; background reports every ten seconds."},
    {"vksched","path_cache",RECOMP_SETTING_BOOL,1,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Read-only disc path/miss cache; writable saves excluded. Restart after importing files."},
    {"vksched","hitch_details",RECOMP_SETTING_BOOL,1,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Bounded per-frame scope attribution; summaries every ten seconds."},
    {"vksched","guest_samples",RECOMP_SETTING_BOOL,0,0,0,NULL,NULL,RECOMP_SETTING_RESTART,"Opt-in 100Hz guest-thread CPU sampling. Profiling has overhead."},
#endif
    { "textures", "enabled", RECOMP_SETTING_BOOL, 0, 0, 0, NULL, NULL, RECOMP_SETTING_RESTART,
      "Use texture packs from the data folder's mods directory." },
    { "textures", "packs", RECOMP_SETTING_STRING, 0, 0, 0, NULL, "RECOMP_TEXTURE_PACKS", RECOMP_SETTING_RESTART,
      "Pack folder names separated by semicolons. Later packs take precedence.", "faithful-hd" },
    { "textures", "cache_mb", RECOMP_SETTING_INT, PC_TEXTURE_CACHE_MB, 32, 4096, NULL, "RECOMP_TEXTURE_CACHE_MB", RECOMP_SETTING_RESTART,
      "Limits memory used by Upscaled Original Textures and downloadable texture packs. "
      "Try increasing it if textures pop in, load slowly, or cause repeated stutters: a low budget can force textures to unload and reload. "
      "Higher values use more graphics and system memory, including retained upload data. "
      "Leave memory for the rest of the game and other apps. Restart the game after changing this." },
    { "textures", "dump", RECOMP_SETTING_BOOL, 0, 0, 0, NULL, NULL, RECOMP_SETTING_RESTART,
      "Capture source textures for making a pack, in hd-work/runtime under the data folder." },
    { "textures", "dump_limit", RECOMP_SETTING_INT, 2000, 0, 65536, NULL, "RECOMP_TEXTURE_DUMP_LIMIT", RECOMP_SETTING_RESTART,
      "Maximum new source images captured in one run." },
    { "display", "fullscreen", RECOMP_SETTING_BOOL, 0, 0, 0, NULL, NULL, 0,
      "Borderless full screen. Alt+Enter or F11 switches while playing." },
    { "display", "window_width", RECOMP_SETTING_INT, 1920, 320, 16384, NULL, NULL, 0,
      "Window size when not full screen. Resizing the window updates these." },
    { "display", "window_height", RECOMP_SETTING_INT, 1080, 240, 16384, NULL, NULL, 0, NULL },
    { "display", "aspect", RECOMP_SETTING_ENUM, 2, 0, 0, k_aspect, "RECOMP_ASPECT", 0,
      "4:3 keeps the console's picture shape with black bars; stretch fills the window; the widescreen shapes "
      "(beta) widen the 3D view and keep menus, HUD and movies in shape, and start with the game." },
    { "display", "widescreen_hud", RECOMP_SETTING_ENUM, 0, 0, 0, k_ws_hud, NULL, RECOMP_SETTING_RESTART,
      "Where the fight HUD goes in widescreen: the screen edges, or a centred 16:9 box (auto: edges up to 16:9)." },
    { "display", "widescreen_cutscenes", RECOMP_SETTING_ENUM, 0, 0, 0, k_ws_cutscenes, NULL, RECOMP_SETTING_RESTART,
      "Story cutscenes in widescreen: authored keeps the game's own 16:9 framing, fill always fills the screen." },
    { "display", "widescreen_side_fill", RECOMP_SETTING_ENUM, 0, 0, 0, k_ws_fill, NULL, RECOMP_SETTING_RESTART,
      "Beside 4:3 pictures (movies, menu backgrounds) in widescreen: a soft blurred extension, or black." },
    { "display", "widescreen_fov_cap", RECOMP_SETTING_INT, 150, 90, 170, NULL, NULL, RECOMP_SETTING_RESTART,
      "Widest horizontal field of view widescreen may use, in degrees (only very wide screens reach it)." },
#if defined(__ANDROID__)
    { "display", "render_height151", RECOMP_SETTING_INT, 0, 0, 2160, NULL, NULL,
      RECOMP_SETTING_RESTART, "Internal vertical resolution:480,540,720,960,1080,1440,1920,2160. 0 keeps legacy render_scale. Width follows aspect. Restart required." },
#endif
    { "display", "render_scale", RECOMP_SETTING_INT, RENDER_DEFAULT145, 1, 4, NULL, "RECOMP_RENDER_SCALE",
      RECOMP_SETTING_RESTART,
      "Internal render height: 1x = 480p, 2x = 960p, 3x = 1440p, 4x = 1920p. "
      "1080p output: 2x balances speed and clarity; 3x is sharper. 1440p output: 3x. "
      "Width follows Picture shape; window size is separate. Higher scales cost GPU time. "
      "Applies on Play/next launch." },
    { "display", "filter", RECOMP_SETTING_ENUM, 0, 0, 0, k_filter, "RECOMP_PRESENT_FILTER", 0,
      "How the picture is scaled to the window. smooth (high quality) also averages a picture rendered "
      "larger than the window instead of skipping pixels (render scale above the window's size)." },
    { "display", "msaa", RECOMP_SETTING_ENUM, 0, 0, 0, k_msaa, "RECOMP_MSAA", RECOMP_SETTING_RESTART,
      "Multisample anti-aliasing: smooths the jagged edges of fighters and the arena, as the console's "
      "own anti-aliasing did. Costs GPU time and memory; lowered to what the graphics card supports." },
    { "display", "texture_mips", RECOMP_SETTING_BOOL, 0, 0, 0, NULL, "RECOMP_TEXTURE_MIPS", 0,
      "Use the smaller texture levels the game ships, as the console does, so distant floors and "
      "crowds do not shimmer." },
    { "display", "anisotropy", RECOMP_SETTING_ENUM, 0, 0, 0, k_aniso, "RECOMP_ANISOTROPY", 0,
      "Anisotropic filtering: keeps floors and walls seen at a low angle sharp. Works best with the "
      "game's texture levels on." },
    { "display", "vsync", RECOMP_SETTING_BOOL, VSYNC_DEFAULT145, 0, 0, NULL, "RECOMP_PRESENT_VSYNC", 0,
      "Pace frames on the display for even motion. Used when the refresh rate is a multiple of 60 (60, 120, 180, 240 Hz); other displays use the game's own 60 fps timer." },
    { "display", "gamma", RECOMP_SETTING_BOOL, 1, 0, 0, NULL, "RECOMP_GAMMA",
      RECOMP_SETTING_RESTART,
      "Apply the game's own brightness curve, as the console does." },
    { "display", "brightness", RECOMP_SETTING_INT, 100, 50, 200, NULL, "RECOMP_BRIGHTNESS", 0,
      "Brightness of the picture's mid-tones; black and white stay as they are. 100 is the console's picture, "
      "higher is brighter." },
    { "display", "vignette", RECOMP_SETTING_BOOL, 1, 0, 0, NULL, "RECOMP_VIGNETTE", 0,
      "The darkened screen edges that venues and scenes draw. Off removes them." },
    { "display", "vignette_strength", RECOMP_SETTING_INT, 100, 0, 100, NULL, "RECOMP_VIGNETTE_STRENGTH", 0,
      "How dark the vignette is, in percent of the game's own (100)." },
    { "display", "color_tint", RECOMP_SETTING_INT, 100, 0, 100, NULL, "RECOMP_COLOR_TINT", 0,
      "Strength of the colour tint venues and scenes put over the picture, in percent of the game's own "
      "(100); 0 shows the picture untinted." },
    { "display", "scratches", RECOMP_SETTING_BOOL, 1, 0, 0, NULL, "RECOMP_SCRATCHES", 0,
      "The film scratches some scenes draw over the picture. Off removes them." },
};
#define DISPLAY_SETTING_COUNT (sizeof(g_display_settings) / sizeof(g_display_settings[0]))

size_t pc_debug_settings(RecompSetting *out, size_t max)
{
    size_t n = 0;
    if (max < 11) return 0;
    memset(out, 0, sizeof(*out) * 11);
#define ADD(...) do { RecompSetting s_ = { __VA_ARGS__ }; out[n++] = s_; } while (0)
    ADD("debug", "record", RECOMP_SETTING_BOOL, 0, 0, 0, NULL, "RECOMP_RECORD", RECOMP_SETTING_RESTART,
        "Keep a play-test record of every run under logs/sessions: frame times, events, the sound and the last "
        "seconds of pictures. The mark key writes a bug bundle of those seconds. See docs/playtest-recorder.md.");
    ADD("debug", "mark_key", RECOMP_SETTING_STRING, 0, 0, 0, NULL, NULL, RECOMP_SETTING_RESTART,
        "While recording, the key that marks a moment and writes a bug bundle of the seconds around it.", "F9");
    ADD("debug", "mark_pad", RECOMP_SETTING_STRING, 0, 0, 0, NULL, NULL, RECOMP_SETTING_RESTART,
        "While recording, pad buttons that mark a moment, for a pad or a Steam Deck. Alternatives are separated "
        "by commas; a combination joined with + must be held for half a second. Button names as in [gamepad]. "
        "Empty for none.", "back+left_stick_click");
    ADD("debug", "record_fps", RECOMP_SETTING_INT, 10, 0, 30, NULL, "RECOMP_RECORD_FPS", RECOMP_SETTING_RESTART,
        "Pictures a second kept for bundles (0 for none). Each one is read back from the graphics card, which "
        "costs that frame a little; frames.csv records how much.");
    ADD("debug", "record_seconds", RECOMP_SETTING_INT, 30, 5, 120, NULL, "RECOMP_RECORD_SECONDS",
        RECOMP_SETTING_RESTART, "How many seconds before a mark a bundle holds.");
    ADD("debug", "record_audio", RECOMP_SETTING_BOOL, 1, 0, 0, NULL, NULL, RECOMP_SETTING_RESTART,
        "Also keep the mixed sound (about 11 MB a minute; older sessions lose it, see keep_sessions).");
    ADD("debug", "record_game_state", RECOMP_SETTING_BOOL, 1, 0, 0, NULL, NULL, RECOMP_SETTING_RESTART,
        "Also log the fighters, health and match events (the test harness's read-only probes).");
    ADD("debug", "auto_bundle_hitch_ms", RECOMP_SETTING_INT, 0, 0, 5000, NULL, "RECOMP_RECORD_AUTO_HITCH_MS",
        RECOMP_SETTING_RESTART,
        "A frame at least this long writes a bundle by itself (at most five a run, a minute apart). 0: only marks.");
    ADD("debug", "hang_seconds", RECOMP_SETTING_INT, 15, 0, 600, NULL, "RECOMP_RECORD_HANG_SECS",
        RECOMP_SETTING_RESTART, "No frame for this many seconds writes a hang bundle. 0: never.");
    ADD("debug", "keep_sessions", RECOMP_SETTING_INT, 20, 1, 500, NULL, NULL, RECOMP_SETTING_RESTART,
        "Sessions kept whole; older ones keep their bundles and events only.");
    ADD("debug", "record_guest_ram", RECOMP_SETTING_BOOL, 0, 0, 0, NULL, NULL, RECOMP_SETTING_RESTART,
        "Crash and hang bundles also hold the game's 64 MB of memory, for offline inspection.");
#undef ADD
    return n;
}

size_t pc_settings_catalog(RecompSetting *out, size_t max)
{
    size_t n = DISPLAY_SETTING_COUNT;
    if (max < PC_SETTINGS_MAX) return 0;
    memcpy(out, g_display_settings, sizeof(g_display_settings));
    n += pc_input_settings(out + n, 64);
    n += pc_ui_settings(out + n, 16);
    n += pc_debug_settings(out + n, 16);
    n += pc_language_settings(out + n, 4);
    n += pc_songs_settings(out + n, 8);
    n += cheats_settings(out + n, 8);
    return n;
}

static RecompSetting g_settings[PC_SETTINGS_MAX];
static char g_path[4096];   /* empty: do not load or save */

static void settings_log(const char *message)
{
    fprintf(stderr, "[SETTINGS] %s\n", message);
}

int pc_settings_open(const char *path)
{
    size_t count = pc_settings_catalog(g_settings, PC_SETTINGS_MAX);
    int read;

    snprintf(g_path, sizeof g_path, "%s", path ? path : "");
    recomp_settings_register(g_settings, count);
    recomp_settings_set_log(settings_log);
    read = g_path[0] ? recomp_settings_load(g_path) : 0;
    if (!g_path[0])
        fprintf(stderr, "[SETTINGS] defaults only (RECOMP_SETTINGS=none)\n");
    else if (read < 0)
        fprintf(stderr, "[SETTINGS] could not read %s; using defaults\n", g_path);
    else {
        fprintf(stderr, "[SETTINGS] %s (%d value(s) read)\n", g_path, read);
        /* Write it out on first run, and after an upgrade adds a setting, so
         * there is always a complete, commented file to edit. */
        if (read < (int)count && recomp_settings_save(g_path))
            fprintf(stderr, "[SETTINGS] could not write %s\n", g_path);
    }
    return read;
}

int pc_display(const char *key, int fallback)
{
    return recomp_settings_get("display", key, fallback);
}

const char *pc_settings_path(void) { return g_path; }

int pc_settings_save_checked(void)
{
    int error = g_path[0] ? recomp_settings_save(g_path) : 0;
    if (error) fprintf(stderr, "[SETTINGS] could not write %s\n", g_path);
    return error ? 1 : 0;
}
void pc_settings_save(void) { (void)pc_settings_save_checked(); }

/* Keep this ownership list identical for both directions. HD is a separate optional install;
 * Original disables replacements without deleting packs. Modern preserves their selection. */
static const struct { const char *key; int original, modern; } k_experience[] = {
    {"aspect", 0, 2}, {"render_scale", 1, 2}, {"filter", 0, 2},
    {"msaa", 1, 1}, {"texture_mips", 1, 1}, {"anisotropy", 0, 4},
    {"vsync", 1, 1}, {"gamma", 1, 1}, {"brightness", 100, 100},
    {"vignette", 1, 1}, {"vignette_strength", 100, 100},
    {"color_tint", 100, 100}, {"scratches", 1, 1},
    {"widescreen_hud", 0, 0}, {"widescreen_cutscenes", 0, 0},
    {"widescreen_side_fill", 0, 0}, {"widescreen_fov_cap", 150, 150}
};
int pc_experience_current(void)
{
    int original = !recomp_settings_get("textures", "enabled", 0), modern = 1;
    for (size_t i = 0; i < sizeof(k_experience)/sizeof(k_experience[0]); ++i) {
        int v = recomp_settings_get("display", k_experience[i].key, -1);
        original &= v == k_experience[i].original;
        modern &= v == k_experience[i].modern;
    }
    return original ? 0 : modern ? 1 : -1;
}
int pc_experience_apply(int modern)
{
    if (modern != 0 && modern != 1) return -1;
    /* An environment override must not leave a silently partial preset. */
    for (size_t i = 0; i < sizeof(k_experience)/sizeof(k_experience[0]); ++i) {
        RecompSetting *s = recomp_settings_find("display", k_experience[i].key);
        if (!s || s->source == RECOMP_SETTING_FROM_ENV) return -1;
    }
    RecompSetting *textures = recomp_settings_find("textures", "enabled");
    RecompSetting *packs = recomp_settings_find("textures", "packs");
    if (!modern && ((textures && textures->source == RECOMP_SETTING_FROM_ENV) ||
                   (packs && packs->source == RECOMP_SETTING_FROM_ENV))) return -1;
    for (size_t i = 0; i < sizeof(k_experience)/sizeof(k_experience[0]); ++i)
        recomp_settings_set("display", k_experience[i].key,
                            modern ? k_experience[i].modern : k_experience[i].original);
    if (!modern) recomp_settings_set("textures", "enabled", 0);
    return pc_settings_save_checked();
}

void pc_display_set(const char *key, int value)
{
    if (recomp_settings_set("display", key, value) > 0 && g_path[0]) {
        if (recomp_settings_save(g_path))
            fprintf(stderr, "[SETTINGS] could not write %s\n", g_path);
    }
}
