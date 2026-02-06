#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>
#include <chrono>

// Forward declarations
namespace Network {
    struct GameInfo {};
    struct WifiPacket {};
    namespace ArticBaseCommon { enum class LogOnServerType { dummy }; }
}

namespace Network {
    class RoomMember {
    public:
        enum class State { Error, Idle, Joining, Joined };
        State GetState() const;
        std::vector<uint8_t> GetMacAddress() const;
        bool IsConnected() const;
        void SendGameInfo(const GameInfo&);
        void SendWifiPacket(const WifiPacket&);
        void BindOnWifiPacketReceived(std::function<void(const WifiPacket&)>);
        template<typename T> void Unbind(std::shared_ptr<std::function<void(const T&)>>) {}
    };

    RoomMember& GetRoomMember() {
        static RoomMember member;
        return member;
    }

    namespace SocketManager {
        void EnableSockets() {}
        void DisableSockets() {}
    }

    namespace ArticBase {
        class Client {
        public:
            struct Response {
                const void* GetResponseBuffer(unsigned int) const;
                uint64_t GetResponseU64(unsigned int) const;
            };
            struct Request {
                Request(uint32_t, const std::string&, uint64_t) {}
                void AddParameterS8(signed char);
                void AddParameterS32(int);
                void AddParameterS64(long);
                void AddParameterBuffer(const void*, size_t);
            };
            struct UDPStream {
                void Start();
            };

            Client(const std::string&, uint16_t);
            ~Client();
            void Connect();
            void Stop();
            void StopImpl(bool);
            Response Send(Request&);
            void LogOnServer(Network::ArticBaseCommon::LogOnServerType, const std::string&);
            std::shared_ptr<UDPStream> NewUDPStream(std::string, uint64_t, const std::chrono::milliseconds&);
            Request& NewRequest(const std::string&);
        };
    }
}

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
}

// Implementations
namespace Network {
    RoomMember::State RoomMember::GetState() const { return State::Idle; }
    std::vector<uint8_t> RoomMember::GetMacAddress() const { return {0,0,0,0,0,0}; }
    bool RoomMember::IsConnected() const { return false; }
    void RoomMember::SendGameInfo(const GameInfo&) {}
    void RoomMember::SendWifiPacket(const WifiPacket&) {}
    void RoomMember::BindOnWifiPacketReceived(std::function<void(const WifiPacket&)>) {}
}

namespace Network::ArticBase {
    Client::Client(const std::string&, uint16_t) {}
    Client::~Client() {}
    void Client::Connect() {}
    void Client::Stop() {}
    void Client::StopImpl(bool) {}
    void Client::LogOnServer(Network::ArticBaseCommon::LogOnServerType, const std::string&) {}
    Client::Response Client::Send(Request&) { return Response(); }
    std::shared_ptr<Client::UDPStream> Client::NewUDPStream(std::string, uint64_t, const std::chrono::milliseconds&) { return nullptr; }
    Client::Request& Client::NewRequest(const std::string&) { static Request r(0,"",0); return r; }

    void Client::Request::AddParameterS8(signed char) {}
    void Client::Request::AddParameterS32(int) {}
    void Client::Request::AddParameterS64(long) {}
    void Client::Request::AddParameterBuffer(const void*, size_t) {}

    uint64_t Client::Response::GetResponseU64(unsigned int) const { return 0; }
    const void* Client::Response::GetResponseBuffer(unsigned int) const { return nullptr; }

    void Client::UDPStream::Start() {}
}

namespace InputCommon::SDL {
    SDLState::~SDLState() = default;
    void* SDLState::GetSDLControllerButtonBindByGUID(const std::string&, int, Settings::NativeButton::Values) { return nullptr; }
    void* SDLState::GetSDLControllerAnalogBindByGUID(const std::string&, int, Settings::NativeAnalog::Values) { return nullptr; }
}

// Global OpenSSL
extern "C" {
    int RAND_bytes(unsigned char *buf, int num) { return 1; }
}

// Bruteforce stubs for SoundTouch mangled names
extern "C" {
    void _ZN10soundtouch12TDStretchSSEC2Ev(void* self) {}
    void _ZN10soundtouch12FIRFilterSSEC1Ev(void* self) {}
    void* _ZTVN10soundtouch12TDStretchSSEE[10] = {0};
}

// Template instantiations/mangled names as needed by linker
extern "C" {
    void _ZN7Network10RoomMember6UnbindINS_10WifiPacketEEEvSt10shared_ptrISt8functionIFvRKT_EEE(void* self, void* ptr) {}
}
