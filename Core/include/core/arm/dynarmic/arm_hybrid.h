// Copyright 2024 Jules
// Licensed under GPLv2 or any later version

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <dynarmic/interface/A32/a32.h>
#include <dynarmic/ir/opcodes.h>
#include "common/common_types.h"
#include "common/u128.h"
#include "core/arm/arm_interface.h"
#include "core/arm/dynarmic/arm_dynarmic_cp15.h"
#include "core/arm/skyeye_common/armstate.h"
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

class ARM_Hybrid_Callbacks;
class DynarmicExclusiveMonitor;
class ExclusiveMonitor;
class System;

class ARM_Hybrid final : public ARM_Interface {
public:
    explicit ARM_Hybrid(Core::System& system_, Memory::MemorySystem& memory_, u32 core_id_,
                          std::shared_ptr<Core::Timing::Timer> timer,
                          Core::ExclusiveMonitor& exclusive_monitor_);
    ~ARM_Hybrid() override;

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
        enum Kind { Immediate, Result, Register, ExtReg, Cond, AccType, CoprocInfo } kind;
        u64 value;
    };

    struct Instruction {
        Dynarmic::IR::Opcode op;
        void (*handler)(ARM_Hybrid&, const Instruction&, U128* results);
        std::array<Operand, 5> args;
        u8 arg_count;
        u16 result_index;
    };

    using IRHandler = void (*)(ARM_Hybrid&, const Instruction&, U128* results);

    ARM_Hybrid_Callbacks& GetCallbacks() { return *cb; }

    using HLEFunction = std::function<void(ARM_Hybrid&)>;
    void RegisterHLEFunction(u32 address, HLEFunction func);

    std::shared_ptr<Memory::PageTable> GetPageTable() const override;

    void SetIRRegion(u32 start, u32 size);
    void SetHLERegion(u32 start, u32 size);

    u32 ReadCP15(u32 crn, u32 op1, u32 crm, u32 op2) const;
    void WriteCP15(u32 value, u32 crn, u32 op1, u32 crm, u32 op2);

private:
    friend class ARM_Hybrid_Callbacks;

    struct TranslatedBlock {
        std::vector<Instruction> instructions;
        u32 guest_end_pc;
        u64 total_ticks = 0;
        u32 instruction_count = 0;
        bool use_ir = true;
    };

    void ExecuteBlock(const TranslatedBlock& block);
    const TranslatedBlock& GetOrTranslateBlock(u32 pc);

    Core::System& system;
    Memory::MemorySystem& memory;
    std::unique_ptr<ARM_Hybrid_Callbacks> cb;
    std::unique_ptr<ARMul_State> state;

    Core::DynarmicExclusiveMonitor& exclusive_monitor;

public:
    Core::DynarmicExclusiveMonitor& GetExclusiveMonitor() { return exclusive_monitor; }

private:
    std::shared_ptr<Memory::PageTable> current_page_table = nullptr;
    std::unordered_map<u32, TranslatedBlock> block_cache;
    std::unordered_map<u32, HLEFunction> hle_functions;

    struct Region {
        u32 start;
        u32 size;
    };
    std::vector<Region> ir_regions;
    std::vector<Region> hle_regions;

    // Fast-path cache
    static constexpr size_t FAST_BLOCK_CACHE_SIZE = 4096;
    struct FastCacheEntry {
        u32 pc = 0xFFFFFFFF;
        const TranslatedBlock* block = nullptr;
    };
    std::array<FastCacheEntry, FAST_BLOCK_CACHE_SIZE> fast_block_cache;

    std::vector<U128> results_buffer;

public:
    std::vector<u32> flags_buffer;
    Dynarmic::A32::UserConfig config;
};

} // namespace Core
