#include <iostream>
#include <string>
#include <vector>
#include <cstdarg>
#include <cstdint>
#include "common/logging/log.h"
#include "common/logging/backend.h"

// ArticBase stubs
namespace ArticBase {
    class Client {
    public:
        Client() {}
        ~Client() {}
        void Connect(const std::string& host, uint16_t port) {}
        void Disconnect() {}
        bool IsConnected() const { return false; }
    };
}

// GDBStub stubs
namespace Core {
    class GDBStub {
    public:
        static void Init() {}
        static void Shutdown() {}
        static bool IsServerStarted() { return false; }
    };
}

// WebService stubs
namespace WebService {
    class Client {
    public:
        Client(const std::string&, const std::string&, const std::string&) {}
        void PostJson(const std::string&, const std::string&, bool) {}
    };
}

// External libraries stubs
extern "C" {
    void cryptopp_stub() {}
}
