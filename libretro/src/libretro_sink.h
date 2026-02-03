#pragma once

#include <vector>
#include <mutex>
#include "audio_core/sink.h"
#include "common/common_types.h"

namespace AudioCore {

class LibretroSink final : public Sink {
public:
    explicit LibretroSink();
    ~LibretroSink() override;

    unsigned int GetNativeSampleRate() const override { return 44100; }
    void SetCallback(std::function<void(s16*, std::size_t)> cb) override;

    void EnqueueSamples(const s16* samples, std::size_t num_frames);
    std::size_t Pull(s16* buffer, std::size_t num_frames);

private:
    std::function<void(s16*, std::size_t)> callback;
    std::vector<s16> ring_buffer;
    std::size_t read_pos = 0;
    std::size_t write_pos = 0;
    std::size_t samples_available = 0;
    std::mutex mutex;
};

} // namespace AudioCore
