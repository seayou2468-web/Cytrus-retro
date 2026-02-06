#pragma once

#include <dynarmic/interface/exclusive_monitor.h>
#include "core/arm/exclusive_monitor.h"

namespace Memory {
class MemorySystem;
}

namespace Core {

class DynarmicExclusiveMonitor final : public ExclusiveMonitor {
public:
    explicit DynarmicExclusiveMonitor(Memory::MemorySystem& memory, std::size_t core_count);
    ~DynarmicExclusiveMonitor() override;

    u8 ExclusiveRead8(std::size_t core_index, VAddr addr) override;
    u16 ExclusiveRead16(std::size_t core_index, VAddr addr) override;
    u32 ExclusiveRead32(std::size_t core_index, VAddr addr) override;
    u64 ExclusiveRead64(std::size_t core_index, VAddr addr) override;
    void ClearExclusive(std::size_t core_index) override;

    bool ExclusiveWrite8(std::size_t core_index, VAddr vaddr, u8 value) override;
    bool ExclusiveWrite16(std::size_t core_index, VAddr vaddr, u16 value) override;
    bool ExclusiveWrite32(std::size_t core_index, VAddr vaddr, u32 value) override;
    bool ExclusiveWrite64(std::size_t core_index, VAddr vaddr, u64 value) override;

    Dynarmic::ExclusiveMonitor& GetMonitor() { return monitor; }

private:
    Dynarmic::ExclusiveMonitor monitor;
    Memory::MemorySystem& memory;
};

} // namespace Core
