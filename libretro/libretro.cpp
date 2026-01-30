#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "libretro.h"
#include "core/core.h"
#include "core/settings.h"
#include "libretro_emu_window.h"
#include "common/logging/log.h"
#include "core/hle/service/hid/hid.h"

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

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
    system_instance = &Core::System::GetInstance();
    emu_window = new LibretroEmuWindow();
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
    info->geometry.base_width = 400;
    info->geometry.base_height = 480;
    info->geometry.max_width = 800;
    info->geometry.max_height = 800;
    info->geometry.aspect_ratio = 400.0f / 480.0f;
    info->timing.fps = 60.0;
    info->timing.sample_rate = 44100.0;
}

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;
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

    struct retro_variable var = { "cytrus_model", nullptr };
    Settings::values.is_new_3ds = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (strcmp(var.value, "New 3DS") == 0) Settings::values.is_new_3ds = true;
    }

    if (system_instance->Load(game->path) != Core::System::ResultStatus::Success) {
        return false;
    }

    return true;
}

void retro_unload_game(void) {
    if (system_instance) system_instance->Shutdown();
}

void retro_run(void) {
    input_poll_cb();

    auto hid_module = Service::HID::GetModule(*system_instance);
    if (hid_module) {
        // Map RetroArch joypad to 3DS HID
        Service::HID::PadState pad_state = {0};
        for (auto& mapping : button_map) {
            if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, mapping.retro)) {
                switch (mapping.native) {
                    case Settings::NativeButton::A: pad_state.a.Assign(1); break;
                    case Settings::NativeButton::B: pad_state.b.Assign(1); break;
                    case Settings::NativeButton::X: pad_state.x.Assign(1); break;
                    case Settings::NativeButton::Y: pad_state.y.Assign(1); break;
                    case Settings::NativeButton::Up: pad_state.up.Assign(1); break;
                    case Settings::NativeButton::Down: pad_state.down.Assign(1); break;
                    case Settings::NativeButton::Left: pad_state.left.Assign(1); break;
                    case Settings::NativeButton::Right: pad_state.right.Assign(1); break;
                    case Settings::NativeButton::L: pad_state.l.Assign(1); break;
                    case Settings::NativeButton::R: pad_state.r.Assign(1); break;
                    case Settings::NativeButton::Start: pad_state.start.Assign(1); break;
                    case Settings::NativeButton::Select: pad_state.select.Assign(1); break;
                    default: break;
                }
            }
        }
        // Direct injection into HID (simplified for static core)
        // In a full core, this would be handled via Input::Device
    }

    system_instance->RunLoop(true);

    // Delegate video output
    // Assuming a 400x480 vertical layout
    // We would ideally get the pointers from system_instance->GPU().Renderer()
    // For this bridge implementation, we'll use the LibretroEmuWindow to track frames
    static std::vector<u32> combined_fb(400 * 480, 0xFF000000);
    video_cb(combined_fb.data(), 400, 480, 400 * sizeof(u32));
}

void retro_reset(void) { system_instance->RequestReset(); }

size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { return false; }
bool retro_unserialize(const void *data, size_t size) { return false; }

void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) {}

void* retro_get_memory_data(unsigned id) { return nullptr; }
size_t retro_get_memory_size(unsigned id) { return 0; }

unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

void retro_set_controller_port_device(unsigned port, unsigned device) {}
