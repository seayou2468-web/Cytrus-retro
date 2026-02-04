#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <sstream>
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
#include "core/hle/service/am/am.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/sm/sm.h"
#include "libretro_sink.h"
#include "libretro_input.h"
#include "common/logging/backend.h"
#include "common/file_util.h"
#include "common/archives.h"
#include <streams/file_stream.h>
#include <vfs/vfs_implementation.h>
#include <file/file_path.h>
#include <retro_dirent.h>
#include <string/stdstring.h>

namespace SwRenderer {
void LibretroRenderOptimized(Core::System& system, u32* output_data, u32 output_pitch, bool side_by_side);
}

retro_environment_t g_environ_cb;
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

static bool accel_enabled = false;
static bool gyro_enabled = false;

static const struct retro_subsystem_rom_info update_subsystem_roms[] = {
    { "Game", "3ds|cci|cxi|app", false, false, true, nullptr, 0 },
    { "Update", "cia|firm|luma", false, false, true, nullptr, 0 },
};

static const struct retro_subsystem_info subsystems[] = {
    { "3DS Game with Update", "update", update_subsystem_roms, 2, 1 },
    { nullptr, nullptr, nullptr, 0, 0 },
};

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
    info->library_name = "Cytrus IR";
    info->library_version = "v1.1";
    info->need_fullpath = true;
    info->valid_extensions = "3ds|3dsx|cci|cia|cxi|app|elf|axf|bin|srl";
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
    g_environ_cb = cb;

    struct retro_log_callback log;
    if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log)) {
        log_cb = log.log;
        Common::Log::SetLibretroLogCallback(libretro_log_callback);
    }

    static const struct retro_core_option_v2_category categories[] = {
        { "emulation", "Emulation Settings", "Settings related to CPU/GPU/Region emulation." },
        { "video", "Video Settings", "Settings related to layout and rendering." },
        { nullptr, nullptr, nullptr },
    };

    static const struct retro_core_option_v2_definition definitions[] = {
        {
            "cytrus_model",
            "Console Model",
            nullptr,
            "Select which 3DS model to emulate. New 3DS has more RAM and a faster CPU.",
            nullptr,
            "emulation",
            {
                { "Old 3DS", nullptr },
                { "New 3DS", nullptr },
                { nullptr, nullptr },
            },
            "Old 3DS"
        },
        {
            "cytrus_cpu_clock",
            "CPU Clock Percentage",
            nullptr,
            "Overclock or underclock the CPU. Higher values can improve performance in some games but may cause glitches.",
            nullptr,
            "emulation",
            {
                { "25%", nullptr },
                { "50%", nullptr },
                { "100%", nullptr },
                { "200%", nullptr },
                { "400%", nullptr },
                { nullptr, nullptr },
            },
            "100%"
        },
        {
            "cytrus_region",
            "Console Region",
            nullptr,
            "Select the region of the console. 'Auto' will use the game's region.",
            nullptr,
            "emulation",
            {
                { "Auto", nullptr },
                { "Japan", nullptr },
                { "USA", nullptr },
                { "Europe", nullptr },
                { "Australia", nullptr },
                { "China", nullptr },
                { "Korea", nullptr },
                { "Taiwan", nullptr },
                { nullptr, nullptr },
            },
            "Auto"
        },
        {
            "cytrus_layout",
            "Screen Layout",
            nullptr,
            "Select how the two screens are displayed.",
            nullptr,
            "video",
            {
                { "Vertical", nullptr },
                { "Side-by-Side", nullptr },
                { nullptr, nullptr },
            },
            "Vertical"
        },
        {
            "cytrus_amiibo_path",
            "Amiibo File Path",
            nullptr,
            "Path to an Amiibo .bin file to mount. The file should be in the 'save/cytrus/' directory.",
            nullptr,
            "emulation",
            {
                { "None", nullptr },
                { "amiibo.bin", nullptr },
                { "amiibo0.bin", nullptr },
                { "amiibo1.bin", nullptr },
                { nullptr, nullptr },
            },
            "None"
        },
        { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, { { nullptr, nullptr } }, nullptr }
    };

    struct retro_core_options_v2 opts = { (struct retro_core_option_v2_category*)categories, (struct retro_core_option_v2_definition*)definitions };
    if (!environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &opts)) {
        static const struct retro_variable vars[] = {
            { "cytrus_model", "Console Model; Old 3DS|New 3DS" },
            { "cytrus_layout", "Screen Layout; Vertical|Side-by-Side" },
            { "cytrus_region", "Console Region; Auto|Japan|USA|Europe|Australia|China|Korea|Taiwan" },
            { nullptr, nullptr },
        };
        environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);
    }

    static const struct retro_controller_description controllers[] = {
        { "3DS Pad", RETRO_DEVICE_JOYPAD },
    };
    static const struct retro_controller_info ports[] = {
        { controllers, 1 },
        { nullptr, 0 },
    };
    environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

    environ_cb(RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO, (void*)subsystems);

    bool support_achievements = true;
    environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, &support_achievements);

    bool can_dupe = true;
    environ_cb(RETRO_ENVIRONMENT_GET_CAN_DUPE, &can_dupe);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

static void set_memory_maps() {
    static struct retro_memory_descriptor descriptors[2];
    memset(descriptors, 0, sizeof(descriptors));

    // FCRAM
    descriptors[0].ptr = (uint8_t*)system_instance->Memory().GetFCRAMPointer(0);
    descriptors[0].start = 0x20000000;
    descriptors[0].len = Settings::values.is_new_3ds ? Memory::FCRAM_N3DS_SIZE : Memory::FCRAM_SIZE;
    descriptors[0].addrspace = "physical";

    // VRAM
    descriptors[1].ptr = (uint8_t*)system_instance->Memory().GetPhysicalPointer(Memory::VRAM_PADDR);
    descriptors[1].start = 0x18000000;
    descriptors[1].len = Memory::VRAM_SIZE;
    descriptors[1].addrspace = "physical";

    struct retro_memory_map mmaps = { descriptors, 2 };
    environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &mmaps);
}

bool retro_load_game(const struct retro_game_info *game) {
    if (!game) return false;

    const char* system_dir = nullptr;
    const char* save_dir = nullptr;
    std::string sys_path = "./cytrus/";
    if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir) {
        sys_path = std::string(system_dir) + "/cytrus/";
    }
    FileUtil::SetUserPath(sys_path);
    FileUtil::CreateFullPath(sys_path);
    // User requested font file to be directly in system/cytrus/
    FileUtil::UpdateUserPath(FileUtil::UserPath::SysDataDir, sys_path);

    if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir) {
        std::string user_save_path = std::string(save_dir) + "/cytrus/";
        FileUtil::CreateFullPath(user_save_path);

        std::string nand_path = user_save_path + "nand/";
        FileUtil::CreateFullPath(nand_path);
        FileUtil::UpdateUserPath(FileUtil::UserPath::NANDDir, nand_path);

        std::string sdmc_path = user_save_path + "sdmc/";
        FileUtil::CreateFullPath(sdmc_path);
        FileUtil::UpdateUserPath(FileUtil::UserPath::SDMCDir, sdmc_path);

        std::string config_path = user_save_path + "config/";
        FileUtil::CreateFullPath(config_path);
        FileUtil::UpdateUserPath(FileUtil::UserPath::ConfigDir, config_path);

        std::string cheats_path = user_save_path + "cheats/";
        FileUtil::CreateFullPath(cheats_path);
        FileUtil::UpdateUserPath(FileUtil::UserPath::CheatsDir, cheats_path);
    }

    // Set pixel format
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
        LOG_ERROR(Frontend, "XRGB8888 is not supported.");
        return false;
    }

    struct retro_variable var_model = { "cytrus_model", nullptr };
    Settings::values.is_new_3ds = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var_model) && var_model.value) {
        if (string_is_equal(var_model.value, "New 3DS")) {
            Settings::values.is_new_3ds = true;
        }
    }

    struct retro_variable var_cpu = { "cytrus_cpu_clock", nullptr };
    Settings::values.cpu_clock_percentage.SetValue(100);
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var_cpu) && var_cpu.value) {
        Settings::values.cpu_clock_percentage.SetValue(atoi(var_cpu.value));
    }

    struct retro_variable var_amiibo = { "cytrus_amiibo_path", nullptr };
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var_amiibo) && var_amiibo.value && !string_is_equal(var_amiibo.value, "None")) {
        std::string path = FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "../" + var_amiibo.value;
        auto nfc = system_instance->ServiceManager().GetService<Service::NFC::Module::Interface>("nfc:u");
        if (nfc) {
            nfc->LoadAmiibo(path);
        }
    }

    struct retro_variable var_region = { "cytrus_region", nullptr };
    Settings::values.region_value.SetValue(-1); // Auto
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var_region) && var_region.value) {
        if (string_is_equal(var_region.value, "Japan")) Settings::values.region_value.SetValue(0);
        else if (string_is_equal(var_region.value, "USA")) Settings::values.region_value.SetValue(1);
        else if (string_is_equal(var_region.value, "Europe")) Settings::values.region_value.SetValue(2);
        else if (string_is_equal(var_region.value, "Australia")) Settings::values.region_value.SetValue(3);
        else if (string_is_equal(var_region.value, "China")) Settings::values.region_value.SetValue(4);
        else if (string_is_equal(var_region.value, "Korea")) Settings::values.region_value.SetValue(5);
        else if (string_is_equal(var_region.value, "Taiwan")) Settings::values.region_value.SetValue(6);
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

    set_memory_maps();

    return true;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num_info) {
    if (type != 1 || num_info < 1) return false; // Only support "update" subsystem
    if (!retro_load_game(&info[0])) return false;
    if (num_info > 1) {
        const char* ext = strrchr(info[1].path, '.');
        if (ext && (string_is_equal(ext + 1, "cia") || string_is_equal(ext + 1, "CIA"))) {
            LOG_INFO(Loader, "Installing subsystem update CIA: {}", info[1].path);
            if (Service::AM::InstallCIA(info[1].path) != Service::AM::InstallStatus::Success) {
                LOG_ERROR(Loader, "Failed to install update CIA: {}", info[1].path);
            }
        } else if (ext && (string_is_equal(ext + 1, "firm") || string_is_equal(ext + 1, "luma"))) {
            LOG_INFO(Loader, "Applying CFW update: {}", info[1].path);
            // Just copy it to NAND for now if it's boot.firm/luma
            std::string nand_path = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
            FileUtil::Copy(info[1].path, nand_path + "boot.firm");
        }
    }
    return true;
}

void retro_unload_game(void) {
    if (system_instance) system_instance->Shutdown();
}

static std::string current_amiibo_path;

void retro_run(void) {
    bool updated = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
        struct retro_variable var_amiibo = { "cytrus_amiibo_path", nullptr };
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var_amiibo) && var_amiibo.value) {
            if (current_amiibo_path != var_amiibo.value) {
                current_amiibo_path = var_amiibo.value;
                auto nfc = system_instance->ServiceManager().GetService<Service::NFC::Module::Interface>("nfc:u");
                if (nfc) {
                    if (current_amiibo_path == "None") {
                        nfc->RemoveAmiibo();
                    } else {
                        std::string full_path = FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "../" + current_amiibo_path;
                        nfc->LoadAmiibo(full_path);
                    }
                }
            }
        }

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

    float circle_x = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) / 32768.0f;
    float circle_y = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / -32768.0f;
    Input::LibretroSetAnalog(false, circle_x, circle_y);

    float c_stick_x = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) / 32768.0f;
    float c_stick_y = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / -32768.0f;
    Input::LibretroSetAnalog(true, c_stick_x, c_stick_y);

    if (accel_enabled || gyro_enabled) {
        static struct retro_sensor_interface *sensor_iface;
        if (environ_cb(RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE, &sensor_iface) && sensor_iface) {
            float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
            if (accel_enabled) {
                ax = sensor_iface->get_sensor_input(0, RETRO_SENSOR_ACCELEROMETER_X);
                ay = sensor_iface->get_sensor_input(0, RETRO_SENSOR_ACCELEROMETER_Y);
                az = sensor_iface->get_sensor_input(0, RETRO_SENSOR_ACCELEROMETER_Z);
            }
            if (gyro_enabled) {
                gx = sensor_iface->get_sensor_input(0, RETRO_SENSOR_GYROSCOPE_X);
                gy = sensor_iface->get_sensor_input(0, RETRO_SENSOR_GYROSCOPE_Y);
                gz = sensor_iface->get_sensor_input(0, RETRO_SENSOR_GYROSCOPE_Z);
            }
            Input::LibretroSetMotion(ax, ay, az, gx, gy, gz);
        }
    }

    struct retro_variable var_layout = { "cytrus_layout", nullptr };
    bool side_by_side = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var_layout) && var_layout.value) {
        if (string_is_equal(var_layout.value, "Side-by-Side")) {
            side_by_side = true;
        }
    }

    bool touch_pressed = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
    if (touch_pressed) {
        float tx = (input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) + 32767) / 65535.0f;
        float ty = (input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) + 32767) / 65535.0f;
        if (side_by_side) {
            if (tx >= 400.0f / 720.0f) {
                float btx = (tx - 400.0f / 720.0f) / (320.0f / 720.0f);
                float bty = ty;
                Input::LibretroSetTouch(btx, bty, true);
            } else {
                Input::LibretroSetTouch(0, 0, false);
            }
        } else {
            if (ty >= 0.5f) {
                float bty = (ty - 0.5f) * 2.0f;
                float btx = (tx - 40.0f / 400.0f) / (320.0f / 400.0f);
                Input::LibretroSetTouch(btx, bty, true);
            } else {
                Input::LibretroSetTouch(0, 0, false);
            }
        }
    } else {
        Input::LibretroSetTouch(0, 0, false);
    }

    if (system_instance->RunLoop(true) != Core::System::ResultStatus::Success) {
        LOG_ERROR(Frontend, "RunLoop failed!");
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
        if (combined_fb.size() != (size_t)fb.width * fb.height) combined_fb.assign(fb.width * fb.height, 0xFF000000);
        output_data = combined_fb.data();
        output_pitch = fb.width * sizeof(u32);
    }

    // Single-pass optimized rendering (decode + rotate + layout)
    SwRenderer::LibretroRenderOptimized(*system_instance, (u32*)output_data, output_pitch, side_by_side);

    video_cb(output_data, fb.width, fb.height, output_pitch);

    auto& dsp = system_instance->DSP();
    auto& sink = static_cast<AudioCore::LibretroSink&>(dsp.GetSink());
    s16 audio_buffer[44100 / 50 * 2];
    std::size_t num_frames = 44100 / 60;
    std::size_t pulled = sink.Pull(audio_buffer, num_frames);
    audio_batch_cb(audio_buffer, pulled);
}

void retro_reset(void) { system_instance->RequestReset(); }

size_t retro_serialize_size(void) {
    return Settings::values.is_new_3ds ? 300 * 1024 * 1024 : 160 * 1024 * 1024;
}

bool retro_serialize(void *data, size_t size) {
    if (!system_instance || !system_instance->IsPoweredOn()) return false;
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
    if (!system_instance || !system_instance->IsPoweredOn()) return false;
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
    system_instance->CheatEngine().LoadCheatFile(0);
}

void retro_cheat_set(unsigned index, bool enabled, const char *code) {
}

void* retro_get_memory_data(unsigned id) {
    if (!system_instance || !system_instance->IsPoweredOn()) return nullptr;
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
    if (!system_instance || !system_instance->IsPoweredOn()) return 0;
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

void retro_set_sensor_state(unsigned port, enum retro_sensor_action action, unsigned rate) {
    switch (action) {
        case RETRO_SENSOR_ACCELEROMETER_ENABLE:
            accel_enabled = true;
            break;
        case RETRO_SENSOR_ACCELEROMETER_DISABLE:
            accel_enabled = false;
            break;
        case RETRO_SENSOR_GYROSCOPE_ENABLE:
            gyro_enabled = true;
            break;
        case RETRO_SENSOR_GYROSCOPE_DISABLE:
            gyro_enabled = false;
            break;
        default:
            break;
    }
}
