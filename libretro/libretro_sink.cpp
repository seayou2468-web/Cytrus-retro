#include "libretro_sink.h"

namespace AudioCore {

LibretroSink::LibretroSink() = default;
LibretroSink::~LibretroSink() = default;

void LibretroSink::SetCallback(std::function<void(s16*, std::size_t)> cb) {
    callback = cb;
}

void LibretroSink::Pull(s16* buffer, std::size_t num_frames) {
    if (callback) {
        callback(buffer, num_frames);
    }
}

} // namespace AudioCore
