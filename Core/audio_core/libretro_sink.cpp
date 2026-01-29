#include <functional>
#include "audio_core/libretro_sink.h"
#include "common/logging/log.h"

namespace AudioCore {

std::function<void(s16*, std::size_t)> g_fill_callback;

LibretroSink::LibretroSink(std::string_view device_id) {
    LOG_INFO(Audio_Sink, "LibretroSink initialized");
}

LibretroSink::~LibretroSink() {
    g_fill_callback = nullptr;
}

void LibretroSink::SetCallback(std::function<void(s16*, std::size_t)> cb) {
    callback = cb;
    g_fill_callback = cb;
}

void LibretroSink::OutputSamples(s16* samples, std::size_t num_samples) {
    // This could be used for a push model if we wanted
}

std::vector<std::string> ListLibretroSinkDevices() {
    return {"Libretro"};
}

} // namespace AudioCore
