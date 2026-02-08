#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>
#include <chrono>

namespace Settings {
    namespace NativeButton { enum Values : int { A, B, X, Y, L, R, Start, Select, Up, Down, Left, Right }; }
    namespace NativeAnalog { enum Values : int { CirclePad, CStick }; }
}

namespace InputCommon::SDL {
    void Init() {}
    class SDLState {
    public:
        virtual ~SDLState();
        void* GetSDLControllerButtonBindByGUID(const std::string&, int, Settings::NativeButton::Values);
        void* GetSDLControllerAnalogBindByGUID(const std::string&, int, Settings::NativeAnalog::Values);
    };

    SDLState::~SDLState() = default;
    void* SDLState::GetSDLControllerButtonBindByGUID(const std::string&, int, Settings::NativeButton::Values) { return nullptr; }
    void* SDLState::GetSDLControllerAnalogBindByGUID(const std::string&, int, Settings::NativeAnalog::Values) { return nullptr; }
}

// Bruteforce stubs for SoundTouch mangled names
extern "C" {
    void _ZN10soundtouch12TDStretchSSEC2Ev(void* self) {}
    void _ZN10soundtouch12FIRFilterSSEC1Ev(void* self) {}
    void* _ZTVN10soundtouch12TDStretchSSEE[10] = {0};
}
