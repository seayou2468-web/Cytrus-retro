#include "libretro_input.h"
#include "common/param_package.h"

namespace LibretroInput {

InputManager& InputManager::GetInstance() {
    static InputManager instance;
    return instance;
}

void InputManager::SetButton(int id, bool pressed) {
    std::lock_guard<std::mutex> lock(mutex);
    if (buttons.count(id)) {
        buttons[id]->SetState(pressed);
    }
}

void InputManager::SetAnalog(int id, float x, float y) {
    std::lock_guard<std::mutex> lock(mutex);
    if (analogs.count(id)) {
        analogs[id]->SetState(x, y);
    }
}

void InputManager::SetTouch(float x, float y, bool pressed) {
    std::lock_guard<std::mutex> lock(mutex);
    if (touch) {
        touch->SetState(x, y, pressed);
    }
}

std::shared_ptr<LibretroButtonDevice> InputManager::GetButtonDevice(int id) {
    if (!buttons.count(id)) {
        buttons[id] = std::make_shared<LibretroButtonDevice>();
    }
    return buttons[id];
}

std::shared_ptr<LibretroAnalogDevice> InputManager::GetAnalogDevice(int id) {
    if (!analogs.count(id)) {
        analogs[id] = std::make_shared<LibretroAnalogDevice>();
    }
    return analogs[id];
}

std::shared_ptr<LibretroTouchDevice> InputManager::GetTouchDevice() {
    if (!touch) {
        touch = std::make_shared<LibretroTouchDevice>();
    }
    return touch;
}

class LibretroButtonFactory final : public Input::Factory<Input::ButtonDevice> {
public:
    std::unique_ptr<Input::ButtonDevice> Create(const Common::ParamPackage& params) override {
        int id = params.Get("id", 0);
        // Returns a dummy that just points to our managed device
        class Proxy : public Input::ButtonDevice {
        public:
            Proxy(std::shared_ptr<LibretroButtonDevice> dev) : device(dev) {}
            bool GetStatus() const override { return device->GetStatus(); }
        private:
            std::shared_ptr<LibretroButtonDevice> device;
        };
        return std::make_unique<Proxy>(InputManager::GetInstance().GetButtonDevice(id));
    }
};

class LibretroAnalogFactory final : public Input::Factory<Input::AnalogDevice> {
public:
    std::unique_ptr<Input::AnalogDevice> Create(const Common::ParamPackage& params) override {
        int id = params.Get("id", 0);
        class Proxy : public Input::AnalogDevice {
        public:
            Proxy(std::shared_ptr<LibretroAnalogDevice> dev) : device(dev) {}
            std::tuple<float, float> GetStatus() const override { return device->GetStatus(); }
        private:
            std::shared_ptr<LibretroAnalogDevice> device;
        };
        return std::make_unique<Proxy>(InputManager::GetInstance().GetAnalogDevice(id));
    }
};

class LibretroTouchFactory final : public Input::Factory<Input::TouchDevice> {
public:
    std::unique_ptr<Input::TouchDevice> Create(const Common::ParamPackage& params) override {
        class Proxy : public Input::TouchDevice {
        public:
            Proxy(std::shared_ptr<LibretroTouchDevice> dev) : device(dev) {}
            std::tuple<float, float, bool> GetStatus() const override { return device->GetStatus(); }
        private:
            std::shared_ptr<LibretroTouchDevice> device;
        };
        return std::make_unique<Proxy>(InputManager::GetInstance().GetTouchDevice());
    }
};

void InputManager::RegisterFactories() {
    Input::RegisterFactory<Input::ButtonDevice>("libretro", std::make_shared<LibretroButtonFactory>());
    Input::RegisterFactory<Input::AnalogDevice>("libretro", std::make_shared<LibretroAnalogFactory>());
    Input::RegisterFactory<Input::TouchDevice>("libretro", std::make_shared<LibretroTouchFactory>());
}

} // namespace LibretroInput
