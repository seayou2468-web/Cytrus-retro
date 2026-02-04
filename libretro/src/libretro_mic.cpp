#include "libretro_mic.h"
#include "common/logging/log.h"
#include <cstring>

namespace AudioCore {

LibretroMic::LibretroMic(retro_environment_t environ_cb) : environ_cb(environ_cb) {
    if (!environ_cb(RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE, &mic_iface)) {
        LOG_WARNING(Audio, "Libretro Microphone interface not supported by frontend");
        std::memset(&mic_iface, 0, sizeof(mic_iface));
    }
}

LibretroMic::~LibretroMic() {
    StopSampling();
}

void LibretroMic::StartSampling(const InputParameters& params) {
    parameters = params;
    if (!mic_iface.open_mic) return;

    retro_microphone_params_t mic_params;
    mic_params.rate = params.sample_rate;

    mic_handle = mic_iface.open_mic(&mic_params);
    if (mic_handle) {
        mic_iface.set_mic_state(mic_handle, true);
        is_sampling = true;
        LOG_INFO(Audio, "Libretro Microphone opened at {} Hz", params.sample_rate);
    } else {
        LOG_ERROR(Audio, "Failed to open Libretro Microphone");
    }
}

void LibretroMic::StopSampling() {
    if (mic_handle && mic_iface.close_mic) {
        mic_iface.set_mic_state(mic_handle, false);
        mic_iface.close_mic(mic_handle);
    }
    mic_handle = nullptr;
    is_sampling = false;
}

bool LibretroMic::IsSampling() {
    return is_sampling;
}

void LibretroMic::AdjustSampleRate(u32 sample_rate) {
    parameters.sample_rate = sample_rate;
    if (is_sampling) {
        StopSampling();
        StartSampling(parameters);
    }
}

Samples LibretroMic::Read() {
    if (!is_sampling || !mic_handle || !mic_iface.read_mic) {
        return GenerateSilentSamples(parameters);
    }

    // 3DS expects 15 samples per update period usually, but Read() is called periodically.
    // Libretro read_mic returns number of 16-bit samples.
    size_t num_samples = 16;
    std::vector<int16_t> buffer(num_samples);
    int read = mic_iface.read_mic(mic_handle, buffer.data(), num_samples);

    if (read <= 0) {
        return GenerateSilentSamples(parameters);
    }

    Samples out;
    if (parameters.sample_size == 8) {
        out.resize(read);
        for (int i = 0; i < read; i++) {
            // Convert i16 to u8 or s8
            int val = buffer[i] >> 8;
            if (parameters.sign == Signedness::Unsigned) {
                out[i] = static_cast<u8>(val + 128);
            } else {
                out[i] = static_cast<u8>(val);
            }
        }
    } else {
        out.resize(read * 2);
        for (int i = 0; i < read; i++) {
            int16_t val = buffer[i];
            if (parameters.sign == Signedness::Unsigned) {
                val = static_cast<int16_t>(static_cast<uint16_t>(val) + 32768);
            }
            std::memcpy(out.data() + i * 2, &val, 2);
        }
    }
    return out;
}

} // namespace AudioCore
