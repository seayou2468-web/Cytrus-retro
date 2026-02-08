// Copyright 2024 Jules
// Licensed under GPLv2 or any later version

#include "libretro_sink.h"
#include "common/common_types.h"
#include <array>
#include <cmath>

namespace AudioCore {

void LibretroSink::Flush(u64 ticks_passed) {
    if (!callback || !audio_batch_cb) return;

    // CPU clock: 268,111,856 Hz
    // Output sample rate: 48,000 Hz
    const double samples_to_pull_double = (double)ticks_passed * 48000.0 / 268111856.0 + sample_accumulator;
    size_t frames_to_pull = (size_t)std::floor(samples_to_pull_double);
    sample_accumulator = samples_to_pull_double - (double)frames_to_pull;

    if (frames_to_pull == 0) return;

    // Limit buffer size
    static std::vector<s16> buffer;
    if (buffer.size() < frames_to_pull * 2) {
        buffer.resize(frames_to_pull * 2);
    }

    callback(buffer.data(), frames_to_pull);
    audio_batch_cb(buffer.data(), frames_to_pull);
}

} // namespace AudioCore
