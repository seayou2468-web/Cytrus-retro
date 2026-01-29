#include <iostream>
#include <string>
#include <vector>

#include "libretro.h"
#include "libretro_options.h"

#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "core/loader/loader.h"
#include "libretro_emu_window.h"
#include "video_core/renderer_software/renderer_software.h"
#include "InputManager/InputManager.h"
#include "core/hle/service/hid/hid.h"
#include "audio_core/libretro_sink.h"

// Static variables for libretro
static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static bool game_loaded = false;
static std::unique_ptr<Frontend::LibretroEmuWindow> emu_window;
static std::string layout_option = "vertical";

// Libretro API Implementation

RETRO_API void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;
    cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &core_options_v2);
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) {
    video_cb = cb;
}

RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) {
    audio_cb = cb;
}

RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    audio_batch_cb = cb;
}

RETRO_API void retro_set_input_poll(retro_input_poll_t cb) {
    input_poll_cb = cb;
}

RETRO_API void retro_set_input_state(retro_input_state_t cb) {
    input_state_cb = cb;
}

RETRO_API void retro_init(void) {
    Common::Log::Initialize();
    Common::Log::Start();
    InputManager::Init();
}

RETRO_API void retro_deinit(void) {
    Core::System::GetInstance().Shutdown();
}

RETRO_API unsigned retro_api_version(void) {
    return RETRO_API_VERSION;
}

RETRO_API void retro_get_system_info(struct retro_system_info *info) {
    info->library_name = "Cytrus";
    info->library_version = "v1.0";
    info->valid_extensions = "3ds|3dsx|cia|app|axf|cci|cxi";
    info->need_fullpath = true;
    info->block_extract = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info) {
    // Default 3DS resolution: 400x240 (top) + 320x240 (bottom)
    // We'll adjust this based on layout later.
    info->geometry.base_width = 400;
    info->geometry.base_height = 480; // Vertical layout default
    info->geometry.max_width = 1600; // 4x scaling
    info->geometry.max_height = 1920;
    info->geometry.aspect_ratio = 400.0f / 480.0f;

    info->timing.fps = 60.0;
    info->timing.sample_rate = 44100.0;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device) {
    (void)port;
    (void)device;
}

RETRO_API void retro_reset(void) {
    Core::System::GetInstance().Reset();
}

static void update_variables() {
    bool updated = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
        struct retro_variable var;
        var.key = "cytrus_layout";
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
            layout_option = var.value;
        }
    }
}

static void update_input() {
    input_poll_cb();

    static const struct {
        unsigned retro_id;
        int n3ds_id;
    } button_map[] = {
        { RETRO_DEVICE_ID_JOYPAD_A, InputManager::N3DS_BUTTON_A },
        { RETRO_DEVICE_ID_JOYPAD_B, InputManager::N3DS_BUTTON_B },
        { RETRO_DEVICE_ID_JOYPAD_X, InputManager::N3DS_BUTTON_X },
        { RETRO_DEVICE_ID_JOYPAD_Y, InputManager::N3DS_BUTTON_Y },
        { RETRO_DEVICE_ID_JOYPAD_START, InputManager::N3DS_BUTTON_START },
        { RETRO_DEVICE_ID_JOYPAD_SELECT, InputManager::N3DS_BUTTON_SELECT },
        { RETRO_DEVICE_ID_JOYPAD_UP, InputManager::N3DS_DPAD_UP },
        { RETRO_DEVICE_ID_JOYPAD_DOWN, InputManager::N3DS_DPAD_DOWN },
        { RETRO_DEVICE_ID_JOYPAD_LEFT, InputManager::N3DS_DPAD_LEFT },
        { RETRO_DEVICE_ID_JOYPAD_RIGHT, InputManager::N3DS_DPAD_RIGHT },
        { RETRO_DEVICE_ID_JOYPAD_L, InputManager::N3DS_TRIGGER_L },
        { RETRO_DEVICE_ID_JOYPAD_R, InputManager::N3DS_TRIGGER_R },
        { RETRO_DEVICE_ID_JOYPAD_L2, InputManager::N3DS_BUTTON_ZL },
        { RETRO_DEVICE_ID_JOYPAD_R2, InputManager::N3DS_BUTTON_ZR },
    };

    auto* button_handler = InputManager::ButtonHandler();
    for (const auto& map : button_map) {
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, map.retro_id)) {
            button_handler->PressKey(map.n3ds_id);
        } else {
            button_handler->ReleaseKey(map.n3ds_id);
        }
    }

    // Analog sticks
    auto* analog_handler = InputManager::AnalogHandler();
    float lx = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) / 32768.0f;
    float ly = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / 32768.0f;
    analog_handler->MoveJoystick(InputManager::N3DS_CIRCLEPAD, lx, -ly);

    float rx = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) / 32768.0f;
    float ry = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / 32768.0f;
    analog_handler->MoveJoystick(InputManager::N3DS_STICK_C, rx, -ry);
}

RETRO_API void retro_run(void) {
    if (!game_loaded) return;

    update_variables();
    update_input();

    auto& system = Core::System::GetInstance();
    system.RunLoop();

    // After RunLoop, present the frame
    auto& renderer = static_cast<SwRenderer::RendererSoftware&>(system.GPU().Renderer());
    emu_window->Present(renderer, video_cb, layout_option);

    // Pump audio
    if (AudioCore::g_fill_callback) {
        // Citra usually outputs 1 frame worth of audio.
        // Sample rate 44100 / 60 FPS = 735 samples per frame.
        s16 audio_buffer[1024 * 2]; // Interleaved stereo
        AudioCore::g_fill_callback(audio_buffer, 735);
        audio_batch_cb(audio_buffer, 735);
    }
}

RETRO_API size_t retro_serialize_size(void) {
    return 0; // TODO: Implement
}

RETRO_API bool retro_serialize(void *data, size_t len) {
    (void)data;
    (void)len;
    return false;
}

RETRO_API bool retro_unserialize(const void *data, size_t len) {
    (void)data;
    (void)len;
    return false;
}

RETRO_API void retro_cheat_reset(void) {}

RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char *code) {
    (void)index;
    (void)enabled;
    (void)code;
}

RETRO_API bool retro_load_game(const struct retro_game_info *game) {
    if (!game) return false;

    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
        return false;
    }

    emu_window = std::make_unique<Frontend::LibretroEmuWindow>();

    auto& system = Core::System::GetInstance();

    // Set settings
    Settings::values.graphics_api.SetValue(Settings::GraphicsAPI::Software);
    Settings::values.use_cpu_jit.SetValue(false);
    Settings::values.output_type.SetValue(AudioCore::SinkType::Libretro);

    if (system.Load(*emu_window, game->path) != Core::System::ResultStatus::Success) {
        return false;
    }

    game_loaded = true;
    return true;
}

RETRO_API bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info) {
    (void)game_type;
    (void)info;
    (void)num_info;
    return false;
}

RETRO_API void retro_unload_game(void) {
    Core::System::GetInstance().Shutdown();
    game_loaded = false;
}

RETRO_API unsigned retro_get_region(void) {
    return RETRO_REGION_NTSC;
}

RETRO_API void *retro_get_memory_data(unsigned id) {
    (void)id;
    return NULL;
}

RETRO_API size_t retro_get_memory_size(unsigned id) {
    (void)id;
    return 0;
}
