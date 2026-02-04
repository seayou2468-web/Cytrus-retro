#pragma once

#include <atomic>
#include <tuple>
#include <vector>
#include "core/frontend/input.h"
#include "common/vector_math.h"

namespace Input {

class LibretroButtonDevice final : public ButtonDevice {
public:
    explicit LibretroButtonDevice(unsigned id);
    bool GetStatus() const override;
    void SetStatus(bool pressed);

private:
    unsigned id;
    std::atomic<bool> status{false};
};

class LibretroAnalogDevice final : public AnalogDevice {
public:
    explicit LibretroAnalogDevice(unsigned axis_x, unsigned axis_y);
    std::tuple<float, float> GetStatus() const override;
    void SetStatus(float x, float y);

private:
    unsigned axis_x, axis_y;
    std::atomic<float> status_x{0.0f}, status_y{0.0f};
};

class LibretroTouchDevice final : public TouchDevice {
public:
    std::tuple<float, float, bool> GetStatus() const override;
    void SetStatus(float x, float y, bool pressed);

private:
    std::atomic<float> status_x{0.0f}, status_y{0.0f};
    std::atomic<bool> status_pressed{false};
};

class LibretroButtonFactory final : public Factory<ButtonDevice> {
public:
    std::unique_ptr<ButtonDevice> Create(const Common::ParamPackage& params) override;
};

class LibretroAnalogFactory final : public Factory<AnalogDevice> {
public:
    std::unique_ptr<AnalogDevice> Create(const Common::ParamPackage& params) override;
};

class LibretroMotionDevice final : public MotionDevice {
public:
    std::tuple<Common::Vec3<float>, Common::Vec3<float>> GetStatus() const override;
    void SetStatus(Common::Vec3<float> accel, Common::Vec3<float> gyro);

private:
    std::atomic<float> ax{0.0f}, ay{0.0f}, az{0.0f};
    std::atomic<float> gx{0.0f}, gy{0.0f}, gz{0.0f};
};

class LibretroTouchFactory final : public Factory<TouchDevice> {
public:
    std::unique_ptr<TouchDevice> Create(const Common::ParamPackage& params) override;
};

class LibretroMotionFactory final : public Factory<MotionDevice> {
public:
    std::unique_ptr<MotionDevice> Create(const Common::ParamPackage& params) override;
};

void RegisterLibretroInput();

void LibretroSetButton(int id, bool pressed);
void LibretroSetAnalog(bool c_stick, float x, float y);
void LibretroSetTouch(float x, float y, bool pressed);
void LibretroSetMotion(float ax, float ay, float az, float gx, float gy, float gz);

} // namespace Input
