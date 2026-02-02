#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "libretro.h"
#include "core/core.h"
#include "common/settings.h"
#include "libretro_emu_window.h"
#include "common/logging/log.h"
#include "core/hle/service/hid/hid.h"
#include "video_core/pica/regs_lcd.h"
#include "video_core/renderer_software/renderer_software.h"
#include "video_core/gpu.h"
#include "audio_core/dsp_interface.h"
#include "audio_core/hle/hle.h"
#include "libretro_sink.h"
#include "libretro_input.h"
#include "common/logging/backend.h"
#include "common/file_util.h"
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <retro_dirent.h>
#include <string/stdstring.h>

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_log_printf_t log_cb;

static void libretro_log_callback(int level, const char* fmt, ...) {
    char buffer[4096];
    va_list v;
    va_start(v, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, v);
    va_end(v);

    if (log_cb) {
        log_cb((enum retro_log_level)level, "%s", buffer);
    } else {
        fprintf(stderr, "%s", buffer);
    }
}

static Core::System* system_instance = nullptr;
static LibretroEmuWindow* emu_window = nullptr;

struct ButtonMapping {
    unsigned retro;
    Settings::NativeButton::Values native;
};

static const ButtonMapping button_map[] = {
    { RETRO_DEVICE_ID_JOYPAD_A, Settings::NativeButton::A },
    { RETRO_DEVICE_ID_JOYPAD_B, Settings::NativeButton::B },
    { RETRO_DEVICE_ID_JOYPAD_X, Settings::NativeButton::X },
    { RETRO_DEVICE_ID_JOYPAD_Y, Settings::NativeButton::Y },
    { RETRO_DEVICE_ID_JOYPAD_UP, Settings::NativeButton::Up },
    { RETRO_DEVICE_ID_JOYPAD_DOWN, Settings::NativeButton::Down },
    { RETRO_DEVICE_ID_JOYPAD_LEFT, Settings::NativeButton::Left },
    { RETRO_DEVICE_ID_JOYPAD_RIGHT, Settings::NativeButton::Right },
    { RETRO_DEVICE_ID_JOYPAD_L, Settings::NativeButton::L },
    { RETRO_DEVICE_ID_JOYPAD_R, Settings::NativeButton::R },
    { RETRO_DEVICE_ID_JOYPAD_START, Settings::NativeButton::Start },
    { RETRO_DEVICE_ID_JOYPAD_SELECT, Settings::NativeButton::Select },
    { RETRO_DEVICE_ID_JOYPAD_L2, Settings::NativeButton::ZL },
    { RETRO_DEVICE_ID_JOYPAD_R2, Settings::NativeButton::ZR },
};

void retro_init(void) {
    struct retro_vfs_interface_info vfs_info = { 3, nullptr };
    if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_info)) {
        filestream_vfs_init(&vfs_info);
        path_vfs_init(&vfs_info);
        dirent_vfs_init(&vfs_info);
    }

    system_instance = &Core::System::GetInstance();
    emu_window = new LibretroEmuWindow();
    Input::RegisterLibretroInput();
}

void retro_deinit(void) {
    if (system_instance) {
        system_instance->Shutdown();
    }
    delete emu_window;
    emu_window = nullptr;
}

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info) {
    memset(info, 0, sizeof(*info));
    info->library_name = "Cytrus";
    info->library_version = "v1";
    info->need_fullpath = true;
    info->valid_extensions = "3ds|cia|app";
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
    struct retro_variable var = { "cytrus_layout", nullptr };
    bool side_by_side = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (string_is_equal(var.value, "Side-by-Side")) {
            side_by_side = true;
        }
    }

    if (side_by_side) {
        info->geometry.base_width = 720;
        info->geometry.base_height = 240;
        info->geometry.aspect_ratio = 720.0f / 240.0f;
    } else {
        info->geometry.base_width = 400;
        info->geometry.base_height = 480;
        info->geometry.aspect_ratio = 400.0f / 480.0f;
    }
    info->geometry.max_width = 800;
    info->geometry.max_height = 800;
    info->timing.fps = 59.8261;
    info->timing.sample_rate = 44100.0;
}

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;

    struct retro_log_callback log;
    if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log)) {
        log_cb = log.log;
        Common::Log::SetLibretroLogCallback(libretro_log_callback);
    }

    static const struct retro_variable vars[] = {
        { "cytrus_model", "Console Model; Old 3DS|New 3DS" },
        { "cytrus_layout", "Screen Layout; Vertical|Side-by-Side" },
        { nullptr, nullptr },
    };
    environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

bool retro_load_game(const struct retro_game_info *game) {
    if (!game) return false;

    const char* system_dir = nullptr;
    if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir) {
        FileUtil::SetUserPath(std::string(system_dir) + "/citra-emu/");
    }

    // Set pixel format
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
        LOG_ERROR(Frontend, "XRGB8888 is not supported.");
        return false;
    }

    struct retro_variable var = { "cytrus_model", nullptr };
    Settings::values.is_new_3ds = false;
    Settings::values.cpu_clock_percentage.SetValue(100);
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (string_is_equal(var.value, "New 3DS")) {
            Settings::values.is_new_3ds = true;
            Settings::values.cpu_clock_percentage.SetValue(400);
        }
    }

    // Configure Input
    auto& p = Settings::values.current_input_profile;
    for (int i = 0; i < Settings::NativeButton::NumButtons; i++) {
        p.buttons[i] = fmt::format("engine:libretro,button:{}", i);
    }
    p.analogs[Settings::NativeAnalog::CirclePad] = "engine:libretro,axis_x:0,axis_y:1";
    p.analogs[Settings::NativeAnalog::CStick] = "engine:libretro,axis_x:2,axis_y:3";
    p.touch_device = "engine:libretro";

    static const struct retro_input_descriptor desc[] = {
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "A" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "Y" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "ZL" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "ZR" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" },
        { 0, 0, 0, 0, nullptr }
    };
    environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void*)desc);

    Settings::values.graphics_api.SetValue(Settings::GraphicsAPI::Software);

    if (system_instance->Load(*emu_window, game->path) != Core::System::ResultStatus::Success) {
        return false;
    }

    return true;
}

void retro_unload_game(void) {
    if (system_instance) system_instance->Shutdown();
}

void retro_run(void) {
    bool updated = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
        struct retro_variable var = { "cytrus_layout", nullptr };
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
            bool side_by_side = string_is_equal(var.value, "Side-by-Side");
            struct retro_game_geometry geometry;
            if (side_by_side) {
                geometry.base_width = 720;
                geometry.base_height = 240;
                geometry.aspect_ratio = 720.0f / 240.0f;
            } else {
                geometry.base_width = 400;
                geometry.base_height = 480;
                geometry.aspect_ratio = 400.0f / 480.0f;
            }
            geometry.max_width = 800;
            geometry.max_height = 800;
            environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &geometry);
        }
    }

    input_poll_cb();

    static bool supports_bitmask = false;
    static bool supports_bitmask_init = false;
    if (!supports_bitmask_init) {
        if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, nullptr)) {
            supports_bitmask = true;
        }
        supports_bitmask_init = true;
    }

    // Update Joypad
    if (supports_bitmask) {
        int16_t mask = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
        for (auto& mapping : button_map) {
            Input::LibretroSetButton(mapping.native, (mask & (1 << mapping.retro)) != 0);
        }
    } else {
        for (auto& mapping : button_map) {
            bool pressed = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, mapping.retro);
            Input::LibretroSetButton(mapping.native, pressed);
        }
    }

    // Update Analogs
    float circle_x = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) / 32768.0f;
    float circle_y = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / -32768.0f;
    Input::LibretroSetAnalog(false, circle_x, circle_y);

    float c_stick_x = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) / 32768.0f;
    float c_stick_y = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / -32768.0f;
    Input::LibretroSetAnalog(true, c_stick_x, c_stick_y);

    // Update Touch
    bool touch_pressed = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
    if (touch_pressed) {
        float tx = (input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) + 32767) / 65535.0f;
        float ty = (input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) + 32767) / 65535.0f;
        // Map tyrosine to 3DS bottom screen area
        // In vertical layout, top is 400x240, bottom is 320x240 centered (40px padding)
        // ty in [0.5, 1.0] corresponds to bottom screen
        if (ty >= 0.5f) {
            float bty = (ty - 0.5f) * 2.0f;
            float btx = (tx - 0.1f) / 0.8f; // 320/400 = 0.8, centered
            Input::LibretroSetTouch(btx, bty, true);
        } else {
            Input::LibretroSetTouch(0, 0, false);
        }
    } else {
        Input::LibretroSetTouch(0, 0, false);
    }

    if (system_instance->RunLoop(true) != Core::System::ResultStatus::Success) {
        // Handle error
    }

    // Delegate video output
    auto& gpu = system_instance->GPU();
    auto& renderer = static_cast<SwRenderer::RendererSoftware&>(gpu.Renderer());
    const auto& top_screen = renderer.Screen(VideoCore::ScreenId::TopLeft);
    const auto& bottom_screen = renderer.Screen(VideoCore::ScreenId::Bottom);

    struct retro_variable var_layout = { "cytrus_layout", nullptr };
    bool side_by_side = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var_layout) && var_layout.value) {
        if (string_is_equal(var_layout.value, "Side-by-Side")) {
            side_by_side = true;
        }
    }

    struct retro_framebuffer fb = {0};
    fb.width = side_by_side ? 720 : 400;
    fb.height = side_by_side ? 240 : 480;
    fb.access_flags = RETRO_MEMORY_ACCESS_WRITE;

    void* output_data = nullptr;
    size_t output_pitch = fb.width * sizeof(u32);

    static std::vector<u32> combined_fb;

    if (environ_cb(RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER, &fb) && fb.data) {
        output_data = fb.data;
        output_pitch = fb.pitch;
    } else {
        if (combined_fb.size() != fb.width * fb.height) combined_fb.assign(fb.width * fb.height, 0xFF000000);
        output_data = combined_fb.data();
        output_pitch = fb.width * sizeof(u32);
    }

    if (side_by_side) {
        if (!top_screen.pixels.empty()) {
            for (u32 y = 0; y < 240; y++) {
                std::memcpy((u8*)output_data + y * output_pitch, top_screen.pixels.data() + y * 400 * 4, 400 * 4);
            }
        }
        if (!bottom_screen.pixels.empty()) {
            for (u32 y = 0; y < 240; y++) {
                std::memcpy((u8*)output_data + y * output_pitch + 400 * 4, bottom_screen.pixels.data() + y * 320 * 4, 320 * 4);
            }
        }
    } else {
        if (!top_screen.pixels.empty()) {
            for (u32 y = 0; y < 240; y++) {
                std::memcpy((u8*)output_data + y * output_pitch, top_screen.pixels.data() + y * 400 * 4, 400 * 4);
            }
        }
        if (!bottom_screen.pixels.empty()) {
            for (u32 y = 0; y < 240; y++) {
                std::memcpy((u8*)output_data + (y + 240) * output_pitch + 40 * 4, bottom_screen.pixels.data() + y * 320 * 4, 320 * 4);
            }
        }
    }

    video_cb(output_data, fb.width, fb.height, output_pitch);

    // Delegate audio output
    auto& dsp = system_instance->DSP();
    auto& sink = static_cast<AudioCore::LibretroSink&>(dsp.GetSink());
    s16 audio_buffer[44100 / 50 * 2]; // Enough for one frame
    std::size_t num_frames = 44100 / 60; // Approximate
    sink.Pull(audio_buffer, num_frames);
    audio_batch_cb(audio_buffer, num_frames);
}

void retro_reset(void) { system_instance->RequestReset(); }

size_t retro_serialize_size(void) {
    // 300MB is enough to hold uncompressed FCRAM (256MB) and other states
    return 300 * 1024 * 1024;
}

bool retro_serialize(void *data, size_t size) {
    try {
        std::ostringstream sstream{std::ios_base::binary};
        oarchive oa{sstream};
        oa << *system_instance;
        std::string str = sstream.str();
        if (str.size() > size) {
            LOG_ERROR(Frontend, "Savestate buffer too small: {} > {}", str.size(), size);
            return false;
        }
        memcpy(data, str.data(), str.size());
        // Zero-fill remaining space to ensure determinism for RetroArch rollback/netplay
        if (size > str.size()) {
            memset((uint8_t*)data + str.size(), 0, size - str.size());
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(Frontend, "Failed to serialize: {}", e.what());
        return false;
    } catch (...) {
        LOG_ERROR(Frontend, "Failed to serialize (unknown error)");
        return false;
    }
}

bool retro_unserialize(const void *data, size_t size) {
    try {
        std::string str((const char*)data, size);
        std::istringstream sstream{str, std::ios_base::binary};
        iarchive ia{sstream};
        ia >> *system_instance;
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(Frontend, "Failed to unserialize: {}", e.what());
        return false;
    } catch (...) {
        LOG_ERROR(Frontend, "Failed to unserialize (unknown error)");
        return false;
    }
}

void retro_cheat_reset(void) {
    system_instance->CheatEngine().LoadCheatFile(0); // TODO: Pass actual title_id
}

void retro_cheat_set(unsigned index, bool enabled, const char *code) {
    // Citra's cheat engine might need more complex integration, but we can try basic support
}

void* retro_get_memory_data(unsigned id) {
    switch (id) {
        case RETRO_MEMORY_SYSTEM_RAM:
            return system_instance->Memory().GetFCRAMPointer(0);
        case RETRO_MEMORY_VIDEO_RAM:
            return system_instance->Memory().GetPhysicalPointer(Memory::VRAM_PADDR);
        default:
            return nullptr;
    }
}

size_t retro_get_memory_size(unsigned id) {
    switch (id) {
        case RETRO_MEMORY_SYSTEM_RAM:
            return Settings::values.is_new_3ds ? Memory::FCRAM_N3DS_SIZE : Memory::FCRAM_SIZE;
        case RETRO_MEMORY_VIDEO_RAM:
            return Memory::VRAM_SIZE;
        default:
            return 0;
    }
}

unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

void retro_set_controller_port_device(unsigned port, unsigned device) {}
