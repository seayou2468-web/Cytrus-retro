#include "libretro_input.h"
#include "common/param_package.h"

extern retro_input_state_t input_state_cb;

namespace Input {

LibretroButtonDevice::LibretroButtonDevice(unsigned port, unsigned id) : port(port), id(id) {}

bool LibretroButtonDevice::GetStatus() const {
    if (!input_state_cb) return false;
    return input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, id) != 0;
}

LibretroAnalogDevice::LibretroAnalogDevice(unsigned port, unsigned index, unsigned id_x, unsigned id_y)
    : port(port), index(index), id_x(id_x), id_y(id_y) {}

std::tuple<float, float> LibretroAnalogDevice::GetStatus() const {
    if (!input_state_cb) return {0.0f, 0.0f};
    float x = input_state_cb(port, RETRO_DEVICE_ANALOG, index, id_x) / 32768.0f;
    float y = input_state_cb(port, RETRO_DEVICE_ANALOG, index, id_y) / 32768.0f;
    return {x, y};
}

LibretroTouchDevice::LibretroTouchDevice() {}

std::tuple<float, float, bool> LibretroTouchDevice::GetStatus() const {
    if (!input_state_cb) return {0.0f, 0.0f, false};
    // Libretro touch is absolute within the screen.
    // Core::kScreenBottomWidth/Height
    s16 x = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
    s16 y = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);
    bool pressed = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) != 0;

    // Convert -1000..1000 to 0..1
    float fx = (x + 1000) / 2000.0f;
    float fy = (y + 1000) / 2000.0f;

    // Map to bottom screen area?
    // In our composition, bottom screen is at y=[240..480]
    // So fy should be in [0.5..1.0] to be on the bottom screen?
    // Actually, libretro frontend might map pointer to the whole window.

    return {fx, (fy - 0.5f) * 2.0f, pressed && fy >= 0.5f};
}

class LibretroButtonFactory final : public Factory<ButtonDevice> {
public:
    std::unique_ptr<ButtonDevice> Create(const Common::ParamPackage& params) override {
        unsigned port = params.Get("port", 0);
        unsigned id = params.Get("id", 0);
        return std::make_unique<LibretroButtonDevice>(port, id);
    }
};

class LibretroAnalogFactory final : public Factory<AnalogDevice> {
public:
    std::unique_ptr<AnalogDevice> Create(const Common::ParamPackage& params) override {
        unsigned port = params.Get("port", 0);
        unsigned index = params.Get("index", 0);
        unsigned id_x = params.Get("id_x", 0);
        unsigned id_y = params.Get("id_y", 0);
        return std::make_unique<LibretroAnalogDevice>(port, index, id_x, id_y);
    }
};

class LibretroTouchFactory final : public Factory<TouchDevice> {
public:
    std::unique_ptr<TouchDevice> Create(const Common::ParamPackage& params) override {
        return std::make_unique<LibretroTouchDevice>();
    }
};

void RegisterLibretroInputFactories() {
    RegisterFactory<ButtonDevice>("libretro", std::make_shared<LibretroButtonFactory>());
    RegisterFactory<AnalogDevice>("libretro", std::make_shared<LibretroAnalogFactory>());
    RegisterFactory<TouchDevice>("libretro", std::make_shared<LibretroTouchFactory>());
}

} // namespace Input
