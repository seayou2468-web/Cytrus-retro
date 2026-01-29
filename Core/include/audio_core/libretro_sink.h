#pragma once

#include <string>
#include <vector>
#include "audio_core/sink.h"

namespace AudioCore {

class LibretroSink : public Sink {
public:
    explicit LibretroSink(std::string_view device_id);
    ~LibretroSink() override;

    unsigned int GetNativeSampleRate() const override {
        return 44100;
    }

    void SetCallback(std::function<void(s16*, std::size_t)> cb) override;

    // Custom method to be called from Libretro core to pull samples
    static void OutputSamples(s16* samples, std::size_t num_samples);

private:
    std::function<void(s16*, std::size_t)> callback;
};

extern std::function<void(s16*, std::size_t)> g_fill_callback;

std::vector<std::string> ListLibretroSinkDevices();

} // namespace AudioCore
