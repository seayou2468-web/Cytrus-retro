#include <iostream>
#include <string>
#include <vector>
#include <mutex>

#include "libretro.h"
#include "libretro_vulkan.h"
#include "libretro_emu_window.h"
#include "libretro_input.h"

#include "core/core.h"
#include "common/settings.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/logging/backend.h"
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include "core/loader/loader.h"
#include "audio_core/libretro_sink.h"
#include "audio_core/dsp_interface.h"

static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_environment_t environ_cb;

static LibretroEmuWindow* emu_window = nullptr;
static struct retro_hw_render_interface_vulkan* vk_interface = nullptr;

static void get_geometry(u32* width, u32* height, float* aspect) {
    u32 base_w = 400;
    u32 base_h = 240;
    u32 factor = Settings::values.resolution_factor.GetValue();

    switch (Settings::values.layout_option.GetValue()) {
    case Settings::LayoutOption::Default:
        base_w = 400;
        base_h = 480;
        break;
    case Settings::LayoutOption::SideScreen:
        base_w = 720;
        base_h = 240;
        break;
    case Settings::LayoutOption::SingleScreen:
        if (Settings::values.swap_screen.GetValue()) {
            base_w = 320;
            base_h = 240;
        } else {
            base_w = 400;
            base_h = 240;
        }
        break;
    default:
        base_w = 400;
        base_h = 480;
        break;
    }

    *width = base_w * factor;
    *height = base_h * factor;
    if (aspect)
        *aspect = (float)base_w / (float)base_h;
}

static void setup_paths() {
    const char* system_dir = nullptr;
    if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir) {
        char base_dir[PATH_MAX_LENGTH];
        fill_pathname_join(base_dir, system_dir, "citra", sizeof(base_dir));
        path_mkdir(base_dir);

        FileUtil::SetUserPath(base_dir);

        char path[PATH_MAX_LENGTH];

        fill_pathname_join(path, base_dir, "nand", sizeof(path));
        fill_pathname_slash(path, sizeof(path));
        FileUtil::UpdateUserPath(FileUtil::UserPath::NANDDir, path);

        fill_pathname_join(path, base_dir, "sysdata", sizeof(path));
        fill_pathname_slash(path, sizeof(path));
        FileUtil::UpdateUserPath(FileUtil::UserPath::SysDataDir, path);

        fill_pathname_join(path, base_dir, "sdmc", sizeof(path));
        fill_pathname_slash(path, sizeof(path));
        FileUtil::UpdateUserPath(FileUtil::UserPath::SDMCDir, path);
    }
}

static void setup_core_options() {
    static const struct retro_core_option_v2_definition option_defs[] = {
        {
            "cytrus_cpu_clock_percentage",
            "CPU Clock Percentage",
            "Change the CPU clock percentage.",
            {
                {"100", nullptr},
                {"200", nullptr},
                {"400", nullptr},
                {"50", nullptr},
                {"25", nullptr},
            },
            "100"
        },
        {
            "cytrus_region",
            "System Region",
            "Select the system region.",
            {
                {"Auto", nullptr},
                {"Japan", nullptr},
                {"USA", nullptr},
                {"Europe", nullptr},
                {"Australia", nullptr},
                {"China", nullptr},
                {"Korea", nullptr},
                {"Taiwan", nullptr},
            },
            "Auto"
        },
        {
            "cytrus_is_new_3ds",
            "New 3DS mode",
            "Enable New 3DS features.",
            {
                {"enabled", nullptr},
                {"disabled", nullptr},
            },
            "enabled"
        },
        {
            "cytrus_layout",
            "Screen Layout",
            "Choose how to display the 3DS screens.",
            {
                {"Stacked", "Stacked"},
                {"Side-by-Side", "Side-by-Side"},
                {"Single Top", "Single Top"},
                {"Single Bottom", "Single Bottom"},
            },
            "Stacked"
        },
        {
            "cytrus_swap_screens",
            "Swap Screens",
            "Swap the positions of the top and bottom screens.",
            {
                {"disabled", nullptr},
                {"enabled", nullptr},
            },
            "disabled"
        },
        {
            "cytrus_resolution_factor",
            "Resolution Factor",
            "Internal rendering resolution multiplier.",
            {
                {"1", nullptr},
                {"2", nullptr},
                {"3", nullptr},
                {"4", nullptr},
                {"5", nullptr},
                {"6", nullptr},
                {"7", nullptr},
                {"8", nullptr},
                {"9", nullptr},
                {"10", nullptr},
            },
            "1"
        },
        { nullptr }
    };

    struct retro_core_options_v2 options = {
        (struct retro_core_option_v2_definition*)option_defs,
        nullptr
    };

    environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &options);
}

static bool options_changed = false;
static void update_core_options() {
    struct retro_variable var;
    options_changed = false;

    var.key = "cytrus_cpu_clock_percentage";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        Settings::values.cpu_clock_percentage.SetValue(std::stoi(var.value));
    }

    var.key = "cytrus_region";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        int region = -1;
        if (std::string(var.value) == "Japan") region = 0;
        else if (std::string(var.value) == "USA") region = 1;
        else if (std::string(var.value) == "Europe") region = 2;
        else if (std::string(var.value) == "Australia") region = 3;
        else if (std::string(var.value) == "China") region = 4;
        else if (std::string(var.value) == "Korea") region = 5;
        else if (std::string(var.value) == "Taiwan") region = 6;
        Settings::values.region_value.SetValue(region);
    }

    var.key = "cytrus_is_new_3ds";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        Settings::values.is_new_3ds.SetValue(std::string(var.value) == "enabled");
    }

    var.key = "cytrus_layout";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        Settings::LayoutOption new_opt = Settings::LayoutOption::Default;
        bool swap = false;
        if (std::string(var.value) == "Stacked")
            new_opt = Settings::LayoutOption::Default;
        else if (std::string(var.value) == "Side-by-Side")
            new_opt = Settings::LayoutOption::SideScreen;
        else if (std::string(var.value) == "Single Top")
            new_opt = Settings::LayoutOption::SingleScreen;
        else if (std::string(var.value) == "Single Bottom") {
            new_opt = Settings::LayoutOption::SingleScreen;
            swap = true;
        }

        if (Settings::values.layout_option.GetValue() != new_opt || Settings::values.swap_screen.GetValue() != swap) {
            Settings::values.layout_option.SetValue(new_opt);
            Settings::values.swap_screen.SetValue(swap);
            options_changed = true;
        }
    }

    var.key = "cytrus_swap_screens";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        bool swap = std::string(var.value) == "enabled";
        if (Settings::values.swap_screen.GetValue() != swap) {
            Settings::values.swap_screen.SetValue(swap);
            options_changed = true;
        }
    }

    var.key = "cytrus_resolution_factor";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        int factor = std::stoi(var.value);
        if (Settings::values.resolution_factor.GetValue() != (u32)factor) {
            Settings::values.resolution_factor.SetValue(factor);
            options_changed = true;
        }
    }

    if (options_changed && emu_window) {
        emu_window->OnFramebufferSizeChanged();
        struct retro_system_av_info av_info;
        retro_get_system_av_info(&av_info);
        environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &av_info.geometry);
    }
}

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;

    struct retro_log_callback log;
    if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log)) {
        log_cb = log.log;
        Common::Log::SetLibretroLogCallback(log_cb);
    }
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_init(void) {
    Common::Log::Initialize();
    Common::Log::Start();
    LibretroInput::InputManager::GetInstance().RegisterFactories();
}

void retro_deinit(void) {
    if (emu_window) {
        delete emu_window;
        emu_window = nullptr;
    }
}

unsigned retro_api_version(void) {
    return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info) {
    info->library_name = "Cytrus IR";
    info->library_version = "v1.0";
    info->valid_extensions = "3ds|3dsx|cia|cci|cxi|app|elf|axf";
    info->need_fullpath = true;
    info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
    u32 w, h;
    float aspect;
    get_geometry(&w, &h, &aspect);

    info->geometry.base_width = w;
    info->geometry.base_height = h;
    info->geometry.max_width = 400 * 10;  // Max resolution factor is 10
    info->geometry.max_height = 480 * 10;
    info->geometry.aspect_ratio = aspect;
    info->timing.fps = 60.0;
    info->timing.sample_rate = 32728.0;
}

static void context_reset(void) {
    if (environ_cb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, &vk_interface) && vk_interface) {
        if (emu_window) {
            emu_window->SetLibretroVulkanContext(vk_interface);
        }
    }
}

static void context_destroy(void) {
    vk_interface = nullptr;
    if (emu_window) {
        emu_window->SetLibretroVulkanContext(nullptr);
    }
}

bool retro_load_game(const struct retro_game_info *game) {
    if (!game) return false;

    setup_paths();

    // Set defaults
    Settings::values.layout_option.SetValue(Settings::LayoutOption::Default);
    Settings::values.swap_screen.SetValue(false);

    setup_core_options();
    update_core_options();

    // Configure Citra settings for Libretro
    Settings::values.graphics_api.SetValue(Settings::GraphicsAPI::Vulkan);
    Settings::values.audio_emulation.SetValue(Settings::AudioEmulation::HLE);
    Settings::values.output_type.SetValue(AudioCore::SinkType::Libretro);

    // Input setup
    Settings::values.current_input_profile.buttons[Settings::NativeButton::A] = "engine:libretro,id:0";
    Settings::values.current_input_profile.buttons[Settings::NativeButton::B] = "engine:libretro,id:1";
    Settings::values.current_input_profile.buttons[Settings::NativeButton::X] = "engine:libretro,id:2";
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Y] = "engine:libretro,id:3";
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Up] = "engine:libretro,id:4";
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Down] = "engine:libretro,id:5";
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Left] = "engine:libretro,id:6";
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Right] = "engine:libretro,id:7";
    Settings::values.current_input_profile.buttons[Settings::NativeButton::L] = "engine:libretro,id:8";
    Settings::values.current_input_profile.buttons[Settings::NativeButton::R] = "engine:libretro,id:9";
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Start] = "engine:libretro,id:10";
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Select] = "engine:libretro,id:11";
    Settings::values.current_input_profile.buttons[Settings::NativeButton::ZL] = "engine:libretro,id:12";
    Settings::values.current_input_profile.buttons[Settings::NativeButton::ZR] = "engine:libretro,id:13";

    Settings::values.current_input_profile.analogs[Settings::NativeAnalog::CirclePad] = "engine:libretro,id:0";
    Settings::values.current_input_profile.analogs[Settings::NativeAnalog::CStick] = "engine:libretro,id:1";
    Settings::values.current_input_profile.touch_device = "engine:libretro";

    static struct retro_hw_render_callback hw_render;
    hw_render.context_type = RETRO_HW_CONTEXT_VULKAN;
    hw_render.version_major = 1;
    hw_render.version_minor = 0;
    hw_render.context_reset = context_reset;
    hw_render.context_destroy = context_destroy;
    hw_render.cache_context = true;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) return false;

    emu_window = new LibretroEmuWindow();

    auto& system = Core::System::GetInstance();
    if (system.Load(*emu_window, game->path) != Core::System::ResultStatus::Success) {
        return false;
    }

    return true;
}

void retro_run(void) {
    bool updated = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
        update_core_options();
    }

    input_poll_cb();

    auto& input_manager = LibretroInput::InputManager::GetInstance();
    // Buttons
    input_manager.SetButton(0, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A));
    input_manager.SetButton(1, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B));
    input_manager.SetButton(2, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X));
    input_manager.SetButton(3, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y));
    input_manager.SetButton(4, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP));
    input_manager.SetButton(5, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN));
    input_manager.SetButton(6, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT));
    input_manager.SetButton(7, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT));
    input_manager.SetButton(8, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L));
    input_manager.SetButton(9, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R));
    input_manager.SetButton(10, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START));
    input_manager.SetButton(11, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT));
    input_manager.SetButton(12, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2));
    input_manager.SetButton(13, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2));

    // Analogs
    float clx = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) / 32768.0f;
    float cly = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / 32768.0f;
    input_manager.SetAnalog(0, clx, -cly);

    float rlx = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) / 32768.0f;
    float rly = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / 32768.0f;
    input_manager.SetAnalog(1, rlx, -rly);

    // Pointer/Touch
    int16_t tx = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
    int16_t ty = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);
    bool pressed = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
    float norm_x = (tx + 32767) / 65534.0f;
    float norm_y = (ty + 32767) / 65534.0f;

    float touch_x = 0, touch_y = 0;
    bool touch_pressed = false;

    bool swapped = Settings::values.swap_screen.GetValue();
    switch (Settings::values.layout_option.GetValue()) {
    case Settings::LayoutOption::Default:
        if (!swapped) {
            if (norm_y >= 0.5f) {
                touch_x = norm_x;
                touch_y = (norm_y - 0.5f) * 2.0f;
                touch_pressed = pressed;
            }
        } else {
            if (norm_y < 0.5f) {
                touch_x = norm_x;
                touch_y = norm_y * 2.0f;
                touch_pressed = pressed;
            }
        }
        break;
    case Settings::LayoutOption::SideScreen:
        if (!swapped) {
            // Top(400) | Bottom(320). Total 720.
            if (norm_x >= (400.0f / 720.0f)) {
                touch_x = (norm_x - (400.0f / 720.0f)) * (720.0f / 320.0f);
                touch_y = norm_y;
                touch_pressed = pressed;
            }
        } else {
            // Bottom(320) | Top(400). Total 720.
            if (norm_x < (320.0f / 720.0f)) {
                touch_x = norm_x * (720.0f / 320.0f);
                touch_y = norm_y;
                touch_pressed = pressed;
            }
        }
        break;
    case Settings::LayoutOption::SingleScreen:
        if (swapped) {
            // Only bottom screen shown
            touch_x = norm_x;
            touch_y = norm_y;
            touch_pressed = pressed;
        }
        break;
    default:
        break;
    }
    input_manager.SetTouch(touch_x, touch_y, touch_pressed);

    auto& system = Core::System::GetInstance();
    system.RunLoop();

    u32 w, h;
    get_geometry(&w, &h, nullptr);

    // Audio
    auto& dsp = system.DSP();
    auto* sink = static_cast<AudioCore::LibretroSink*>(&dsp.GetSink());
    sink->Drain([](s16* buffer, std::size_t frames) {
        audio_batch_cb(buffer, frames);
    });

    // Video
    video_cb(RETRO_HW_FRAME_BUFFER_VALID, w, h, 0);
}

void retro_reset(void) {
    Core::System::GetInstance().Reset();
}

void retro_unload_game(void) {
    Core::System::GetInstance().Shutdown();
}

size_t retro_serialize_size(void) {
    return 0; // TODO
}

bool retro_serialize(void *data, size_t size) {
    return false;
}

bool retro_unserialize(const void *data, size_t size) {
    return false;
}

void retro_set_controller_port_device(unsigned port, unsigned device) {}
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) {}
bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info) { return false; }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned id) { return nullptr; }
size_t retro_get_memory_size(unsigned id) { return 0; }
