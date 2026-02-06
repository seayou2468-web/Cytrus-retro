// Copyright 2024 Jules
// Licensed under GPLv2 or any later version

#include "core/arm/hybrid_cpu.h"
#include "core/arm/ir_backend.h"
#include "core/arm/hle_backend.h"
#include "core/core.h"

namespace Core {

ARM_HybridCPU::ARM_HybridCPU(Core::System& system_, Memory::MemorySystem& memory_, u32 core_id_,
                             std::shared_ptr<Core::Timing::Timer> timer_,
                             Core::ExclusiveMonitor& exclusive_monitor_)
    : ARM_Interface(core_id_, timer_), system(system_) {

    ir_backend = std::make_unique<ARM_IRBackend>(system, memory_, core_id_, timer_, exclusive_monitor_);
    hle_backend = std::make_unique<ARM_HLEBackend>(system, memory_, USER32MODE, core_id_, timer_);
    current_active_backend = hle_backend.get(); // HLE is default
}

ARM_HybridCPU::~ARM_HybridCPU() = default;

bool ARM_HybridCPU::UseIR(u32 pc) const {
    for (const auto& region : ir_regions) {
        if (pc >= region.start && pc < region.end) {
            return true;
        }
    }
    return false;
}

void ARM_HybridCPU::SyncState(ARM_Interface& from, ARM_Interface& to) {
    ThreadContext ctx;
    from.SaveContext(ctx);
    to.LoadContext(ctx);
    to.SetPageTable(from.GetPageTable());

    // Sync CP15 registers (important for TLS)
    to.SetCP15Register(CP15_THREAD_UPRW, from.GetCP15Register(CP15_THREAD_UPRW));
    to.SetCP15Register(CP15_THREAD_URO, from.GetCP15Register(CP15_THREAD_URO));
}

void ARM_HybridCPU::Run() {
    while (system.IsPoweredOn()) {
        u32 pc = current_active_backend->GetPC();
        bool want_ir = UseIR(pc);

        if (want_ir && current_active_backend == hle_backend.get()) {
            SyncState(*hle_backend, *ir_backend);
            current_active_backend = ir_backend.get();
        } else if (!want_ir && current_active_backend == ir_backend.get()) {
            SyncState(*ir_backend, *hle_backend);
            current_active_backend = hle_backend.get();
        }

        current_active_backend->Run();

        if (timer->GetDowncount() <= 0) {
            break;
        }
    }
}

void ARM_HybridCPU::Step() {
    u32 pc = current_active_backend->GetPC();
    bool want_ir = UseIR(pc);

    if (want_ir && current_active_backend == hle_backend.get()) {
        SyncState(*hle_backend, *ir_backend);
        current_active_backend = ir_backend.get();
    } else if (!want_ir && current_active_backend == ir_backend.get()) {
        SyncState(*ir_backend, *hle_backend);
        current_active_backend = hle_backend.get();
    }

    current_active_backend->Step();
}

void ARM_HybridCPU::SetPC(u32 pc) { current_active_backend->SetPC(pc); }
u32 ARM_HybridCPU::GetPC() const { return current_active_backend->GetPC(); }
u32 ARM_HybridCPU::GetReg(int index) const { return current_active_backend->GetReg(index); }
void ARM_HybridCPU::SetReg(int index, u32 value) { current_active_backend->SetReg(index, value); }
u32 ARM_HybridCPU::GetVFPReg(int index) const { return current_active_backend->GetVFPReg(index); }
void ARM_HybridCPU::SetVFPReg(int index, u32 value) { current_active_backend->SetVFPReg(index, value); }
u32 ARM_HybridCPU::GetVFPSystemReg(VFPSystemRegister reg) const { return current_active_backend->GetVFPSystemReg(reg); }
void ARM_HybridCPU::SetVFPSystemReg(VFPSystemRegister reg, u32 value) { current_active_backend->SetVFPSystemReg(reg, value); }
u32 ARM_HybridCPU::GetCPSR() const { return current_active_backend->GetCPSR(); }
void ARM_HybridCPU::SetCPSR(u32 cpsr) { current_active_backend->SetCPSR(cpsr); }
u32 ARM_HybridCPU::GetCP15Register(CP15Register reg) const { return current_active_backend->GetCP15Register(reg); }
void ARM_HybridCPU::SetCP15Register(CP15Register reg, u32 value) { current_active_backend->SetCP15Register(reg, value); }

void ARM_HybridCPU::SaveContext(ThreadContext& ctx) { current_active_backend->SaveContext(ctx); }
void ARM_HybridCPU::LoadContext(const ThreadContext& ctx) {
    ir_backend->LoadContext(ctx);
    hle_backend->LoadContext(ctx);
}

void ARM_HybridCPU::PrepareReschedule() { current_active_backend->PrepareReschedule(); }

void ARM_HybridCPU::ClearInstructionCache() {
    ir_backend->ClearInstructionCache();
    hle_backend->ClearInstructionCache();
}
void ARM_HybridCPU::InvalidateCacheRange(u32 start_address, std::size_t length) {
    ir_backend->InvalidateCacheRange(start_address, length);
    hle_backend->InvalidateCacheRange(start_address, length);
}
void ARM_HybridCPU::ClearExclusiveState() {
    ir_backend->ClearExclusiveState();
    hle_backend->ClearExclusiveState();
}
void ARM_HybridCPU::SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) {
    ir_backend->SetPageTable(page_table);
    hle_backend->SetPageTable(page_table);
}

void ARM_HybridCPU::SetIRRegion(u32 start, u32 size) {
    ir_regions.push_back({start, start + size});
}

void ARM_HybridCPU::SetHLERegion(u32 start, u32 size) {
    for (auto it = ir_regions.begin(); it != ir_regions.end(); ) {
        if (it->start >= start && it->end <= start + size) {
            it = ir_regions.erase(it);
        } else {
            ++it;
        }
    }
}

std::shared_ptr<Memory::PageTable> ARM_HybridCPU::GetPageTable() const {
    return current_active_backend->GetPageTable();
}

} // namespace Core
