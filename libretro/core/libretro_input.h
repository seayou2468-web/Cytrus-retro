#pragma once

#include "core/frontend/input.h"
#include "libretro.h"

namespace Input {

class LibretroButtonDevice final : public ButtonDevice {
public:
    explicit LibretroButtonDevice(unsigned port, unsigned id);
    bool GetStatus() const override;
private:
    unsigned port;
    unsigned id;
};

class LibretroAnalogDevice final : public AnalogDevice {
public:
    explicit LibretroAnalogDevice(unsigned port, unsigned index, unsigned id_x, unsigned id_y);
    std::tuple<float, float> GetStatus() const override;
private:
    unsigned port;
    unsigned index;
    unsigned id_x;
    unsigned id_y;
};

class LibretroTouchDevice final : public TouchDevice {
public:
    explicit LibretroTouchDevice();
    std::tuple<float, float, bool> GetStatus() const override;
};

void RegisterLibretroInputFactories();

} // namespace Input
