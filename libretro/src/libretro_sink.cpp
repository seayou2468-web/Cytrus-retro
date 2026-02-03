#include "libretro_sink.h"
#include <algorithm>
#include <cstring>

namespace AudioCore {

LibretroSink::LibretroSink() {
    ring_buffer.resize(44100 * 2); // 1 second buffer
}

LibretroSink::~LibretroSink() = default;

void LibretroSink::SetCallback(std::function<void(s16*, std::size_t)> cb) {
    callback = cb;
}

void LibretroSink::EnqueueSamples(const s16* samples, std::size_t num_frames) {
    std::lock_guard<std::mutex> lock(mutex);
    std::size_t num_samples = num_frames * 2;

    for (std::size_t i = 0; i < num_samples; i++) {
        ring_buffer[write_pos] = samples[i];
        write_pos = (write_pos + 1) % ring_buffer.size();
        if (samples_available < ring_buffer.size()) {
            samples_available++;
        } else {
            read_pos = (read_pos + 1) % ring_buffer.size(); // Overwrite oldest
        }
    }
}

std::size_t LibretroSink::Pull(s16* buffer, std::size_t num_frames) {
    std::lock_guard<std::mutex> lock(mutex);
    std::size_t num_samples = num_frames * 2;
    std::size_t to_read = std::min(num_samples, samples_available);

    for (std::size_t i = 0; i < to_read; i++) {
        buffer[i] = ring_buffer[read_pos];
        read_pos = (read_pos + 1) % ring_buffer.size();
        samples_available--;
    }

    return to_read / 2;
}

} // namespace AudioCore
