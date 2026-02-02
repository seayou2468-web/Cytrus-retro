#pragma once

#include <functional>
#include "audio_core/sink.h"
#include "common/common_types.h"

namespace AudioCore {

class LibretroSink final : public Sink {
public:
    explicit LibretroSink();
    ~LibretroSink() override;

    unsigned int GetNativeSampleRate() const override { return 44100; }
    void SetCallback(std::function<void(s16*, std::size_t)> cb) override;

    void Pull(s16* buffer, std::size_t num_frames);

private:
    std::function<void(s16*, std::size_t)> callback;
};

} // namespace AudioCore
