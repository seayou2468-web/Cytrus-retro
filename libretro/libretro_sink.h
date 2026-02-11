#pragma once

#include "audio_core/sink.h"
#include <functional>

namespace AudioCore {

class LibretroSink : public Sink {
public:
    LibretroSink() = default;
    ~LibretroSink() override = default;

    unsigned int GetNativeSampleRate() const override { return 32768; }

    void SetCallback(std::function<void(s16*, std::size_t)> cb) override {
        callback = cb;
    }

    void PullSamples(s16* buffer, std::size_t num_frames) {
        if (callback) {
            callback(buffer, num_frames);
        }
    }

private:
    std::function<void(s16*, std::size_t)> callback;
};

} // namespace AudioCore
