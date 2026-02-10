#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "audio_core/sink.h"

namespace AudioCore {

class LibretroSink final : public Sink {
public:
    explicit LibretroSink();
    ~LibretroSink() override;

    unsigned int GetNativeSampleRate() const override;
    void SetCallback(std::function<void(s16*, std::size_t)> cb) override;

    void Drain(std::function<void(s16*, std::size_t)> push_cb);

private:
    std::function<void(s16*, std::size_t)> callback;
};

std::vector<std::string> ListLibretroSinkDevices();

} // namespace AudioCore
