#pragma once

#include <memory>
#include <tuple>
#include <unordered_map>
#include <mutex>
#include "core/frontend/input.h"

namespace LibretroInput {

class LibretroButtonDevice final : public Input::ButtonDevice {
public:
    void SetState(bool pressed) { state = pressed; }
    bool GetStatus() const override { return state; }
private:
    bool state = false;
};

class LibretroAnalogDevice final : public Input::AnalogDevice {
public:
    void SetState(float x, float y) { this->x = x; this->y = y; }
    std::tuple<float, float> GetStatus() const override { return {x, y}; }
private:
    float x = 0.0f, y = 0.0f;
};

class LibretroTouchDevice final : public Input::TouchDevice {
public:
    void SetState(float x, float y, bool pressed) {
        this->x = x;
        this->y = y;
        this->pressed = pressed;
    }
    std::tuple<float, float, bool> GetStatus() const override { return {x, y, pressed}; }
private:
    float x = 0.0f, y = 0.0f;
    bool pressed = false;
};

class InputManager {
public:
    static InputManager& GetInstance();

    void SetButton(int id, bool pressed);
    void SetAnalog(int id, float x, float y);
    void SetTouch(float x, float y, bool pressed);

    std::shared_ptr<LibretroButtonDevice> GetButtonDevice(int id);
    std::shared_ptr<LibretroAnalogDevice> GetAnalogDevice(int id);
    std::shared_ptr<LibretroTouchDevice> GetTouchDevice();

    void RegisterFactories();

private:
    std::unordered_map<int, std::shared_ptr<LibretroButtonDevice>> buttons;
    std::unordered_map<int, std::shared_ptr<LibretroAnalogDevice>> analogs;
    std::shared_ptr<LibretroTouchDevice> touch;
    std::mutex mutex;
};

} // namespace LibretroInput
