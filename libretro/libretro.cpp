#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <climits>
#include <cstdint>

#include "libretro.h"
#include "common/settings.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/loader/loader.h"
#include "common/file_util.h"
#include "video_core/renderer_base.h"
#include "core/frontend/emu_window.h"
#include "emu_window.h"
#include "audio_core/dsp_interface.h"
#include "core/hle/service/service.h"
#include "libretro_sink.h"
#include "InputManager/InputManager.h"
#include "input_common/main.h"

#include "file/file_path.h"
#include "streams/file_stream.h"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static LibretroEmuWindow* emu_window = nullptr;
static AudioCore::LibretroSink* audio_sink = nullptr;

static void setup_settings(const char* system_dir) {
    char citra_path[PATH_MAX];
    fill_pathname_join(citra_path, system_dir, "citra", sizeof(citra_path));
    path_mkdir(citra_path);
    FileUtil::SetUserPath(std::string(citra_path) + "/");

    // Map Virtual NAND
    FileUtil::UpdateUserPath(FileUtil::UserPath::NANDDir, std::string(citra_path) + "/nand");
    FileUtil::UpdateUserPath(FileUtil::UserPath::SDMCDir, std::string(citra_path) + "/sdmc");
    FileUtil::UpdateUserPath(FileUtil::UserPath::SysDataDir, std::string(citra_path) + "/sysdata");

    // Populate LLE modules to prevent crashes
    for (const auto& module : Service::service_module_map) {
        Settings::values.lle_modules[module.name] = false;
    }

    // Ensure JIT is disabled globally
    Settings::values.use_cpu_jit.SetValue(false);
    Settings::values.use_shader_jit.SetValue(false);
}

struct MapEntry {
    unsigned retro_id;
    InputManager::ButtonType n3ds_id;
};

static const MapEntry button_map[] = {
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

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;

    bool supports_no_game = true;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &supports_no_game);

    static const struct retro_variable vars[] = {
        { "cytrus_region", "Region; Auto|Japan|USA|Europe|Australia|China|Korea|Taiwan" },
        { "cytrus_model", "System Model; New 3DS|Old 3DS" },
        { "cytrus_audio_emulation", "Audio Emulation; HLE|LLE" },
        { "cytrus_direct_boot", "Direct Boot; enabled|disabled" },
        { NULL, NULL },
    };

    cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);

    static const struct retro_input_descriptor desc[] = {
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "ZL" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "ZR" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
        { 0, 0, 0, 0, NULL }
    };
    cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void*)desc);
}

void retro_set_video_refresh(retro_video_refresh_t cb) {
    video_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb) {
    audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb) {
    input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb) {
    input_state_cb = cb;
}

void retro_init(void) {
    Common::Log::Initialize();
    Common::Log::Start();

    // Set some default settings
    Settings::values.use_cpu_jit.SetValue(false);
    Settings::values.use_shader_jit.SetValue(false);
    Settings::values.graphics_api.SetValue(Settings::GraphicsAPI::Software);
    Settings::values.output_type.SetValue(AudioCore::SinkType::Libretro);

    InputCommon::Init();
    InputManager::Init();
}

void retro_deinit(void) {
    InputManager::Shutdown();
    if (emu_window) {
        delete emu_window;
        emu_window = nullptr;
    }
}

unsigned retro_api_version(void) {
    return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info) {
    info->library_name = "Nintendo - 3DS (Cytrus IR)";
    info->library_version = "v1.0";
    info->valid_extensions = "3ds|3dsx|cia|cci|cxi|app|elf|axf";
    info->need_fullpath = true;
    info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
    info->geometry.base_width = 400;
    info->geometry.base_height = 480;
    info->geometry.max_width = 400;
    info->geometry.max_height = 480;
    info->geometry.aspect_ratio = 400.0f / 480.0f;

    info->timing.fps = 60.0;
    info->timing.sample_rate = 32768.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
}

void retro_reset(void) {
    Core::System::GetInstance().Reset();
}

static void update_variables() {
    struct retro_variable var = {0};

    var.key = "cytrus_region";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (strcmp(var.value, "Auto") == 0) Settings::values.region_value.SetValue(-1);
        else if (strcmp(var.value, "Japan") == 0) Settings::values.region_value.SetValue(0);
        else if (strcmp(var.value, "USA") == 0) Settings::values.region_value.SetValue(1);
        else if (strcmp(var.value, "Europe") == 0) Settings::values.region_value.SetValue(2);
        else if (strcmp(var.value, "Australia") == 0) Settings::values.region_value.SetValue(3);
        else if (strcmp(var.value, "China") == 0) Settings::values.region_value.SetValue(4);
        else if (strcmp(var.value, "Korea") == 0) Settings::values.region_value.SetValue(5);
        else if (strcmp(var.value, "Taiwan") == 0) Settings::values.region_value.SetValue(6);
    }

    var.key = "cytrus_model";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (strcmp(var.value, "New 3DS") == 0) Settings::values.is_new_3ds.SetValue(true);
        else Settings::values.is_new_3ds.SetValue(false);
    }

    var.key = "cytrus_audio_emulation";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (strcmp(var.value, "HLE") == 0) Settings::values.audio_emulation.SetValue(Settings::AudioEmulation::HLE);
        else Settings::values.audio_emulation.SetValue(Settings::AudioEmulation::LLE);
    }
}

static void update_input() {
    input_poll_cb();

    auto* button_handler = InputManager::ButtonHandler();
    for (auto& entry : button_map) {
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, entry.retro_id)) {
            button_handler->PressKey(entry.n3ds_id);
        } else {
            button_handler->ReleaseKey(entry.n3ds_id);
        }
    }

    auto* analog_handler = InputManager::AnalogHandler();
    float lx = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) / 32768.0f;
    float ly = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / 32768.0f;
    analog_handler->MoveJoystick(InputManager::N3DS_CIRCLEPAD, lx, -ly);

    // Pointer (Touch)
    if (input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED)) {
        s16 px = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
        s16 py = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);

        // Convert [-32767, 32767] to [0, 400], [0, 480]
        unsigned fx = (px + 32767) * 400 / 65534;
        unsigned fy = (py + 32767) * 480 / 65534;

        emu_window->TouchPressed(fx, fy);
    } else {
        emu_window->TouchReleased();
    }
}

void retro_run(void) {
    bool updated = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
        update_variables();
    }

    update_input();

    Core::System& system = Core::System::GetInstance();
    if (system.IsPoweredOn()) {
        (void)system.RunLoop();
    }

    if (emu_window) {
        emu_window->SwapBuffers();
        video_cb(emu_window->GetVideoBuffer(), emu_window->GetVideoWidth(), emu_window->GetVideoHeight(), emu_window->GetVideoPitch());
    }

    if (!audio_sink) {
        audio_sink = dynamic_cast<AudioCore::LibretroSink*>(&system.DSP().GetSink());
    }

    if (audio_sink) {
        static constexpr size_t samples_per_frame = 32768 / 60;
        s16 samples[samples_per_frame * 2]; // * 2 for safety
        audio_sink->PullSamples(samples, samples_per_frame);
        audio_batch_cb(samples, samples_per_frame);
    }
}

size_t retro_serialize_size(void) {
    // 3DS states can be large. 64MB should be a safe upper bound for most games.
    return 64 * 1024 * 1024;
}

bool retro_serialize(void *data, size_t len) {
    try {
        namespace io = boost::iostreams;
        io::array_sink sink((char*)data, len);
        io::stream<io::array_sink> os(sink);
        boost::archive::binary_oarchive ar(os);
        ar & Core::System::GetInstance();
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(Core, "retro_serialize failed: {}", e.what());
        return false;
    }
}

bool retro_unserialize(const void *data, size_t len) {
    try {
        namespace io = boost::iostreams;
        io::array_source source((const char*)data, len);
        io::stream<io::array_source> is(source);
        boost::archive::binary_iarchive ar(is);
        ar & Core::System::GetInstance();
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(Core, "retro_unserialize failed: {}", e.what());
        return false;
    }
}

void retro_cheat_reset(void) {
}

void retro_cheat_set(unsigned index, bool enabled, const char *code) {
}

bool retro_load_game(const struct retro_game_info *game) {
    if (!game) return false;

    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) return false;

    if (!emu_window) {
        emu_window = new LibretroEmuWindow();
    }

    update_variables();

    Core::System& system = Core::System::GetInstance();

    const char* system_dir = nullptr;
    if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir) {
        setup_settings(system_dir);
    }

    FileUtil::SetCurrentRomPath(game->path);
    auto status = system.Load(*emu_window, game->path);

    if (status == Core::System::ResultStatus::Success) {
        system.DSP().SetSink(AudioCore::SinkType::Libretro, "");
        audio_sink = dynamic_cast<AudioCore::LibretroSink*>(&system.DSP().GetSink());
        return true;
    }

    return false;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info) {
    return false;
}

void retro_unload_game(void) {
    Core::System::GetInstance().Shutdown();
    audio_sink = nullptr;
}

unsigned retro_get_region(void) {
    return RETRO_REGION_NTSC;
}

void *retro_get_memory_data(unsigned id) {
    return nullptr;
}

size_t retro_get_memory_size(unsigned id) {
    return 0;
}
