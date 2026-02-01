// Copyright 2024 Jules
// Licensed under GPLv2 or any later version

#pragma once

#include <array>
#include <memory>
#include <unordered_map>
#include <vector>
#include <dynarmic/interface/A32/a32.h>
#include <dynarmic/ir/opcodes.h>
#include "common/common_types.h"
#include "core/arm/arm_interface.h"
#include "core/arm/dynarmic/arm_dynarmic_cp15.h"
#include "core/hle/kernel/svc.h"

namespace Memory {
struct PageTable;
class MemorySystem;
} // namespace Memory

namespace Dynarmic::IR {
class Block;
class Inst;
}

namespace Core {

class ARM_StaticIR_Callbacks;
class DynarmicExclusiveMonitor;
class ExclusiveMonitor;
class System;

class ARM_StaticIR final : public ARM_Interface {
public:
    explicit ARM_StaticIR(Core::System& system_, Memory::MemorySystem& memory_, u32 core_id_,
                          std::shared_ptr<Core::Timing::Timer> timer,
                          Core::ExclusiveMonitor& exclusive_monitor_);
    ~ARM_StaticIR() override;

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

    struct Operand {
        enum Kind { Immediate, Result, Register, ExtReg, Cond, AccType } kind;
        u64 value;
    };

    struct Instruction {
        Dynarmic::IR::Opcode op;
        std::array<Operand, 5> args;
        u8 arg_count;
        u16 result_index;
    };

    ARM_StaticIR_Callbacks& GetCallbacks();

protected:
    std::shared_ptr<Memory::PageTable> GetPageTable() const override;

private:
    friend class ARM_StaticIR_Callbacks;

    struct TranslatedBlock {
        std::vector<Instruction> instructions;
        u32 guest_end_pc;
    };

    void ExecuteBlock(const TranslatedBlock& block);
    const TranslatedBlock& GetOrTranslateBlock(u32 pc);

    Core::System& system;
    Memory::MemorySystem& memory;
    std::unique_ptr<ARM_StaticIR_Callbacks> cb;

    u32 regs[16] = {0};
    u32 vfp_regs[64] = {0};
    u32 cpsr = 0x1D3;
    u32 fpscr = 0;
    u32 fpexc = 0;
    CP15State cp15_state;
    Core::DynarmicExclusiveMonitor& exclusive_monitor;

    std::shared_ptr<Memory::PageTable> current_page_table = nullptr;
    std::unordered_map<u32, TranslatedBlock> block_cache;
    std::vector<u64> results_buffer;

public:
    std::vector<u32> flags_buffer;
    Dynarmic::A32::UserConfig config;
};

} // namespace Core
