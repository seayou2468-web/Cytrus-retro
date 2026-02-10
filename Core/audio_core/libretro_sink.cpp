#include "audio_core/libretro_sink.h"
#include "audio_core/audio_types.h"
#include <cstring>

namespace AudioCore {

LibretroSink::LibretroSink() = default;
LibretroSink::~LibretroSink() = default;

unsigned int LibretroSink::GetNativeSampleRate() const {
    return native_sample_rate;
}

void LibretroSink::SetCallback(std::function<void(s16*, std::size_t)> cb) {
    callback = std::move(cb);
}

void LibretroSink::Drain(std::function<void(s16*, std::size_t)> push_cb) {
    if (!callback) return;

    // Libretro usually expects a certain amount of samples per frame.
    // 32728 / 60 is about 545.46 samples.
    // We'll pull 512 samples for now, or we could calculate exactly.
    s16 buffer[2048]; // Max 1024 stereo frames
    std::size_t frames_to_pull = 512;

    callback(buffer, frames_to_pull);
    push_cb(buffer, frames_to_pull);
}

std::vector<std::string> ListLibretroSinkDevices() {
    return {"Libretro"};
}

} // namespace AudioCore
