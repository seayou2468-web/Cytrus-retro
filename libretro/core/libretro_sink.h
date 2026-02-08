#pragma once

#include <functional>
#include <vector>
#include "audio_core/sink.h"
#include "libretro.h"

namespace AudioCore {

class LibretroSink final : public Sink {
public:
    explicit LibretroSink() = default;
    ~LibretroSink() override = default;

    unsigned int GetNativeSampleRate() const override {
        return 48000;
    }

    void SetCallback(std::function<void(s16*, std::size_t)> cb) override {
        callback = std::move(cb);
    }

    void SetLibretroCallback(retro_audio_sample_batch_t cb) {
        audio_batch_cb = cb;
    }

    void Flush(u64 ticks_passed);

private:
    std::function<void(s16*, std::size_t)> callback;
    retro_audio_sample_batch_t audio_batch_cb = nullptr;
    double sample_accumulator = 0.0;
};

} // namespace AudioCore
