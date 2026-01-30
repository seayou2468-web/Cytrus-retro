// Copyright 2024 Jules
// Licensed under GPLv2 or any later version

#include <algorithm>
#include <cstring>
#include <dynarmic/frontend/A32/translate/a32_translate.h>
#include <dynarmic/ir/basic_block.h>
#include <dynarmic/ir/microinstruction.h>
#include <dynarmic/ir/opcodes.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/arm/dynarmic/arm_dynarmic_cp15.h"
#include "core/arm/dynarmic/arm_exclusive_monitor.h"
#include "core/arm/dynarmic/arm_static_ir.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/memory.h"

namespace Core {

using namespace Dynarmic;

class DynarmicUserCallbacks final : public Dynarmic::A32::UserCallbacks {
public:
    explicit DynarmicUserCallbacks(ARM_StaticIR& parent)
        : parent(parent), memory(parent.memory) {}

    std::uint8_t MemoryRead8(VAddr vaddr) override { return memory.Read8(vaddr); }
    std::uint16_t MemoryRead16(VAddr vaddr) override { return memory.Read16(vaddr); }
    std::uint32_t MemoryRead32(VAddr vaddr) override { return memory.Read32(vaddr); }
    std::uint64_t MemoryRead64(VAddr vaddr) override { return memory.Read64(vaddr); }

    void MemoryWrite8(VAddr vaddr, std::uint8_t value) override { memory.Write8(vaddr, value); }
    void MemoryWrite16(VAddr vaddr, std::uint16_t value) override { memory.Write16(vaddr, value); }
    void MemoryWrite32(VAddr vaddr, std::uint32_t value) override { memory.Write32(vaddr, value); }
    void MemoryWrite64(VAddr vaddr, std::uint64_t value) override { memory.Write64(vaddr, value); }

    bool MemoryWriteExclusive8(u32 vaddr, u8 value, u8 expected) override {
        return memory.WriteExclusive8(vaddr, value, expected);
    }
    bool MemoryWriteExclusive16(u32 vaddr, u16 value, u16 expected) override {
        return memory.WriteExclusive16(vaddr, value, expected);
    }
    bool MemoryWriteExclusive32(u32 vaddr, u32 value, u32 expected) override {
        return memory.WriteExclusive32(vaddr, value, expected);
    }
    bool MemoryWriteExclusive64(u32 vaddr, u64 value, u64 expected) override {
        return memory.WriteExclusive64(vaddr, value, expected);
    }

    void InterpreterFallback(VAddr pc, std::size_t num_instructions) override {
        LOG_CRITICAL(Core_ARM, "InterpreterFallback reached at 0x{:08X}", pc);
    }

    void CallSVC(std::uint32_t swi) override {
        parent.system.Kernel().GetSVCContext().CallSVC(swi);
    }

    void ExceptionRaised(VAddr pc, Dynarmic::A32::Exception exception) override {
        LOG_CRITICAL(Core_ARM, "ExceptionRaised: {} at 0x{:08X}", (int)exception, pc);
    }

    void AddTicks(std::uint64_t ticks) override { parent.GetTimer().AddTicks(ticks); }
    std::uint64_t GetTicksRemaining() override {
        s64 ticks = parent.GetTimer().GetDowncount();
        return static_cast<u64>(ticks <= 0 ? 0 : ticks);
    }
    std::uint64_t GetTicksForCode(bool is_thumb, VAddr, std::uint32_t instruction) override {
        return Core::TicksForInstruction(is_thumb, instruction);
    }

    ARM_StaticIR& parent;
    Memory::MemorySystem& memory;
};

ARM_StaticIR::ARM_StaticIR(Core::System& system_, Memory::MemorySystem& memory_, u32 core_id_,
                           std::shared_ptr<Core::Timing::Timer> timer_,
                           Core::ExclusiveMonitor& exclusive_monitor_)
    : ARM_Interface(core_id_, timer_), system(system_), memory(memory_),
      cb(std::make_unique<DynarmicUserCallbacks>(*this)),
      exclusive_monitor{dynamic_cast<Core::DynarmicExclusiveMonitor&>(exclusive_monitor_)} {

    config.callbacks = cb.get();
    config.define_unpredictable_behaviour = true;
    config.processor_id = core_id_;
    config.global_monitor = &exclusive_monitor.monitor;
}

ARM_StaticIR::~ARM_StaticIR() = default;

void ARM_StaticIR::Run() {
    while (system.IsPoweredOn()) {
        const auto& block = GetOrTranslateBlock(regs[15]);
        ExecuteBlock(block);
        if (parent_timer->GetDowncount() <= 0) {
            break;
        }
    }
}

void ARM_StaticIR::Step() {
    const auto& block = GetOrTranslateBlock(regs[15]);
    ExecuteBlock(block);
}

void ARM_StaticIR::SetPC(u32 pc) { regs[15] = pc; }
u32 ARM_StaticIR::GetPC() const { return regs[15]; }
u32 ARM_StaticIR::GetReg(int index) const { return regs[index]; }
void ARM_StaticIR::SetReg(int index, u32 value) { regs[index] = value; }
u32 ARM_StaticIR::GetVFPReg(int index) const { return vfp_regs[index]; }
void ARM_StaticIR::SetVFPReg(int index, u32 value) { vfp_regs[index] = value; }
u32 ARM_StaticIR::GetVFPSystemReg(VFPSystemRegister reg) const {
    if (reg == VFP_FPSCR) return fpscr;
    if (reg == VFP_FPEXC) return fpexc;
    return 0;
}
void ARM_StaticIR::SetVFPSystemReg(VFPSystemRegister reg, u32 value) {
    if (reg == VFP_FPSCR) fpscr = value;
    else if (reg == VFP_FPEXC) fpexc = value;
}
u32 ARM_StaticIR::GetCPSR() const { return cpsr; }
void ARM_StaticIR::SetCPSR(u32 cpsr_) { cpsr = cpsr_; }
u32 ARM_StaticIR::GetCP15Register(CP15Register reg) const {
    if (reg == CP15_THREAD_UPRW) return cp15_state.cp15_thread_uprw;
    if (reg == CP15_THREAD_URO) return cp15_state.cp15_thread_uro;
    return 0;
}
void ARM_StaticIR::SetCP15Register(CP15Register reg, u32 value) {
    if (reg == CP15_THREAD_UPRW) cp15_state.cp15_thread_uprw = value;
    else if (reg == CP15_THREAD_URO) cp15_state.cp15_thread_uro = value;
}

void ARM_StaticIR::SaveContext(ThreadContext& ctx) {
    std::memcpy(ctx.cpu_registers.data(), regs, sizeof(regs));
    ctx.cpsr = cpsr;
    std::memcpy(ctx.fpu_registers.data(), vfp_regs, sizeof(vfp_regs));
    ctx.fpscr = fpscr;
    ctx.fpexc = fpexc;
}

void ARM_StaticIR::LoadContext(const ThreadContext& ctx) {
    std::memcpy(regs, ctx.cpu_registers.data(), sizeof(regs));
    cpsr = ctx.cpsr;
    std::memcpy(vfp_regs, ctx.fpu_registers.data(), sizeof(vfp_regs));
    fpscr = ctx.fpscr;
    fpexc = ctx.fpexc;
}

void ARM_StaticIR::PrepareReschedule() {}
void ARM_StaticIR::ClearInstructionCache() { block_cache.clear(); }
void ARM_StaticIR::InvalidateCacheRange(u32 start_address, std::size_t length) {
    auto it = block_cache.lower_bound(start_address);
    while (it != block_cache.end() && it->first < start_address + length) {
        it = block_cache.erase(it);
    }
}
void ARM_StaticIR::ClearExclusiveState() {}
void ARM_StaticIR::SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) {
    current_page_table = page_table;
}
std::shared_ptr<Memory::PageTable> ARM_StaticIR::GetPageTable() const { return current_page_table; }

const ARM_StaticIR::TranslatedBlock& ARM_StaticIR::GetOrTranslateBlock(u32 pc) {
    auto it = block_cache.find(pc);
    if (it != block_cache.end()) return it->second;

    const bool is_thumb = (cpsr & 0x20) != 0;
    LocationDescriptor ld{pc, {is_thumb}};
    IR::Block block = A32::Translate(ld, cb.get(), {config.arch_version, config.define_unpredictable_behaviour, config.hook_hint_instructions});

    TranslatedBlock tb;
    std::map<IR::Inst*, u16> inst_to_index;
    u16 next_index = 0;

    for (auto& inst : block.Instructions()) {
        Instruction decoded;
        decoded.op = inst.GetOpcode();
        decoded.result_index = next_index;
        inst_to_index[&inst] = next_index++;

        for (size_t i = 0; i < inst.NumArgs(); ++i) {
            IR::Value arg = inst.GetArg(i);
            Operand op;
            if (arg.IsImmediate()) {
                op.kind = Operand::Immediate;
                op.value = arg.GetImmediateAsU64();
            } else if (arg.GetType() == IR::Type::Cond) {
                op.kind = Operand::Cond;
                op.value = (u64)arg.GetCond();
            } else if (arg.GetType() == IR::Type::AccType) {
                op.kind = Operand::AccType;
                op.value = (u64)arg.GetAccType();
            } else if (arg.GetType() == IR::Type::A32Reg) {
                op.kind = Operand::Register;
                op.value = (u64)arg.GetA32RegRef();
            } else if (arg.GetType() == IR::Type::A32ExtReg) {
                op.kind = Operand::ExtReg;
                op.value = (u64)arg.GetA32ExtRegRef();
            } else if (!arg.IsEmpty()) {
                op.kind = Operand::Result;
                op.value = inst_to_index[arg.GetInst()];
            } else {
                op.kind = Operand::Immediate;
                op.value = 0;
            }
            decoded.args.push_back(op);
        }
        tb.instructions.push_back(decoded);
    }

    tb.guest_end_pc = block.EndLocation().PC();
    return block_cache.emplace(pc, std::move(tb)).first->second;
}

void ARM_StaticIR::ExecuteBlock(const TranslatedBlock& block) {
    std::vector<u64> results(std::max<size_t>(block.instructions.size(), 1));
    u32 next_pc = block.guest_end_pc;
    bool branched = false;

    for (const auto& inst : block.instructions) {
        auto get_arg = [&](size_t i) -> u64 {
            if (inst.args[i].kind == Operand::Immediate) return inst.args[i].value;
            return results[inst.args[i].value];
        };

        switch (inst.op) {
        case IR::Opcode::A32GetRegister:
            results[inst.result_index] = regs[inst.args[0].value];
            break;
        case IR::Opcode::A32SetRegister:
            regs[inst.args[0].value] = (u32)get_arg(1);
            break;
        case IR::Opcode::A32GetExtendedRegister32:
            results[inst.result_index] = vfp_regs[inst.args[0].value];
            break;
        case IR::Opcode::A32SetExtendedRegister32:
            vfp_regs[inst.args[0].value] = (u32)get_arg(1);
            break;
        case IR::Opcode::A32GetCpsr:
            results[inst.result_index] = cpsr;
            break;
        case IR::Opcode::A32SetCpsr:
            cpsr = (u32)get_arg(0);
            break;
        case IR::Opcode::A32SetCpsrNZCV:
            cpsr = (cpsr & 0x0FFFFFFF) | ((u32)get_arg(0) << 28);
            break;
        case IR::Opcode::Add32:
            results[inst.result_index] = (u32)get_arg(0) + (u32)get_arg(1);
            break;
        case IR::Opcode::Sub32:
            results[inst.result_index] = (u32)get_arg(0) - (u32)get_arg(1);
            break;
        case IR::Opcode::Mul32:
            results[inst.result_index] = (u32)get_arg(0) * (u32)get_arg(1);
            break;
        case IR::Opcode::And32:
            results[inst.result_index] = (u32)get_arg(0) & (u32)get_arg(1);
            break;
        case IR::Opcode::Or32:
            results[inst.result_index] = (u32)get_arg(0) | (u32)get_arg(1);
            break;
        case IR::Opcode::Eor32:
            results[inst.result_index] = (u32)get_arg(0) ^ (u32)get_arg(1);
            break;
        case IR::Opcode::Not32:
            results[inst.result_index] = ~(u32)get_arg(0);
            break;
        case IR::Opcode::LogicalShiftLeft32:
            results[inst.result_index] = (u32)get_arg(0) << (get_arg(1) & 31);
            break;
        case IR::Opcode::LogicalShiftRight32:
            results[inst.result_index] = (u32)get_arg(0) >> (get_arg(1) & 31);
            break;
        case IR::Opcode::ArithmeticShiftRight32:
            results[inst.result_index] = (u32)((s32)get_arg(0) >> (get_arg(1) & 31));
            break;
        case IR::Opcode::A32ReadMemory8:
            results[inst.result_index] = cb->MemoryRead8((u32)get_arg(0));
            break;
        case IR::Opcode::A32ReadMemory16:
            results[inst.result_index] = cb->MemoryRead16((u32)get_arg(0));
            break;
        case IR::Opcode::A32ReadMemory32:
            results[inst.result_index] = cb->MemoryRead32((u32)get_arg(0));
            break;
        case IR::Opcode::A32WriteMemory8:
            cb->MemoryWrite8((u32)get_arg(0), (u8)get_arg(1));
            break;
        case IR::Opcode::A32WriteMemory16:
            cb->MemoryWrite16((u32)get_arg(0), (u16)get_arg(1));
            break;
        case IR::Opcode::A32WriteMemory32:
            cb->MemoryWrite32((u32)get_arg(0), (u32)get_arg(1));
            break;
        case IR::Opcode::A32BXWritePC: {
            u32 val = (u32)get_arg(0);
            next_pc = val & ~1;
            if (val & 1) cpsr |= 0x20; else cpsr &= ~0x20;
            branched = true;
            break;
        }
        case IR::Opcode::A32UpdateUpperLocationDescriptor:
            // Usually does nothing in our interpreter but we should handle it if it did
            break;
        case IR::Opcode::A32CallSupervisor:
            cb->CallSVC((u32)inst.args[0].value);
            break;
        case IR::Opcode::ConditionalSelect32: {
            bool cond_met = false;
            const bool n = (cpsr >> 31) & 1;
            const bool z = (cpsr >> 30) & 1;
            const bool c = (cpsr >> 29) & 1;
            const bool v = (cpsr >> 28) & 1;
            switch ((IR::Cond)inst.args[0].value) {
            case IR::Cond::EQ: cond_met = z; break;
            case IR::Cond::NE: cond_met = !z; break;
            case IR::Cond::CS: cond_met = c; break;
            case IR::Cond::CC: cond_met = !c; break;
            case IR::Cond::MI: cond_met = n; break;
            case IR::Cond::PL: cond_met = !n; break;
            case IR::Cond::VS: cond_met = v; break;
            case IR::Cond::VC: cond_met = !v; break;
            case IR::Cond::HI: cond_met = c && !z; break;
            case IR::Cond::LS: cond_met = !c || z; break;
            case IR::Cond::GE: cond_met = n == v; break;
            case IR::Cond::LT: cond_met = n != v; break;
            case IR::Cond::GT: cond_met = !z && (n == v); break;
            case IR::Cond::LE: cond_met = z || (n != v); break;
            case IR::Cond::AL: cond_met = true; break;
            default: break;
            }
            results[inst.result_index] = cond_met ? get_arg(1) : get_arg(2);
            break;
        }
        case IR::Opcode::SignExtendByteToWord:
            results[inst.result_index] = (u32)(s32)(s8)get_arg(0);
            break;
        case IR::Opcode::SignExtendHalfToWord:
            results[inst.result_index] = (u32)(s32)(s16)get_arg(0);
            break;
        case IR::Opcode::ZeroExtendByteToWord:
            results[inst.result_index] = (u32)(u8)get_arg(0);
            break;
        case IR::Opcode::ZeroExtendHalfToWord:
            results[inst.result_index] = (u32)(u16)get_arg(0);
            break;
        case IR::Opcode::ByteReverseWord: {
            u32 val = (u32)get_arg(0);
            results[inst.result_index] = ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val & 0xFF0000) >> 8) | ((val >> 24) & 0xFF);
            break;
        }
        case IR::Opcode::RotateRight32: {
            u32 val = (u32)get_arg(0);
            u32 amount = (u32)get_arg(1) & 31;
            results[inst.result_index] = amount == 0 ? val : (val >> amount) | (val << (32 - amount));
            break;
        }
        case IR::Opcode::CountLeadingZeros32: {
            u32 val = (u32)get_arg(0);
            results[inst.result_index] = val == 0 ? 32 : __builtin_clz(val);
            break;
        }
        case IR::Opcode::Pack2x32To1x64:
            results[inst.result_index] = (get_arg(0) & 0xFFFFFFFF) | (get_arg(1) << 32);
            break;
        case IR::Opcode::LeastSignificantWord:
            results[inst.result_index] = get_arg(0) & 0xFFFFFFFF;
            break;
        case IR::Opcode::MostSignificantWord:
            results[inst.result_index] = get_arg(0) >> 32;
            break;
        case IR::Opcode::A32GetExtendedRegister64:
            results[inst.result_index] = ((u64)vfp_regs[inst.args[0].value + 1] << 32) | vfp_regs[inst.args[0].value];
            break;
        case IR::Opcode::A32SetExtendedRegister64:
            vfp_regs[inst.args[0].value] = (u32)get_arg(1);
            vfp_regs[inst.args[0].value + 1] = (u32)(get_arg(1) >> 32);
            break;
        case IR::Opcode::GetNZCVFromOp:
            // Placeholder: return current CPSR flags as NZCV
            results[inst.result_index] = cpsr >> 28;
            break;
        default:
            LOG_TRACE(Core_ARM, "Unimplemented IR opcode: {}", (int)inst.op);
            break;
        }
    }
    regs[15] = next_pc;
    cb->AddTicks(10); // Simple tick increment
}

} // namespace Core
