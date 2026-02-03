#include "libretro_input.h"
#include <mutex>
#include <unordered_map>

namespace Input {

static std::unordered_map<unsigned, LibretroButtonDevice*> buttons;
static LibretroAnalogDevice* circle_pad = nullptr;
static LibretroAnalogDevice* c_stick = nullptr;
static LibretroTouchDevice* touch_device = nullptr;
static LibretroMotionDevice* motion_device = nullptr;

LibretroButtonDevice::LibretroButtonDevice(unsigned id) : id(id) {}
bool LibretroButtonDevice::GetStatus() const { return status.load(); }
void LibretroButtonDevice::SetStatus(bool pressed) { status.store(pressed); }

LibretroAnalogDevice::LibretroAnalogDevice(unsigned ax, unsigned ay) : axis_x(ax), axis_y(ay) {}
std::tuple<float, float> LibretroAnalogDevice::GetStatus() const {
    return { status_x.load(), status_y.load() };
}
void LibretroAnalogDevice::SetStatus(float x, float y) {
    status_x.store(x);
    status_y.store(y);
}

std::tuple<float, float, bool> LibretroTouchDevice::GetStatus() const {
    return { status_x.load(), status_y.load(), status_pressed.load() };
}
void LibretroTouchDevice::SetStatus(float x, float y, bool pressed) {
    status_x.store(x);
    status_y.store(y);
    status_pressed.store(pressed);
}

std::tuple<Common::Vec3<float>, Common::Vec3<float>> LibretroMotionDevice::GetStatus() const {
    return { {ax.load(), ay.load(), az.load()}, {gx.load(), gy.load(), gz.load()} };
}

void LibretroMotionDevice::SetStatus(Common::Vec3<float> accel, Common::Vec3<float> gyro) {
    ax.store(accel.x); ay.store(accel.y); az.store(accel.z);
    gx.store(gyro.x); gy.store(gyro.y); gz.store(gyro.z);
}

std::unique_ptr<ButtonDevice> LibretroButtonFactory::Create(const Common::ParamPackage& params) {
    unsigned id = static_cast<unsigned>(params.Get("button", 0));
    auto device = std::make_unique<LibretroButtonDevice>(id);
    buttons[id] = device.get();
    return device;
}

std::unique_ptr<AnalogDevice> LibretroAnalogFactory::Create(const Common::ParamPackage& params) {
    unsigned ax = static_cast<unsigned>(params.Get("axis_x", 0));
    unsigned ay = static_cast<unsigned>(params.Get("axis_y", 1));
    auto device = std::make_unique<LibretroAnalogDevice>(ax, ay);
    if (ax == 0) circle_pad = device.get();
    else if (ax == 2) c_stick = device.get();
    return device;
}

std::unique_ptr<TouchDevice> LibretroTouchFactory::Create(const Common::ParamPackage& params) {
    auto device = std::make_unique<LibretroTouchDevice>();
    touch_device = device.get();
    return device;
}

std::unique_ptr<MotionDevice> LibretroMotionFactory::Create(const Common::ParamPackage& params) {
    auto device = std::make_unique<LibretroMotionDevice>();
    motion_device = device.get();
    return device;
}

void RegisterLibretroInput() {
    RegisterFactory<ButtonDevice>("libretro", std::make_shared<LibretroButtonFactory>());
    RegisterFactory<AnalogDevice>("libretro", std::make_shared<LibretroAnalogFactory>());
    RegisterFactory<TouchDevice>("libretro", std::make_shared<LibretroTouchFactory>());
    RegisterFactory<MotionDevice>("libretro", std::make_shared<LibretroMotionFactory>());
}

void LibretroSetButton(int id, bool pressed) {
    auto it = buttons.find(static_cast<unsigned>(id));
    if (it != buttons.end()) it->second->SetStatus(pressed);
}

void LibretroSetAnalog(bool is_c_stick, float x, float y) {
    if (is_c_stick) {
        if (c_stick) c_stick->SetStatus(x, y);
    } else {
        if (circle_pad) circle_pad->SetStatus(x, y);
    }
}

void LibretroSetTouch(float x, float y, bool pressed) {
    if (touch_device) touch_device->SetStatus(x, y, pressed);
}

void LibretroSetMotion(float ax, float ay, float az, float gx, float gy, float gz) {
    if (motion_device) motion_device->SetStatus({ax, ay, az}, {gx, gy, gz});
}

} // namespace Input
