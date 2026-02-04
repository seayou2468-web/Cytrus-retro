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
            } else if (arg.GetType() == IR::Type::CoprocInfo) {
                op.kind = Operand::CoprocInfo;
                IR::Value::CoprocessorInfo info = arg.GetCoprocInfo();
                u64 val = 0;
                for (int i = 0; i < 8; ++i) val |= (u64)info[i] << (i * 8);
                op.value = val;
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
static inline unsigned __int128 GetArg(const ARM_StaticIR::Instruction& inst, const unsigned __int128* results, size_t i) {
    if (i >= inst.arg_count) return 0;
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
        results_buffer.resize(std::max<size_t>(num_insts, 1024));
        flags_buffer.resize(std::max<size_t>(num_insts, 1024));
    }
    unsigned __int128* results_ptr = results_buffer.data();

    u32 next_pc = block.guest_end_pc;
    bool branched = false;

    size_t executed_count = 0;
    for (const auto& inst : block.instructions) {
        executed_count++;
        switch (inst.op) {
        case IR::Opcode::A32GetRegister: results_ptr[inst.result_index] = regs[(int)inst.args[0].value]; break;
        case IR::Opcode::A32SetRegister: {
            u32 val = (u32)GetArg(inst, results_ptr, 1);
            int reg = (int)inst.args[0].value;
            regs[reg] = val;
            if (reg == 15) {
                next_pc = val & ~1;
                branched = true;
            }
        } break;
        case IR::Opcode::A32GetExtendedRegister32: {
            int reg = (int)inst.args[0].value;
            results_ptr[inst.result_index] = vfp_regs[reg];
        } break;
        case IR::Opcode::A32SetExtendedRegister32: {
            int reg = (int)inst.args[0].value;
            vfp_regs[reg] = (u32)GetArg(inst, results_ptr, 1);
        } break;
        case IR::Opcode::A32GetExtendedRegister64: {
            int reg = (int)inst.args[0].value;
            int idx = (reg - 32) * 2;
            results_ptr[inst.result_index] = ((u64)vfp_regs[idx + 1] << 32) | vfp_regs[idx];
        } break;
        case IR::Opcode::A32SetExtendedRegister64: {
            int reg = (int)inst.args[0].value;
            int idx = (reg - 32) * 2;
            u64 val = (u64)GetArg(inst, results_ptr, 1);
            vfp_regs[idx] = (u32)val;
            vfp_regs[idx + 1] = (u32)(val >> 32);
        } break;
        case IR::Opcode::A32GetVector: {
            int reg = (int)inst.args[0].value;
            unsigned __int128 res = 0;
            if (reg >= 64) { // Q0-Q15
                int idx = (reg - 64) * 4;
                res |= (unsigned __int128)vfp_regs[idx];
                res |= (unsigned __int128)vfp_regs[idx + 1] << 32;
                res |= (unsigned __int128)vfp_regs[idx + 2] << 64;
                res |= (unsigned __int128)vfp_regs[idx + 3] << 96;
            } else if (reg >= 32) { // D0-D31
                int idx = (reg - 32) * 2;
                res |= (unsigned __int128)vfp_regs[idx];
                res |= (unsigned __int128)vfp_regs[idx + 1] << 32;
            }
            results_ptr[inst.result_index] = res;
        } break;
        case IR::Opcode::A32SetVector: {
            int reg = (int)inst.args[0].value;
            unsigned __int128 val = GetArg(inst, results_ptr, 1);
            if (reg >= 64) { // Q0-Q15
                int idx = (reg - 64) * 4;
                vfp_regs[idx] = (u32)val;
                vfp_regs[idx + 1] = (u32)(val >> 32);
                vfp_regs[idx + 2] = (u32)(val >> 64);
                vfp_regs[idx + 3] = (u32)(val >> 96);
            } else if (reg >= 32) { // D0-D31
                int idx = (reg - 32) * 2;
                vfp_regs[idx] = (u32)val;
                vfp_regs[idx + 1] = (u32)(val >> 32);
            }
        } break;
        case IR::Opcode::A32GetCpsr: results_ptr[inst.result_index] = cpsr; break;
        case IR::Opcode::A32SetCpsr: cpsr = (u32)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::A32SetCpsrNZCV: cpsr = (cpsr & 0x0FFFFFFF) | ((u32)GetArg(inst, results_ptr, 0) << 28); break;
        case IR::Opcode::A32GetCFlag: results_ptr[inst.result_index] = (cpsr >> 29) & 1; break;
        case IR::Opcode::A32SetCpsrNZCVRaw: cpsr = (cpsr & 0x0FFFFFFF) | ((u32)GetArg(inst, results_ptr, 0) & 0xF0000000); break;
        case IR::Opcode::A32SetCpsrNZCVQ: cpsr = (cpsr & 0x07FFFFFF) | ((u32)GetArg(inst, results_ptr, 0) & 0xF8000000); break;
        case IR::Opcode::A32SetCpsrNZ: cpsr = (cpsr & 0x3FFFFFFF) | (((u32)GetArg(inst, results_ptr, 0) & 0xC) << 28); break;
        case IR::Opcode::A32SetCpsrNZC: cpsr = (cpsr & 0x1FFFFFFF) | (((u32)GetArg(inst, results_ptr, 0) & 0xC) << 28) | (((u32)GetArg(inst, results_ptr, 1) & 1) << 29); break;
        case IR::Opcode::A32OrQFlag: if (GetArg(inst, results_ptr, 0) & 1) cpsr |= (1 << 27); break;
        case IR::Opcode::A32GetGEFlags: results_ptr[inst.result_index] = (cpsr >> 16) & 0xF; break;
        case IR::Opcode::A32SetGEFlags: cpsr = (cpsr & ~0xF0000) | ((u32)GetArg(inst, results_ptr, 0) << 16); break;
        case IR::Opcode::A32SetGEFlagsCompressed: cpsr = (cpsr & ~0xF0000) | (((u32)GetArg(inst, results_ptr, 0) & 0xF) << 16); break;
        case IR::Opcode::A32GetFpscr: results_ptr[inst.result_index] = fpscr; break;
        case IR::Opcode::A32SetFpscr: fpscr = (u32)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::A32GetFpscrNZCV: results_ptr[inst.result_index] = fpscr >> 28; break;
        case IR::Opcode::A32SetFpscrNZCV: {
            u32 nzcv = (u32)GetArg(inst, results_ptr, 0);
            fpscr = (fpscr & 0x0FFFFFFF) | (nzcv << 28);
        } break;
        case IR::Opcode::Add64: {
            u64 a = (u64)GetArg(inst, results_ptr, 0);
            u64 b = (u64)GetArg(inst, results_ptr, 1);
            u64 carry_in = (u64)GetArg(inst, results_ptr, 2);
            unsigned __int128 res128 = (unsigned __int128)a + b + carry_in;
            u64 res = (u64)res128;
            results_ptr[inst.result_index] = res;
            flags_buffer[inst.result_index] = ((res >> 63) << 3) | ((res == 0) << 2) | ((res128 >> 64) << 1) | (((~(a ^ b) & (a ^ res)) >> 63) & 1);
        } break;
        case IR::Opcode::Add32: {
            u32 a = (u32)GetArg(inst, results_ptr, 0);
            u32 b = (u32)GetArg(inst, results_ptr, 1);
            u32 carry_in = (u32)GetArg(inst, results_ptr, 2);
            u64 res64 = (u64)a + b + carry_in;
            u32 res = (u32)res64;
            results_ptr[inst.result_index] = res;
            flags_buffer[inst.result_index] = ((res >> 31) << 3) | ((res == 0) << 2) | ((res64 >> 32) << 1) | (((~(a ^ b) & (a ^ res)) >> 31) & 1);
        } break;
        case IR::Opcode::Sub64: {
            u64 a = (u64)GetArg(inst, results_ptr, 0);
            u64 b = (u64)GetArg(inst, results_ptr, 1);
            u64 carry_in = (u64)GetArg(inst, results_ptr, 2);
            unsigned __int128 res128 = (unsigned __int128)a + ~b + carry_in;
            u64 res = (u64)res128;
            results_ptr[inst.result_index] = res;
            flags_buffer[inst.result_index] = ((res >> 63) << 3) | ((res == 0) << 2) | ((res128 >> 64) << 1) | (((a ^ b) & (a ^ res)) >> 63);
        } break;
        case IR::Opcode::Sub32: {
            u32 a = (u32)GetArg(inst, results_ptr, 0);
            u32 b = (u32)GetArg(inst, results_ptr, 1);
            u32 carry_in = (u32)GetArg(inst, results_ptr, 2);
            u64 res64 = (u64)a + ~b + carry_in;
            u32 res = (u32)res64;
            results_ptr[inst.result_index] = res;
            flags_buffer[inst.result_index] = ((res >> 31) << 3) | ((res == 0) << 2) | ((res64 >> 32) << 1) | (((a ^ b) & (a ^ res)) >> 31);
        } break;
        case IR::Opcode::Mul32: results_ptr[inst.result_index] = (u32)GetArg(inst, results_ptr, 0) * (u32)GetArg(inst, results_ptr, 1); break;
        case IR::Opcode::Mul64: results_ptr[inst.result_index] = (u64)GetArg(inst, results_ptr, 0) * (u64)GetArg(inst, results_ptr, 1); break;
        case IR::Opcode::SignedMultiplyHigh64: {
            s64 a = (s64)GetArg(inst, results_ptr, 0);
            s64 b = (s64)GetArg(inst, results_ptr, 1);
            results_ptr[inst.result_index] = (u64)(((__int128)a * b) >> 64);
        } break;
        case IR::Opcode::UnsignedMultiplyHigh64: {
            u64 a = (u64)GetArg(inst, results_ptr, 0);
            u64 b = (u64)GetArg(inst, results_ptr, 1);
            results_ptr[inst.result_index] = (u64)(((unsigned __int128)a * b) >> 64);
        } break;
        case IR::Opcode::And32: results_ptr[inst.result_index] = (u32)GetArg(inst, results_ptr, 0) & (u32)GetArg(inst, results_ptr, 1); break;
        case IR::Opcode::AndNot32: results_ptr[inst.result_index] = (u32)GetArg(inst, results_ptr, 0) & ~(u32)GetArg(inst, results_ptr, 1); break;
        case IR::Opcode::Or32: results_ptr[inst.result_index] = (u32)GetArg(inst, results_ptr, 0) | (u32)GetArg(inst, results_ptr, 1); break;
        case IR::Opcode::Eor32: results_ptr[inst.result_index] = (u32)GetArg(inst, results_ptr, 0) ^ (u32)GetArg(inst, results_ptr, 1); break;
        case IR::Opcode::Not32: results_ptr[inst.result_index] = ~(u32)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::LogicalShiftLeft32: results_ptr[inst.result_index] = (u32)GetArg(inst, results_ptr, 0) << (GetArg(inst, results_ptr, 1) & 31); break;
        case IR::Opcode::LogicalShiftRight32: results_ptr[inst.result_index] = (u32)GetArg(inst, results_ptr, 0) >> (GetArg(inst, results_ptr, 1) & 31); break;
        case IR::Opcode::ArithmeticShiftRight32: results_ptr[inst.result_index] = (u32)((s32)GetArg(inst, results_ptr, 0) >> (GetArg(inst, results_ptr, 1) & 31)); break;
        case IR::Opcode::ConditionalSelect32: {
            bool cond_met = CheckCondition(cpsr, (IR::Cond)inst.args[0].value);
            results_ptr[inst.result_index] = cond_met ? GetArg(inst, results_ptr, 1) : GetArg(inst, results_ptr, 2);
        } break;
        case IR::Opcode::GetNZCVFromOp: results_ptr[inst.result_index] = flags_buffer[inst.args[0].value]; break;
        case IR::Opcode::Pack2x64To1x128: {
            results_ptr[inst.result_index] = (GetArg(inst, results_ptr, 0) & 0xFFFFFFFFFFFFFFFFULL) | (GetArg(inst, results_ptr, 1) << 64);
        } break;
        case IR::Opcode::Identity: results_ptr[inst.result_index] = GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::A32CoprocGetOneWord: {
            u64 info_raw = inst.args[0].value;
            u8 coproc_no = info_raw & 0xFF;
            u8 opc1 = (info_raw >> 16) & 0xFF;
            u8 CRn = (info_raw >> 24) & 0xFF;
            u8 CRm = (info_raw >> 32) & 0xFF;
            u8 opc2 = (info_raw >> 40) & 0xFF;
            u32 res = 0;
            if (coproc_no == 15) {
                if (opc1 == 0 && CRn == 13 && CRm == 0 && opc2 == 2) res = cp15_state.cp15_thread_uprw;
                else if (opc1 == 0 && CRn == 13 && CRm == 0 && opc2 == 3) res = cp15_state.cp15_thread_uro;
            }
            results_ptr[inst.result_index] = res;
        } break;
        case IR::Opcode::A32CoprocSendOneWord: {
            u64 info_raw = inst.args[0].value;
            u8 coproc_no = info_raw & 0xFF;
            u8 opc1 = (info_raw >> 16) & 0xFF;
            u8 CRn = (info_raw >> 24) & 0xFF;
            u8 CRm = (info_raw >> 32) & 0xFF;
            u8 opc2 = (info_raw >> 40) & 0xFF;
            u32 val = (u32)GetArg(inst, results_ptr, 1);
            if (coproc_no == 15) {
                if (opc1 == 0 && CRn == 13 && CRm == 0 && opc2 == 2) cp15_state.cp15_thread_uprw = val;
                else if (opc1 == 0 && CRn == 13 && CRm == 0 && opc2 == 3) cp15_state.cp15_thread_uro = val;
            }
        } break;
        case IR::Opcode::A32DataSynchronizationBarrier:
        case IR::Opcode::A32DataMemoryBarrier:
        case IR::Opcode::A32InstructionSynchronizationBarrier: break;
        case IR::Opcode::ConditionalSelectNZCV: {
            u32 nzcv = (u32)GetArg(inst, results_ptr, 0);
            IR::Cond cond = (IR::Cond)inst.args[1].value;
            bool cond_met = CheckCondition(nzcv << 28, cond);
            results_ptr[inst.result_index] = cond_met ? GetArg(inst, results_ptr, 2) : GetArg(inst, results_ptr, 3);
        } break;
        case IR::Opcode::Void: break;

        // Optimized hot-path opcodes
        case IR::Opcode::A32BXWritePC: {
            u32 val = (u32)GetArg(inst, results_ptr, 0);
            next_pc = val & ~1;
            if (val & 1) cpsr |= 0x20; else cpsr &= ~0x20;
            branched = true;
        } break;
        case IR::Opcode::A32ReadMemory8: results_ptr[inst.result_index] = this->cb->MemoryRead8((u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32ReadMemory16: results_ptr[inst.result_index] = this->cb->MemoryRead16((u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32ReadMemory32: results_ptr[inst.result_index] = this->cb->MemoryRead32((u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32ReadMemory64: results_ptr[inst.result_index] = this->cb->MemoryRead64((u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32WriteMemory8: this->cb->MemoryWrite8((u32)GetArg(inst, results_ptr, 1), (u8)GetArg(inst, results_ptr, 2)); break;
        case IR::Opcode::A32WriteMemory16: this->cb->MemoryWrite16((u32)GetArg(inst, results_ptr, 1), (u16)GetArg(inst, results_ptr, 2)); break;
        case IR::Opcode::A32WriteMemory32: this->cb->MemoryWrite32((u32)GetArg(inst, results_ptr, 1), (u32)GetArg(inst, results_ptr, 2)); break;
        case IR::Opcode::A32WriteMemory64: this->cb->MemoryWrite64((u32)GetArg(inst, results_ptr, 1), (u64)GetArg(inst, results_ptr, 2)); break;
        case IR::Opcode::A32ClearExclusive: this->exclusive_monitor.ClearExclusive(config.processor_id); break;
        case IR::Opcode::A32ExclusiveReadMemory8: results_ptr[inst.result_index] = this->exclusive_monitor.ExclusiveRead8(config.processor_id, (u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32ExclusiveReadMemory16: results_ptr[inst.result_index] = this->exclusive_monitor.ExclusiveRead16(config.processor_id, (u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32ExclusiveReadMemory32: results_ptr[inst.result_index] = this->exclusive_monitor.ExclusiveRead32(config.processor_id, (u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32ExclusiveReadMemory64: results_ptr[inst.result_index] = this->exclusive_monitor.ExclusiveRead64(config.processor_id, (u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32ExclusiveWriteMemory8: results_ptr[inst.result_index] = this->exclusive_monitor.ExclusiveWrite8(config.processor_id, (u32)GetArg(inst, results_ptr, 1), (u8)GetArg(inst, results_ptr, 2)) ? 0 : 1; break;
        case IR::Opcode::A32ExclusiveWriteMemory16: results_ptr[inst.result_index] = this->exclusive_monitor.ExclusiveWrite16(config.processor_id, (u32)GetArg(inst, results_ptr, 1), (u16)GetArg(inst, results_ptr, 2)) ? 0 : 1; break;
        case IR::Opcode::A32ExclusiveWriteMemory32: results_ptr[inst.result_index] = this->exclusive_monitor.ExclusiveWrite32(config.processor_id, (u32)GetArg(inst, results_ptr, 1), (u32)GetArg(inst, results_ptr, 2)) ? 0 : 1; break;
        case IR::Opcode::A32ExclusiveWriteMemory64: results_ptr[inst.result_index] = this->exclusive_monitor.ExclusiveWrite64(config.processor_id, (u32)GetArg(inst, results_ptr, 1), (u64)GetArg(inst, results_ptr, 2)) ? 0 : 1; break;
        case IR::Opcode::A32SetCheckBit: break;
        case IR::Opcode::A32UpdateUpperLocationDescriptor: break;
        case IR::Opcode::A32CallSupervisor: this->cb->CallSVC((u32)inst.args[0].value); break;
        case IR::Opcode::A32ExceptionRaised: this->cb->ExceptionRaised(regs[15], (Dynarmic::A32::Exception)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::SignedDiv32: {
            s32 a = (s32)GetArg(inst, results_ptr, 0);
            s32 b = (s32)GetArg(inst, results_ptr, 1);
            results_ptr[inst.result_index] = (u32)(b == 0 ? 0 : a / b);
        } break;
        case IR::Opcode::UnsignedDiv32: {
            u32 a = (u32)GetArg(inst, results_ptr, 0);
            u32 b = (u32)GetArg(inst, results_ptr, 1);
            results_ptr[inst.result_index] = (b == 0 ? 0 : a / b);
        } break;
        case IR::Opcode::And64: results_ptr[inst.result_index] = GetArg(inst, results_ptr, 0) & GetArg(inst, results_ptr, 1); break;
        case IR::Opcode::AndNot64: results_ptr[inst.result_index] = GetArg(inst, results_ptr, 0) & ~GetArg(inst, results_ptr, 1); break;
        case IR::Opcode::Or64: results_ptr[inst.result_index] = GetArg(inst, results_ptr, 0) | GetArg(inst, results_ptr, 1); break;
        case IR::Opcode::Eor64: results_ptr[inst.result_index] = GetArg(inst, results_ptr, 0) ^ GetArg(inst, results_ptr, 1); break;
        case IR::Opcode::Not64: results_ptr[inst.result_index] = ~GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::LogicalShiftLeft64: results_ptr[inst.result_index] = GetArg(inst, results_ptr, 0) << (GetArg(inst, results_ptr, 1) & 63); break;
        case IR::Opcode::LogicalShiftRight64: results_ptr[inst.result_index] = GetArg(inst, results_ptr, 0) >> (GetArg(inst, results_ptr, 1) & 63); break;
        case IR::Opcode::ArithmeticShiftRight64: results_ptr[inst.result_index] = (u64)((s64)GetArg(inst, results_ptr, 0) >> (GetArg(inst, results_ptr, 1) & 63)); break;
        case IR::Opcode::LogicalShiftLeftMasked32: results_ptr[inst.result_index] = (u32)GetArg(inst, results_ptr, 0) << (GetArg(inst, results_ptr, 1) & 31); break;
        case IR::Opcode::LogicalShiftRightMasked32: results_ptr[inst.result_index] = (u32)GetArg(inst, results_ptr, 0) >> (GetArg(inst, results_ptr, 1) & 31); break;
        case IR::Opcode::ArithmeticShiftRightMasked32: results_ptr[inst.result_index] = (u32)((s32)GetArg(inst, results_ptr, 0) >> (GetArg(inst, results_ptr, 1) & 31)); break;
        case IR::Opcode::RotateRightMasked32: {
            u32 val = (u32)GetArg(inst, results_ptr, 0);
            u32 amount = (u32)GetArg(inst, results_ptr, 1) & 31;
            results_ptr[inst.result_index] = amount == 0 ? val : (val >> amount) | (val << (32 - amount));
        } break;
        case IR::Opcode::ConditionalSelect64: results_ptr[inst.result_index] = CheckCondition(cpsr, (IR::Cond)inst.args[0].value) ? GetArg(inst, results_ptr, 1) : GetArg(inst, results_ptr, 2); break;
        case IR::Opcode::SignExtendByteToWord: results_ptr[inst.result_index] = (u32)(s32)(s8)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::SignExtendHalfToWord: results_ptr[inst.result_index] = (u32)(s32)(s16)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::ZeroExtendByteToWord: results_ptr[inst.result_index] = (u32)(u8)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::ZeroExtendHalfToWord: results_ptr[inst.result_index] = (u32)(u16)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::SignExtendByteToLong: results_ptr[inst.result_index] = (u64)(s64)(s8)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::SignExtendHalfToLong: results_ptr[inst.result_index] = (u64)(s64)(s16)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::SignExtendWordToLong: results_ptr[inst.result_index] = (u64)(s64)(s32)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::ZeroExtendByteToLong: results_ptr[inst.result_index] = (u8)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::ZeroExtendHalfToLong: results_ptr[inst.result_index] = (u16)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::ZeroExtendWordToLong: results_ptr[inst.result_index] = (u32)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::UnsignedSaturation: {
            u32 val = (u32)GetArg(inst, results_ptr, 0);
            u32 bits = (u32)GetArg(inst, results_ptr, 1);
            u32 max = (bits >= 32) ? 0xFFFFFFFF : ((1U << bits) - 1);
            u32 res = std::min(val, max);
            results_ptr[inst.result_index] = res;
            if (res != val) cpsr |= (1 << 27);
        } break;
        case IR::Opcode::SignedSaturation: {
            s32 val = (s32)GetArg(inst, results_ptr, 0);
            u32 bits = (u32)GetArg(inst, results_ptr, 1);
            s32 max = (1LL << (bits - 1)) - 1;
            s32 min = -(1LL << (bits - 1));
            s32 res = std::clamp(val, min, max);
            results_ptr[inst.result_index] = (u32)res;
            if (res != val) cpsr |= (1 << 27);
        } break;
        case IR::Opcode::ByteReverseWord: {
            u32 val = (u32)GetArg(inst, results_ptr, 0);
            results_ptr[inst.result_index] = ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val & 0xFF0000) >> 8) | ((val >> 24) & 0xFF);
        } break;
        case IR::Opcode::ByteReverseHalf: {
            u16 val = (u16)GetArg(inst, results_ptr, 0);
            results_ptr[inst.result_index] = (u16)((val >> 8) | (val << 8));
        } break;
        case IR::Opcode::RotateRight32: {
            u32 val = (u32)GetArg(inst, results_ptr, 0);
            u32 amount = (u32)GetArg(inst, results_ptr, 1) & 31;
            results_ptr[inst.result_index] = amount == 0 ? val : (val >> amount) | (val << (32 - amount));
        } break;
        case IR::Opcode::RotateRight64: {
            u64 val = GetArg(inst, results_ptr, 0);
            u64 amount = GetArg(inst, results_ptr, 1) & 63;
            results_ptr[inst.result_index] = amount == 0 ? val : (val >> amount) | (val << (64 - amount));
        } break;
        case IR::Opcode::CountLeadingZeros32: {
            u32 val = (u32)GetArg(inst, results_ptr, 0);
            results_ptr[inst.result_index] = val == 0 ? 32 : (u32)std::countl_zero(val);
        } break;
        case IR::Opcode::CountLeadingZeros64: {
            u64 val = GetArg(inst, results_ptr, 0);
            results_ptr[inst.result_index] = val == 0 ? 64 : (u32)std::countl_zero(val);
        } break;
        case IR::Opcode::LeastSignificantWord: results_ptr[inst.result_index] = GetArg(inst, results_ptr, 0) & 0xFFFFFFFF; break;
        case IR::Opcode::LeastSignificantHalf: results_ptr[inst.result_index] = (u16)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::LeastSignificantByte: results_ptr[inst.result_index] = (u8)GetArg(inst, results_ptr, 0); break;
        case IR::Opcode::MostSignificantWord: results_ptr[inst.result_index] = GetArg(inst, results_ptr, 0) >> 32; break;
        case IR::Opcode::MaxSigned32: results_ptr[inst.result_index] = (u32)std::max((s32)GetArg(inst, results_ptr, 0), (s32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::MaxUnsigned32: results_ptr[inst.result_index] = (u32)std::max((u32)GetArg(inst, results_ptr, 0), (u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::MinSigned32: results_ptr[inst.result_index] = (u32)std::min((s32)GetArg(inst, results_ptr, 0), (s32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::MinUnsigned32: results_ptr[inst.result_index] = (u32)std::min((u32)GetArg(inst, results_ptr, 0), (u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::IsZero32: results_ptr[inst.result_index] = (u32)GetArg(inst, results_ptr, 0) == 0; break;
        case IR::Opcode::TestBit: results_ptr[inst.result_index] = (GetArg(inst, results_ptr, 0) >> (u8)GetArg(inst, results_ptr, 1)) & 1; break;
        case IR::Opcode::GetCarryFromOp: results_ptr[inst.result_index] = (flags_buffer[inst.args[0].value] >> 1) & 1; break;
        case IR::Opcode::GetOverflowFromOp: results_ptr[inst.result_index] = flags_buffer[inst.args[0].value] & 1; break;
        case IR::Opcode::RotateRightExtended: {
            u32 val = (u32)GetArg(inst, results_ptr, 0);
            u32 carry = (u32)GetArg(inst, results_ptr, 1) & 1;
            results_ptr[inst.result_index] = (val >> 1) | (carry << 31);
        } break;
        case IR::Opcode::NZCVFromPackedFlags: results_ptr[inst.result_index] = (u32)GetArg(inst, results_ptr, 0) >> 28; break;
        case IR::Opcode::FPAdd32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u32>(a + b);
        } break;
        case IR::Opcode::FPAdd64: {
            double a = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u64>(a + b);
        } break;
        case IR::Opcode::FPSub32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u32>(a - b);
        } break;
        case IR::Opcode::FPSub64: {
            double a = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u64>(a - b);
        } break;
        case IR::Opcode::FPMul32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u32>(a * b);
        } break;
        case IR::Opcode::FPMul64: {
            double a = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u64>(a * b);
        } break;
        case IR::Opcode::FPMulAdd32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            float c = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 2));
            results_ptr[inst.result_index] = std::bit_cast<u32>(a + (b * c));
        } break;
        case IR::Opcode::FPMulAdd64: {
            double a = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 1));
            double c = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 2));
            results_ptr[inst.result_index] = std::bit_cast<u64>(a + (b * c));
        } break;
        case IR::Opcode::FPMulSub32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            float c = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 2));
            results_ptr[inst.result_index] = std::bit_cast<u32>(a - (b * c));
        } break;
        case IR::Opcode::FPMulSub64: {
            double a = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 1));
            double c = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 2));
            results_ptr[inst.result_index] = std::bit_cast<u64>(a - (b * c));
        } break;
        case IR::Opcode::FPDiv32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u32>(a / b);
        } break;
        case IR::Opcode::FPDiv64: {
            double a = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u64>(a / b);
        } break;
        case IR::Opcode::FPMax32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u32>(std::max(a, b));
        } break;
        case IR::Opcode::FPMax64: {
            double a = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u64>(std::max(a, b));
        } break;
        case IR::Opcode::FPMin32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u32>(std::min(a, b));
        } break;
        case IR::Opcode::FPMin64: {
            double a = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u64>(std::min(a, b));
        } break;
        case IR::Opcode::FPAbs32: results_ptr[inst.result_index] = (u32)GetArg(inst, results_ptr, 0) & 0x7FFFFFFF; break;
        case IR::Opcode::FPAbs64: results_ptr[inst.result_index] = GetArg(inst, results_ptr, 0) & 0x7FFFFFFFFFFFFFFFULL; break;
        case IR::Opcode::FPNeg32: results_ptr[inst.result_index] = (u32)GetArg(inst, results_ptr, 0) ^ 0x80000000; break;
        case IR::Opcode::FPNeg64: results_ptr[inst.result_index] = GetArg(inst, results_ptr, 0) ^ 0x8000000000000000ULL; break;
        case IR::Opcode::FPSqrt32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            results_ptr[inst.result_index] = std::bit_cast<u32>(std::sqrt(a));
        } break;
        case IR::Opcode::FPSqrt64: {
            double a = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 0));
            results_ptr[inst.result_index] = std::bit_cast<u64>((double)std::sqrt(a));
        } break;
        case IR::Opcode::FPCompare32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            if (std::isnan(a) || std::isnan(b)) results_ptr[inst.result_index] = 3;
            else if (a == b) results_ptr[inst.result_index] = 0;
            else if (a < b) results_ptr[inst.result_index] = 1;
            else results_ptr[inst.result_index] = 2;
        } break;
        case IR::Opcode::FPCompare64: {
            double a = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>((u64)GetArg(inst, results_ptr, 1));
            if (std::isnan(a) || std::isnan(b)) results_ptr[inst.result_index] = 3;
            else if (a == b) results_ptr[inst.result_index] = 0;
            else if (a < b) results_ptr[inst.result_index] = 1;
            else results_ptr[inst.result_index] = 2;
        } break;
        case IR::Opcode::FPDoubleToSingle: results_ptr[inst.result_index] = std::bit_cast<u32>((float)std::bit_cast<double>((u64)GetArg(inst, results_ptr, 0))); break;
        case IR::Opcode::FPSingleToDouble: results_ptr[inst.result_index] = std::bit_cast<u64>((double)std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0))); break;
        case IR::Opcode::SignedSaturatedAdd32: {
            s32 a = (s32)GetArg(inst, results_ptr, 0);
            s32 b = (s32)GetArg(inst, results_ptr, 1);
            s32 res;
            if (__builtin_add_overflow(a, b, &res)) {
                res = (a < 0) ? std::numeric_limits<s32>::min() : std::numeric_limits<s32>::max();
                cpsr |= (1 << 27);
            }
            results_ptr[inst.result_index] = (u32)res;
        } break;
        case IR::Opcode::SignedSaturatedSub32: {
            s32 a = (s32)GetArg(inst, results_ptr, 0);
            s32 b = (s32)GetArg(inst, results_ptr, 1);
            s32 res;
            if (__builtin_sub_overflow(a, b, &res)) {
                res = (a < 0) ? std::numeric_limits<s32>::min() : std::numeric_limits<s32>::max();
                cpsr |= (1 << 27);
            }
            results_ptr[inst.result_index] = (u32)res;
        } break;
        case IR::Opcode::UnsignedSaturatedAdd32: {
            u32 a = (u32)GetArg(inst, results_ptr, 0);
            u32 b = (u32)GetArg(inst, results_ptr, 1);
            u32 res;
            if (__builtin_add_overflow(a, b, &res)) {
                res = 0xFFFFFFFF;
                cpsr |= (1 << 27);
            }
            results_ptr[inst.result_index] = res;
        } break;
        case IR::Opcode::UnsignedSaturatedSub32: {
            u32 a = (u32)GetArg(inst, results_ptr, 0);
            u32 b = (u32)GetArg(inst, results_ptr, 1);
            if (a < b) {
                results_ptr[inst.result_index] = 0;
                cpsr |= (1 << 27);
            } else {
                results_ptr[inst.result_index] = a - b;
            }
        } break;
        case IR::Opcode::GetCFlagFromNZCV: results_ptr[inst.result_index] = (GetArg(inst, results_ptr, 0) >> 1) & 1; break;
        default:
            LOG_TRACE(Core_ARM11, "Unimplemented IR opcode: {}", (int)inst.op);
            break;
        }
        if (branched) break;
    }
    regs[15] = next_pc;
    this->cb->AddTicks(std::max<u64>(1, (u64)executed_count * 2));
}


} // namespace Core
