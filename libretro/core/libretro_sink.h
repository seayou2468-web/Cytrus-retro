#pragma once

#include "audio_core/sink.h"
#include "libretro.h"

namespace AudioCore {

class LibretroSink final : public Sink {
public:
    explicit LibretroSink() = default;
    ~LibretroSink() override = default;

    unsigned int GetNativeSampleRate() const override {
        return 32768;
    }

    void SetCallback(std::function<void(s16*, std::size_t)> cb) override {
        callback = std::move(cb);
    }

    void Flush();

private:
    std::function<void(s16*, std::size_t)> callback;
};

} // namespace AudioCore
