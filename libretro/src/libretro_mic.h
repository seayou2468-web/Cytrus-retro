#pragma once

#include <memory>
#include <vector>
#include "audio_core/input.h"
#include "libretro.h"

namespace AudioCore {

class LibretroMic final : public Input {
public:
    explicit LibretroMic(retro_environment_t environ_cb);
    ~LibretroMic() override;

    void StartSampling(const InputParameters& params) override;
    void StopSampling() override;
    bool IsSampling() override;
    void AdjustSampleRate(u32 sample_rate) override;
    Samples Read() override;

private:
    retro_environment_t environ_cb;
    retro_microphone_interface mic_iface{};
    retro_microphone_t* mic_handle = nullptr;
    bool is_sampling = false;
};

} // namespace AudioCore
