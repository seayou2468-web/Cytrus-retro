// Copyright 2024 Jules
// Licensed under GPLv2 or any later version

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <cstring>
#include <map>
#include <unordered_map>
#include <vector>
#include <dynarmic/frontend/A32/translate/a32_translate.h>
#include <dynarmic/ir/basic_block.h>
#include <dynarmic/ir/microinstruction.h>
#include <dynarmic/ir/opcodes.h>
#include <dynarmic/frontend/A32/a32_location_descriptor.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/arm/dynarmic/arm_dynarmic_cp15.h"
#include "core/arm/dynarmic/arm_exclusive_monitor.h"
#include "core/arm/dynarmic/arm_static_ir.h"
#include "core/arm/dynarmic/arm_tick_counts.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/svc.h"
#include "core/memory.h"

namespace Core {

using namespace Dynarmic;

class ARM_StaticIR_Callbacks final : public Dynarmic::A32::UserCallbacks {
public:
    explicit ARM_StaticIR_Callbacks(ARM_StaticIR& parent)
        : parent(parent), memory(parent.memory), svc_context(parent.system) {}

    std::uint8_t MemoryRead8(Dynarmic::A32::VAddr vaddr) override { return memory.Read8(vaddr); }
    std::uint16_t MemoryRead16(Dynarmic::A32::VAddr vaddr) override { return memory.Read16(vaddr); }
    std::uint32_t MemoryRead32(Dynarmic::A32::VAddr vaddr) override { return memory.Read32(vaddr); }
    std::uint64_t MemoryRead64(Dynarmic::A32::VAddr vaddr) override { return memory.Read64(vaddr); }

    void MemoryWrite8(Dynarmic::A32::VAddr vaddr, std::uint8_t value) override { memory.Write8(vaddr, value); }
    void MemoryWrite16(Dynarmic::A32::VAddr vaddr, std::uint16_t value) override { memory.Write16(vaddr, value); }
    void MemoryWrite32(Dynarmic::A32::VAddr vaddr, std::uint32_t value) override { memory.Write32(vaddr, value); }
    void MemoryWrite64(Dynarmic::A32::VAddr vaddr, std::uint64_t value) override { memory.Write64(vaddr, value); }

    bool MemoryWriteExclusive8(Dynarmic::A32::VAddr vaddr, u8 value, u8 expected) override {
        return memory.WriteExclusive8(vaddr, value, expected);
    }
    bool MemoryWriteExclusive16(Dynarmic::A32::VAddr vaddr, u16 value, u16 expected) override {
        return memory.WriteExclusive16(vaddr, value, expected);
    }
    bool MemoryWriteExclusive32(Dynarmic::A32::VAddr vaddr, u32 value, u32 expected) override {
        return memory.WriteExclusive32(vaddr, value, expected);
    }
    bool MemoryWriteExclusive64(Dynarmic::A32::VAddr vaddr, u64 value, u64 expected) override {
        return memory.WriteExclusive64(vaddr, value, expected);
    }

    void InterpreterFallback(Dynarmic::A32::VAddr pc, std::size_t num_instructions) override {
        ::Common::Log::FmtLogMessage(::Common::Log::Class::Core_ARM11, ::Common::Log::Level::Critical,
                                     ::Common::Log::TrimSourcePath(__FILE__), __LINE__, __func__,
                                     "InterpreterFallback reached at 0x{:08X}", pc);
    }

    void CallSVC(std::uint32_t swi) override {
        svc_context.CallSVC(swi);
    }

    void ExceptionRaised(Dynarmic::A32::VAddr pc, Dynarmic::A32::Exception exception) override {
        ::Common::Log::FmtLogMessage(::Common::Log::Class::Core_ARM11, ::Common::Log::Level::Critical,
                                     ::Common::Log::TrimSourcePath(__FILE__), __LINE__, __func__,
                                     "ExceptionRaised: {} at 0x{:08X}", (int)exception, pc);
    }

    void AddTicks(std::uint64_t ticks) override { parent.GetTimer().AddTicks(ticks); }
    std::uint64_t GetTicksRemaining() override {
        s64 ticks = parent.GetTimer().GetDowncount();
        return static_cast<u64>(ticks <= 0 ? 0 : ticks);
    }
    std::uint64_t GetTicksForCode(bool is_thumb, Dynarmic::A32::VAddr, std::uint32_t instruction) override {
        return Core::TicksForInstruction(is_thumb, instruction);
    }

    ARM_StaticIR& parent;
    Memory::MemorySystem& memory;
    Kernel::SVCContext svc_context;
};

ARM_StaticIR::ARM_StaticIR(Core::System& system_, Memory::MemorySystem& memory_, u32 core_id_,
                           std::shared_ptr<Core::Timing::Timer> timer_,
                           Core::ExclusiveMonitor& exclusive_monitor_)
    : ARM_Interface(core_id_, timer_), system(system_), memory(memory_),
      cb(std::make_unique<ARM_StaticIR_Callbacks>(*this)),
      exclusive_monitor{dynamic_cast<Core::DynarmicExclusiveMonitor&>(exclusive_monitor_)} {

    config.callbacks = cb.get();
    config.define_unpredictable_behaviour = true;
    config.processor_id = core_id_;
    config.global_monitor = &exclusive_monitor.GetMonitor();
}

ARM_StaticIR::~ARM_StaticIR() = default;

ARM_StaticIR_Callbacks& ARM_StaticIR::GetCallbacks() { return *cb; }

void ARM_StaticIR::Run() {
    while (system.IsPoweredOn()) {
        const auto& block = GetOrTranslateBlock(regs[15]);
        ExecuteBlock(block);
        if (timer->GetDowncount() <= 0) {
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
void ARM_StaticIR::ClearInstructionCache() {
    block_cache.clear();
    for (auto& entry : fast_block_cache) entry.pc = 0xFFFFFFFF;
}
void ARM_StaticIR::InvalidateCacheRange(u32 start_address, std::size_t length) {
    for (auto it = block_cache.begin(); it != block_cache.end(); ) {
        if (it->first >= start_address && it->first < start_address + length) {
            it = block_cache.erase(it);
        } else {
            ++it;
        }
    }
    for (auto& entry : fast_block_cache) {
        if (entry.pc >= start_address && entry.pc < start_address + length) {
            entry.pc = 0xFFFFFFFF;
        }
    }
}
void ARM_StaticIR::ClearExclusiveState() {}
void ARM_StaticIR::SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) {
    current_page_table = page_table;
}
std::shared_ptr<Memory::PageTable> ARM_StaticIR::GetPageTable() const { return current_page_table; }

const ARM_StaticIR::TranslatedBlock& ARM_StaticIR::GetOrTranslateBlock(u32 pc) {
    const u32 index = (pc >> 2) & (FAST_BLOCK_CACHE_SIZE - 1);
    if (fast_block_cache[index].pc == pc) return *fast_block_cache[index].block;

    auto it = block_cache.find(pc);
    if (it != block_cache.end()) {
        fast_block_cache[index].pc = pc;
        fast_block_cache[index].block = &it->second;
        return it->second;
    }

    Dynarmic::A32::LocationDescriptor ld{pc, Dynarmic::A32::PSR(cpsr), Dynarmic::A32::FPSCR(fpscr)};
    IR::Block block = A32::Translate(ld, cb.get(), {config.arch_version, config.define_unpredictable_behaviour, config.hook_hint_instructions});

    TranslatedBlock tb;
    std::map<IR::Inst*, u16> inst_to_index;
    u16 next_index = 0;

    for (auto& inst : block.Instructions()) {
        Instruction decoded;
        decoded.op = inst.GetOpcode();
        decoded.result_index = next_index;
        decoded.arg_count = static_cast<u8>(std::min<size_t>(inst.NumArgs(), decoded.args.size()));
        inst_to_index[&inst] = next_index++;

        for (size_t i = 0; i < decoded.arg_count; ++i) {
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
            decoded.args[i] = op;
        }
        tb.instructions.push_back(decoded);
    }

    tb.guest_end_pc = Dynarmic::A32::LocationDescriptor{block.EndLocation()}.PC();
    auto [new_it, inserted] = block_cache.emplace(pc, std::move(tb));
    fast_block_cache[index].pc = pc;
    fast_block_cache[index].block = &new_it->second;
    return new_it->second;
}

namespace {
static inline u64 GetArg(const ARM_StaticIR::Instruction& inst, const u64* results, size_t i) {
    if (inst.args[i].kind == ARM_StaticIR::Operand::Immediate) return inst.args[i].value;
    return results[inst.args[i].value];
}

static bool CheckCondition(u32 cpsr, IR::Cond cond) {
    const bool n = (cpsr >> 31) & 1;
    const bool z = (cpsr >> 30) & 1;
    const bool c = (cpsr >> 29) & 1;
    const bool v = (cpsr >> 28) & 1;
    switch (cond) {
    case IR::Cond::EQ: return z;
    case IR::Cond::NE: return !z;
    case IR::Cond::CS: return c;
    case IR::Cond::CC: return !c;
    case IR::Cond::MI: return n;
    case IR::Cond::PL: return !n;
    case IR::Cond::VS: return v;
    case IR::Cond::VC: return !v;
    case IR::Cond::HI: return c && !z;
    case IR::Cond::LS: return !c || z;
    case IR::Cond::GE: return n == v;
    case IR::Cond::LT: return n != v;
    case IR::Cond::GT: return !z && (n == v);
    case IR::Cond::LE: return z || (n != v);
    case IR::Cond::AL: return true;
    default: return true;
    }
}
} // namespace

void ARM_StaticIR::ExecuteBlock(const TranslatedBlock& block) {
    size_t num_insts = block.instructions.size();
    if (results_buffer.size() < num_insts) {
        results_buffer.resize(std::max<size_t>(num_insts, 512));
        flags_buffer.resize(std::max<size_t>(num_insts, 512));
    }
    u64* results = results_buffer.data();

    u32 next_pc = block.guest_end_pc;
    bool branched = false;
    size_t executed_count = 0;

    for (const auto& inst : block.instructions) {
        executed_count++;
        switch (inst.op) {
        case IR::Opcode::A32GetRegister: results[inst.result_index] = GetReg((int)inst.args[0].value); break;
        case IR::Opcode::A32SetRegister: SetReg((int)inst.args[0].value, (u32)GetArg(inst, results, 1)); break;
        case IR::Opcode::A32GetExtendedRegister32: results[inst.result_index] = GetVFPReg((int)inst.args[0].value); break;
        case IR::Opcode::A32SetExtendedRegister32: SetVFPReg((int)inst.args[0].value, (u32)GetArg(inst, results, 1)); break;
        case IR::Opcode::A32GetCpsr: results[inst.result_index] = GetCPSR(); break;
        case IR::Opcode::A32SetCpsr: SetCPSR((u32)GetArg(inst, results, 0)); break;
        case IR::Opcode::A32SetCpsrNZCV: SetCPSR((GetCPSR() & 0x0FFFFFFF) | ((u32)GetArg(inst, results, 0) << 28)); break;
        case IR::Opcode::Add32: {
            u32 a = (u32)GetArg(inst, results, 0);
            u32 b = (u32)GetArg(inst, results, 1);
            u32 res = a + b;
            results[inst.result_index] = res;
            u32 flags = (((res >> 31) & 1) << 3) | ((res == 0) << 2) | ((res < a) << 1) | ((~(a ^ b) & (a ^ res)) >> 31);
            flags_buffer[inst.result_index] = flags;
            if (inst.arg_count > 2 && inst.args[2].kind == Operand::Immediate && inst.args[2].value) SetCPSR((GetCPSR() & 0x0FFFFFFF) | (flags << 28));
        } break;
        case IR::Opcode::Sub32: {
            u32 a = (u32)GetArg(inst, results, 0);
            u32 b = (u32)GetArg(inst, results, 1);
            u32 res = a - b;
            results[inst.result_index] = res;
            u32 flags = (((res >> 31) & 1) << 3) | ((res == 0) << 2) | ((a >= b) << 1) | (((a ^ b) & (a ^ res)) >> 31);
            flags_buffer[inst.result_index] = flags;
            if (inst.arg_count > 2 && inst.args[2].kind == Operand::Immediate && inst.args[2].value) SetCPSR((GetCPSR() & 0x0FFFFFFF) | (flags << 28));
        } break;
        case IR::Opcode::LogicalShiftLeft32: results[inst.result_index] = (u32)GetArg(inst, results, 0) << (GetArg(inst, results, 1) & 31); break;
        case IR::Opcode::LogicalShiftRight32: results[inst.result_index] = (u32)GetArg(inst, results, 0) >> (GetArg(inst, results, 1) & 31); break;
        case IR::Opcode::A32ReadMemory32: results[inst.result_index] = cb->MemoryRead32((u32)GetArg(inst, results, 0)); break;
        case IR::Opcode::A32WriteMemory32: cb->MemoryWrite32((u32)GetArg(inst, results, 0), (u32)GetArg(inst, results, 1)); break;
        case IR::Opcode::A32BXWritePC: {
            u32 val = (u32)GetArg(inst, results, 0);
            next_pc = val & ~1;
            if (val & 1) SetCPSR(GetCPSR() | 0x20); else SetCPSR(GetCPSR() & ~0x20);
            branched = true;
        } break;
        case IR::Opcode::ConditionalSelect32: {
            bool cond_met = CheckCondition(GetCPSR(), (IR::Cond)inst.args[0].value);
            results[inst.result_index] = cond_met ? GetArg(inst, results, 1) : GetArg(inst, results, 2);
        } break;
        case IR::Opcode::A32CallSupervisor: cb->CallSVC((u32)inst.args[0].value); break;
        case IR::Opcode::Identity: results[inst.result_index] = GetArg(inst, results, 0); break;
        case IR::Opcode::Void: break;
        default: {
            // Handle rarer opcodes via slow path or individual cases
            if (inst.op == IR::Opcode::Mul32) results[inst.result_index] = (u32)GetArg(inst, results, 0) * (u32)GetArg(inst, results, 1);
            else if (inst.op == IR::Opcode::And32) results[inst.result_index] = (u32)GetArg(inst, results, 0) & (u32)GetArg(inst, results, 1);
            else if (inst.op == IR::Opcode::Or32) results[inst.result_index] = (u32)GetArg(inst, results, 0) | (u32)GetArg(inst, results, 1);
            else if (inst.op == IR::Opcode::Eor32) results[inst.result_index] = (u32)GetArg(inst, results, 0) ^ (u32)GetArg(inst, results, 1);
            else if (inst.op == IR::Opcode::ArithmeticShiftRight32) results[inst.result_index] = (u32)((s32)GetArg(inst, results, 0) >> (GetArg(inst, results, 1) & 31));
            else if (inst.op == IR::Opcode::A32GetCFlag) results[inst.result_index] = (GetCPSR() >> 29) & 1;
            else if (inst.op == IR::Opcode::GetNZCVFromOp) results[inst.result_index] = flags_buffer[inst.args[0].value];
            else {
                 // Fallback for everything else
                 LOG_TRACE(Core_ARM11, "Rare/Unimplemented IR opcode in optimized loop: {}", (int)inst.op);
            }
        } break;
        }
        if (branched) break;
    }
    regs[15] = next_pc;
    cb->AddTicks(std::max<u64>(1, (u64)executed_count * 2));
}

} // namespace Core
