#include "core/hle/kernel/svc.h"
#include "core/core.h"
#include "core/arm/arm_interface.h"

namespace Kernel {

typedef void (*FastSvcFn)(SVC*);

// Example of a fast SVC wrapper
static void FastControlMemory(SVC* self) {
    // R0: out_addr, R1: addr0, R2: addr1, R3: size, R4: operation, R5: permissions
    // In original: Wrap<&SVC::ControlMemory>
    // We can do it directly.
    // ...
}

// For now, I'll just implement the table with the existing functions
// but using a faster lookup.

void SVC::CallSVC(u32 immediate) {
    // Special fast path for common SVCs
    // 0x32: SendSyncRequest
    // 0x24: WaitSynchronization1

    std::scoped_lock lock{kernel.GetHLELock()};

    if (immediate == 0x32) {
        // Handle SendSyncRequest directly
        this->SendSyncRequest(static_cast<Handle>(this->GetReg(0)));
        return;
    }

    const FunctionDef* info = GetSVCInfo(immediate);
    if (info && info->func) {
        (this->*(info->func))();
    }
}

} // namespace Kernel
