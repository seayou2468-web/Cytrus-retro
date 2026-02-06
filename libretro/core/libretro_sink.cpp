#include "libretro_sink.h"
#include <vector>

extern retro_audio_sample_batch_t audio_batch_cb;

namespace AudioCore {

void LibretroSink::Flush() {
    if (!callback || !audio_batch_cb) return;

    static std::vector<s16> buffer(2048 * 2);
    // Request some samples from DspInterface
    callback(buffer.data(), 1024);
    audio_batch_cb(buffer.data(), 1024);
}

} // namespace AudioCore
