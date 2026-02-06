#pragma once
#include <cstdint>

namespace Common::Log {
    enum class Level : uint8_t {
        Trace, Debug, Info, Warning, Error, Critical, Count
    };
    enum class Class : uint8_t {
        Log, Common, Common_Filesystem, Common_Memory, Core, Core_ARM11,
        Core_Timing, Core_Cheats, Config, Savestate, Debug, Debug_Emulated,
        Debug_GPU, Debug_Breakpoint, Debug_GDBStub, Kernel, Kernel_SVC,
        Applet, Applet_SWKBD, Service, Service_SRV, Service_FRD, Service_FS,
        Service_ERR, Service_ACT, Service_APT, Service_BOSS, Service_GSP,
        Service_AC, Service_AM, Service_PTM, Service_LDR, Service_MIC,
        Service_NDM, Service_NFC, Service_NIM, Service_NS, Service_NWM,
        Service_CAM, Service_CECD, Service_CFG, Service_CSND, Service_DSP,
        Service_DLP, Service_HID, Service_HTTP, Service_SOC, Service_IR,
        Service_Y2R, Service_PS, Service_PLGLDR, Service_NEWS, HW, HW_Memory,
        HW_LCD, HW_GPU, HW_AES, HW_RSA, HW_ECC, Frontend, Render,
        Render_Software, Render_OpenGL, Render_Vulkan, Audio, Audio_DSP,
        Audio_Sink, Audio_Source, Audio_Bitrate, Audio_Buffer, Web_Service,
        Count
    };

    template<typename... Args>
    void FmtLogMessage(Class log_class, Level log_level, const char* filename, unsigned int line_num, const char* function, const char* format, const Args&... args) {
        // Do nothing
    }
}

#define LOG_TRACE(log_class, ...)
#define LOG_DEBUG(log_class, ...)
#define LOG_INFO(log_class, ...)
#define LOG_WARNING(log_class, ...)
#define LOG_ERROR(log_class, ...)
#define LOG_CRITICAL(log_class, ...)

// Also define Config so it can be used as Class::Config if needed,
// but LOG_INFO(Config, ...) usually expects Config to be a member of Class
using Config = Common::Log::Class;
