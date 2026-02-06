// Copyright 2024 Jules
// Licensed under GPLv2 or any later version

#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"
#include "core/arm/arm_interface.h"

namespace Core {

class ExclusiveMonitor;
class ARM_IRBackend;
class ARM_DynCom;
using ARM_HLEBackend = ARM_DynCom;

class ARM_HybridCPU final : public ARM_Interface {
public:
    explicit ARM_HybridCPU(Core::System& system_, Memory::MemorySystem& memory_, u32 core_id_,
                           std::shared_ptr<Core::Timing::Timer> timer,
                           Core::ExclusiveMonitor& exclusive_monitor_);
    ~ARM_HybridCPU() override;

    void Run() override;
    void Step() override;

    void SetPC(u32 pc) override;
    u32 GetPC() const override;
    u32 GetReg(int index) const override;
    void SetReg(int index, u32 value) override;
    u32 GetVFPReg(int index) const override;
    void SetVFPReg(int index, u32 value) override;
    u32 GetVFPSystemReg(VFPSystemRegister reg) const override;
    void SetVFPSystemReg(VFPSystemRegister reg, u32 value) override;
    u32 GetCPSR() const override;
    void SetCPSR(u32 cpsr) override;
    u32 GetCP15Register(CP15Register reg) const override;
    void SetCP15Register(CP15Register reg, u32 value) override;

    void SaveContext(ThreadContext& ctx) override;
    void LoadContext(const ThreadContext& ctx) override;

    void PrepareReschedule() override;

    void ClearInstructionCache() override;
    void InvalidateCacheRange(u32 start_address, std::size_t length) override;
    void ClearExclusiveState() override;
    void SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) override;

    // Region management
    void SetIRRegion(u32 start, u32 size);
    void SetHLERegion(u32 start, u32 size);

public:
    std::shared_ptr<Memory::PageTable> GetPageTable() const override;

private:
    bool UseIR(u32 pc) const;
    void SyncState(ARM_Interface& from, ARM_Interface& to);

    Core::System& system;
    std::unique_ptr<ARM_IRBackend> ir_backend;
    std::unique_ptr<ARM_HLEBackend> hle_backend;
    ARM_Interface* current_active_backend;

    struct Region {
        u32 start;
        u32 end;
    };
    std::vector<Region> ir_regions;
};

} // namespace Core
