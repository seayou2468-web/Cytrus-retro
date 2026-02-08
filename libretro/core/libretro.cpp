#include "libretro.h"
#include "core/core.h"
#include "audio_core/dsp_interface.h"
#include "core/hle/service/hid/hid.h"
#include "common/logging/log.h"
#include "common/logging/backend.h"
#include "common/settings.h"
#include "common/file_util.h"
#include "core/system_titles.h"
#include "core/hw/unique_data.h"
#include "libretro_emu_window.h"
#include "libretro_sink.h"
#include "libretro_input.h"
#include <iostream>
#include <chrono>

retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
retro_input_state_t input_state_cb;
static retro_environment_t environ_cb;

static std::unique_ptr<Frontend::LibretroEmuWindow> emu_window;

void libretro_log_cb(int level, const char* fmt, ...) {
    if (log_cb) {
        va_list args;
        va_start(args, fmt);
        log_cb((retro_log_level)level, fmt, args);
        va_end(args);
    }
}

void retro_init(void) {
    Common::Log::Initialize("");
    Common::Log::SetLibretroLogCallback(libretro_log_cb);
    Common::Log::Start();
    Input::RegisterLibretroInputFactories();
}

void retro_deinit(void) {
    Core::System::GetInstance().Shutdown();
    emu_window.reset();
    Common::Log::Stop();
}

unsigned retro_api_version(void) {
    return RETRO_API_VERSION;
}

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;
    struct retro_log_callback log;
    if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
        log_cb = log.log;

    static const struct retro_core_option_definition definitions[] = {
        {
            "cytrus_network_provider",
            "Network Provider",
            "Select the Nintendo Network alternative service.",
            {
                { "nintendo", "Nintendo" },
                { "pretendo", "Pretendo" },
                { "plustendo", "Plustendo" },
                { NULL, NULL },
            },
            "nintendo"
        },
        {
            "cytrus_miiverse_provider",
            "Miiverse Provider",
            "Select the Miiverse clone service.",
            {
                { "none", "None" },
                { "juxtaposition", "Juxtaposition" },
                { NULL, NULL },
            },
            "none"
        },
        {
            "cytrus_ssl_verification",
            "SSL Verification",
            "Enable or disable SSL certificate verification.",
            {
                { "enabled", "Enabled" },
                { "disabled", "Disabled" },
                { NULL, NULL },
            },
            "enabled"
        },
        {
            "cytrus_region",
            "Region",
            "Select the console region.",
            {
                { "japan", "Japan" },
                { "auto", "Auto" },
                { "usa", "USA" },
                { "europe", "Europe" },
                { "australia", "Australia" },
                { "china", "China" },
                { "korea", "Korea" },
                { "taiwan", "Taiwan" },
                { NULL, NULL },
            },
            "japan"
        },
        {
            "cytrus_model",
            "System Model",
            "Select the console model.",
            {
                { "new_3ds", "New 3DS" },
                { "old_3ds", "Old 3DS" },
                { NULL, NULL },
            },
            "new_3ds"
        },
        {
            "cytrus_boot_home_menu",
            "Boot Home Menu",
            "Boot to Home Menu if available in NAND.",
            {
                { "enabled", "Enabled" },
                { "disabled", "Disabled" },
                { NULL, NULL },
            },
            "enabled"
        },
        {
            "cytrus_audio_emulation",
            "Audio Emulation",
            "Select the audio emulation mode.",
            {
                { "hle", "HLE" },
                { "lle", "LLE" },
                { NULL, NULL },
            },
            "hle"
        },
        { NULL, NULL, NULL, { { NULL, NULL } }, NULL },
    };

    cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, (void*)definitions);
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

void retro_get_system_info(struct retro_system_info *info) {
    info->library_name     = "Cytrus IR";
    info->library_version  = "v1.0";
    info->need_fullpath    = true;
    info->valid_extensions = "3ds|cia|cci|cxi|app";
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
    info->geometry.base_width   = 400;
    info->geometry.base_height  = 480;
    info->geometry.max_width    = 400;
    info->geometry.max_height   = 480;
    info->geometry.aspect_ratio = 400.0f / 480.0f;
    info->timing.fps            = 60.0; // 3DS is ~59.82Hz
    info->timing.sample_rate    = 48000.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
    (void)port;
    (void)device;
}

void retro_reset(void) {
    Core::System::GetInstance().Reset();
}

void retro_run(void) {
    input_poll_cb();

    auto& system = Core::System::GetInstance();

    s64 starting_ticks = system.CoreTiming().GetGlobalTicks();

    if (emu_window) {
        emu_window->ResetFrameDone();

        // Run until GPU triggers VBlank (SwapBuffers)
        while (!emu_window->IsFrameDone()) {
            system.RunLoop();
        }
    } else {
        system.RunLoop();
    }

    s64 ending_ticks = system.CoreTiming().GetGlobalTicks();
    u64 ticks_passed = (u64)(ending_ticks - starting_ticks);

    // Audio Integration: Use emulated cycles to determine how many samples to submit to Libretro.
    auto& dsp = system.DSP();
    static_cast<AudioCore::LibretroSink&>(dsp.GetSink()).Flush(ticks_passed);

    if (emu_window) {
        video_cb(emu_window->GetFramebuffer(), emu_window->GetWidth(), emu_window->GetHeight(), emu_window->GetWidth() * sizeof(u32));
    }
}

size_t retro_serialize_size(void) {
    return 64 * 1024 * 1024;
}

bool retro_serialize(void *data, size_t size) {
    try {
        auto buffer = Core::System::GetInstance().SaveStateToBuffer();
        if (buffer.empty() || buffer.size() > size) return false;
        memcpy(data, buffer.data(), buffer.size());
        return true;
    } catch (...) {
        return false;
    }
}

bool retro_unserialize(const void *data, size_t size) {
    try {
        return Core::System::GetInstance().LoadStateFromBuffer({reinterpret_cast<const u8*>(data), size});
    } catch (...) {
        return false;
    }
}

void retro_cheat_reset(void) {}

void retro_cheat_set(unsigned index, bool enabled, const char *code) {
    (void)index;
    (void)enabled;
    (void)code;
}

static void setup_settings() {
    Settings::values.graphics_api.SetValue(Settings::GraphicsAPI::Software);

    char system_dir[1024];
    const char* dir = nullptr;
    if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir) {
        snprintf(system_dir, sizeof(system_dir), "%s/cytrus", dir);
        FileUtil::SetUserPath(system_dir);

        // Auto-setup NAND structure
        std::string nand_dir = std::string(system_dir) + "/nand";
        if (!FileUtil::Exists(nand_dir)) {
            FileUtil::CreateDir(nand_dir);
            std::string system_id = "00000000000000000000000000000000";
            FileUtil::CreateFullPath(nand_dir + "/" + system_id + "/title");
            FileUtil::CreateFullPath(nand_dir + "/data/" + system_id + "/extdata");
            FileUtil::CreateFullPath(nand_dir + "/data/" + system_id + "/sysdata");
            FileUtil::CreateDir(nand_dir + "/private");
            FileUtil::CreateDir(nand_dir + "/ro");
            FileUtil::CreateDir(nand_dir + "/sysdata");
            FileUtil::CreateFullPath(nand_dir + "/dbs");

            std::string ticket_db_path = nand_dir + "/dbs/ticket.db";
            if (!FileUtil::Exists(ticket_db_path)) {
                FileUtil::WriteStringToFile(false, ticket_db_path, "");
            }
            std::string title_db_path = nand_dir + "/dbs/title.db";
            if (!FileUtil::Exists(title_db_path)) {
                FileUtil::WriteStringToFile(false, title_db_path, "");
            }
        }

        // Auto-setup SDMC structure
        std::string sdmc_dir = std::string(system_dir) + "/sdmc";
        if (!FileUtil::Exists(sdmc_dir)) {
            FileUtil::CreateDir(sdmc_dir);
            std::string system_id = "00000000000000000000000000000000";
            std::string sd_id = "00000000000000000000000000000000";
            std::string sdmc_base_dir = sdmc_dir + "/Nintendo 3DS/" + system_id + "/" + sd_id;
            FileUtil::CreateFullPath(sdmc_base_dir + "/title");
            FileUtil::CreateFullPath(sdmc_base_dir + "/extdata");
        }

        // Create keys template if missing
        std::string keys_path = std::string(system_dir) + "/sysdata/keys.txt";
        if (!FileUtil::Exists(keys_path)) {
            std::string keys_template =
                "# Cytrus Keys Template\n"
                "[AES]\n"
                "# slot0x11KeyX = ...\n"
                "# slot0x1DKeyX = ...\n"
                "[RSA]\n"
                "# ticketWrapExp = ...\n";
            FileUtil::WriteStringToFile(true, keys_path, keys_template);
        }
    }

    struct retro_variable var;

    var.key = "cytrus_network_provider";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (strcmp(var.value, "pretendo") == 0) Settings::values.network_provider.SetValue(1);
        else if (strcmp(var.value, "plustendo") == 0) Settings::values.network_provider.SetValue(2);
        else Settings::values.network_provider.SetValue(0);
    }

    var.key = "cytrus_miiverse_provider";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (strcmp(var.value, "juxtaposition") == 0) Settings::values.miiverse_provider.SetValue(1);
        else Settings::values.miiverse_provider.SetValue(0);
    }

    var.key = "cytrus_ssl_verification";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (strcmp(var.value, "disabled") == 0) Settings::values.ssl_verification.SetValue(1);
        else Settings::values.ssl_verification.SetValue(0);
    }

    var.key = "cytrus_audio_emulation";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (strcmp(var.value, "lle") == 0) Settings::values.audio_emulation.SetValue(Settings::AudioEmulation::LLE);
        else Settings::values.audio_emulation.SetValue(Settings::AudioEmulation::HLE);
    }

    var.key = "cytrus_region";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (strcmp(var.value, "japan") == 0) Settings::values.region_value.SetValue(HW::UniqueData::Region::JPN);
        else if (strcmp(var.value, "usa") == 0) Settings::values.region_value.SetValue(HW::UniqueData::Region::USA);
        else if (strcmp(var.value, "europe") == 0) Settings::values.region_value.SetValue(HW::UniqueData::Region::EUR);
        else if (strcmp(var.value, "australia") == 0) Settings::values.region_value.SetValue(HW::UniqueData::Region::AUS);
        else if (strcmp(var.value, "china") == 0) Settings::values.region_value.SetValue(HW::UniqueData::Region::CHN);
        else if (strcmp(var.value, "korea") == 0) Settings::values.region_value.SetValue(HW::UniqueData::Region::KOR);
        else if (strcmp(var.value, "taiwan") == 0) Settings::values.region_value.SetValue(HW::UniqueData::Region::TWN);
        else Settings::values.region_value.SetValue(Settings::REGION_VALUE_AUTO_SELECT);
    }

    var.key = "cytrus_model";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (strcmp(var.value, "old_3ds") == 0) Settings::values.is_new_3ds.SetValue(false);
        else Settings::values.is_new_3ds.SetValue(true);
    }

    // Setup input profile for libretro
    auto& profile = Settings::values.current_input_profile;
    profile.buttons[Settings::NativeButton::A] = "engine:libretro,port:0,id:8"; // RETRO_DEVICE_ID_JOYPAD_A
    profile.buttons[Settings::NativeButton::B] = "engine:libretro,port:0,id:0"; // RETRO_DEVICE_ID_JOYPAD_B
    profile.buttons[Settings::NativeButton::X] = "engine:libretro,port:0,id:9"; // RETRO_DEVICE_ID_JOYPAD_X
    profile.buttons[Settings::NativeButton::Y] = "engine:libretro,port:0,id:1"; // RETRO_DEVICE_ID_JOYPAD_Y
    profile.buttons[Settings::NativeButton::Up] = "engine:libretro,port:0,id:2";
    profile.buttons[Settings::NativeButton::Down] = "engine:libretro,port:0,id:3";
    profile.buttons[Settings::NativeButton::Left] = "engine:libretro,port:0,id:4";
    profile.buttons[Settings::NativeButton::Right] = "engine:libretro,port:0,id:5";
    profile.buttons[Settings::NativeButton::L] = "engine:libretro,port:0,id:6";
    profile.buttons[Settings::NativeButton::R] = "engine:libretro,port:0,id:7";
    profile.buttons[Settings::NativeButton::Start] = "engine:libretro,port:0,id:11";
    profile.buttons[Settings::NativeButton::Select] = "engine:libretro,port:0,id:10";

    profile.analogs[Settings::NativeAnalog::CirclePad] = "engine:libretro,port:0,index:0,id_x:0,id_y:1";
    profile.touch_device = "engine:libretro";
}

bool retro_load_game(const struct retro_game_info *game) {
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) return false;

    setup_settings();

    emu_window = std::make_unique<Frontend::LibretroEmuWindow>();

    auto& system = Core::System::GetInstance();

    bool boot_home_menu = true;
    struct retro_variable var;
    var.key = "cytrus_boot_home_menu";
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        boot_home_menu = (strcmp(var.value, "enabled") == 0);
    }

    std::string path_to_load = game->path;
    bool using_home_menu = false;
    if (boot_home_menu) {
        s32 region = Settings::values.region_value.GetValue();
        if (region == Settings::REGION_VALUE_AUTO_SELECT) {
            for (u32 i = 0; i < Core::NUM_SYSTEM_TITLE_REGIONS; i++) {
                std::string p = Core::GetHomeMenuNcchPath(i);
                if (!p.empty() && FileUtil::Exists(p)) {
                    path_to_load = p;
                    using_home_menu = true;
                    break;
                }
            }
        } else {
            std::string p = Core::GetHomeMenuNcchPath(static_cast<u32>(region));
            if (!p.empty() && FileUtil::Exists(p)) {
                path_to_load = p;
                using_home_menu = true;
            }
        }
    }

    if (system.Load(*emu_window, path_to_load) != Core::System::ResultStatus::Success) {
        return false;
    }

    if (using_home_menu) {
        system.SetCartridge(game->path);
    }

    auto& dsp = system.DSP();
    static_cast<AudioCore::LibretroSink&>(dsp.GetSink()).SetLibretroCallback(audio_batch_cb);

    return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info) {
    (void)game_type;
    (void)info;
    (void)num_info;
    return false;
}

void retro_unload_game(void) {
    Core::System::GetInstance().Shutdown();
}

unsigned retro_get_region(void) {
    return RETRO_REGION_NTSC;
}

void *retro_get_memory_data(unsigned id) {
    (void)id;
    return NULL;
}

size_t retro_get_memory_size(unsigned id) {
    (void)id;
    return 0;
}
