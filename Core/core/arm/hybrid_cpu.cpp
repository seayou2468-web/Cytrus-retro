// Copyright 2024 Jules
// Licensed under GPLv2 or any later version

#include "core/arm/hybrid_cpu.h"
#include "core/arm/dynarmic/arm_hybrid.h"
#include "core/core.h"

namespace Core {

ARM_HybridCPU::ARM_HybridCPU(Core::System& system_, Memory::MemorySystem& memory_, u32 core_id_,
                             std::shared_ptr<Core::Timing::Timer> timer_,
                             Core::ExclusiveMonitor& exclusive_monitor_)
    : ARM_Interface(core_id_, timer_), system(system_) {

    backend = std::make_unique<ARM_Hybrid>(system, memory_, core_id_, timer_, exclusive_monitor_);
}

ARM_HybridCPU::~ARM_HybridCPU() = default;

void ARM_HybridCPU::Run() {
    backend->Run();
}

void ARM_HybridCPU::Step() {
    backend->Step();
}

void ARM_HybridCPU::SetPC(u32 pc) { backend->SetPC(pc); }
u32 ARM_HybridCPU::GetPC() const { return backend->GetPC(); }
u32 ARM_HybridCPU::GetReg(int index) const { return backend->GetReg(index); }
void ARM_HybridCPU::SetReg(int index, u32 value) { backend->SetReg(index, value); }
u32 ARM_HybridCPU::GetVFPReg(int index) const { return backend->GetVFPReg(index); }
void ARM_HybridCPU::SetVFPReg(int index, u32 value) { backend->SetVFPReg(index, value); }
u32 ARM_HybridCPU::GetVFPSystemReg(VFPSystemRegister reg) const { return backend->GetVFPSystemReg(reg); }
void ARM_HybridCPU::SetVFPSystemReg(VFPSystemRegister reg, u32 value) { backend->SetVFPSystemReg(reg, value); }
u32 ARM_HybridCPU::GetCPSR() const { return backend->GetCPSR(); }
void ARM_HybridCPU::SetCPSR(u32 cpsr) { backend->SetCPSR(cpsr); }
u32 ARM_HybridCPU::GetCP15Register(CP15Register reg) const { return backend->GetCP15Register(reg); }
void ARM_HybridCPU::SetCP15Register(CP15Register reg, u32 value) { backend->SetCP15Register(reg, value); }

void ARM_HybridCPU::SaveContext(ThreadContext& ctx) { backend->SaveContext(ctx); }
void ARM_HybridCPU::LoadContext(const ThreadContext& ctx) { backend->LoadContext(ctx); }

void ARM_HybridCPU::PrepareReschedule() { backend->PrepareReschedule(); }

void ARM_HybridCPU::ClearInstructionCache() {
    backend->ClearInstructionCache();
}
void ARM_HybridCPU::InvalidateCacheRange(u32 start_address, std::size_t length) {
    backend->InvalidateCacheRange(start_address, length);
}
void ARM_HybridCPU::ClearExclusiveState() {
    backend->ClearExclusiveState();
}
void ARM_HybridCPU::SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) {
    backend->SetPageTable(page_table);
}

void ARM_HybridCPU::SetIRRegion(u32 start, u32 size) {
    // Forward or handle if backend needs it. For now, backend has its own logic.
}

void ARM_HybridCPU::SetHLERegion(u32 start, u32 size) {
    // Forward or handle if backend needs it.
}

std::shared_ptr<Memory::PageTable> ARM_HybridCPU::GetPageTable() const {
    return backend->GetPageTable();
}

} // namespace Core
