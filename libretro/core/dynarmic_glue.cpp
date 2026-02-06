#include <dynarmic/interface/exclusive_monitor.h>
#include <dynarmic/frontend/A64/a64_types.h>
#include <dynarmic/frontend/A64/a64_location_descriptor.h>
#include <dynarmic/ir/basic_block.h>
#include <dynarmic/ir/value.h>
#include <string>

namespace Dynarmic {

ExclusiveMonitor::ExclusiveMonitor(size_t processor_count) {
    exclusive_addresses.resize(processor_count, INVALID_EXCLUSIVE_ADDRESS);
    exclusive_values.resize(processor_count);
}

size_t ExclusiveMonitor::GetProcessorCount() const {
    return exclusive_addresses.size();
}

void ExclusiveMonitor::Clear() {
    for (auto& addr : exclusive_addresses) {
        addr = INVALID_EXCLUSIVE_ADDRESS;
    }
}

void ExclusiveMonitor::ClearProcessor(size_t processor_id) {
    exclusive_addresses[processor_id] = INVALID_EXCLUSIVE_ADDRESS;
}

bool ExclusiveMonitor::CheckAndClear(size_t processor_id, VAddr address) {
    if (exclusive_addresses[processor_id] != (address & RESERVATION_GRANULE_MASK)) {
        return false;
    }
    exclusive_addresses[processor_id] = INVALID_EXCLUSIVE_ADDRESS;
    return true;
}

void ExclusiveMonitor::Lock() {}
void ExclusiveMonitor::Unlock() {}

namespace A64 {
    const char* CondToString(IR::Cond cond) { return "cond"; }
    std::string RegToString(Reg reg) { return "reg"; }
    std::string VecToString(Vec vec) { return "vec"; }

    bool TranslateSingleInstruction(IR::Block&, LocationDescriptor, uint32_t) { return false; }

    namespace IREmitter {
        void WriteMemory128(const IR::U64&, const IR::U128&, IR::AccType) {}
        void WriteMemory64(const IR::U64&, const IR::U64&, IR::AccType) {}
        void WriteMemory32(const IR::U64&, const IR::U32&, IR::AccType) {}
    }
}

} // namespace Dynarmic
