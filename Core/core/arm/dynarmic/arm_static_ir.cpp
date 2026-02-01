// Copyright 2024 Jules
// Licensed under GPLv2 or any later version

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
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
void ARM_StaticIR::ClearInstructionCache() { block_cache.clear(); }
void ARM_StaticIR::InvalidateCacheRange(u32 start_address, std::size_t length) {
    for (auto it = block_cache.begin(); it != block_cache.end(); ) {
        if (it->first >= start_address && it->first < start_address + length) {
            it = block_cache.erase(it);
        } else {
            ++it;
        }
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
    return block_cache.emplace(pc, std::move(tb)).first->second;
}

namespace {
typedef void (*OpHandler)(ARM_StaticIR& self, const ARM_StaticIR::Instruction& inst, u64* results, u32& next_pc, bool& branched);

static inline u64 GetArg(const ARM_StaticIR::Instruction& inst, const u64* results, size_t i) {
    if (inst.args[i].kind == ARM_StaticIR::Operand::Immediate) return inst.args[i].value;
    return results[inst.args[i].value];
}

#define OP_HANDLER(name) static void Handle##name(ARM_StaticIR& self, const ARM_StaticIR::Instruction& inst, u64* results, u32& next_pc, bool& branched)

OP_HANDLER(A32GetRegister) { results[inst.result_index] = self.GetReg((int)inst.args[0].value); }
OP_HANDLER(A32SetRegister) { self.SetReg((int)inst.args[0].value, (u32)GetArg(inst, results, 1)); }
OP_HANDLER(A32GetExtendedRegister32) { results[inst.result_index] = self.GetVFPReg((int)inst.args[0].value); }
OP_HANDLER(A32SetExtendedRegister32) { self.SetVFPReg((int)inst.args[0].value, (u32)GetArg(inst, results, 1)); }
OP_HANDLER(A32GetCpsr) { results[inst.result_index] = self.GetCPSR(); }
OP_HANDLER(A32SetCpsr) { self.SetCPSR((u32)GetArg(inst, results, 0)); }
OP_HANDLER(A32SetCpsrNZCV) { self.SetCPSR((self.GetCPSR() & 0x0FFFFFFF) | ((u32)GetArg(inst, results, 0) << 28)); }
OP_HANDLER(Add32) { results[inst.result_index] = (u32)GetArg(inst, results, 0) + (u32)GetArg(inst, results, 1); }
OP_HANDLER(Add64) { results[inst.result_index] = GetArg(inst, results, 0) + GetArg(inst, results, 1); }
OP_HANDLER(Sub32) { results[inst.result_index] = (u32)GetArg(inst, results, 0) - (u32)GetArg(inst, results, 1); }
OP_HANDLER(Sub64) { results[inst.result_index] = GetArg(inst, results, 0) - GetArg(inst, results, 1); }
OP_HANDLER(Mul32) { results[inst.result_index] = (u32)GetArg(inst, results, 0) * (u32)GetArg(inst, results, 1); }
OP_HANDLER(And32) { results[inst.result_index] = (u32)GetArg(inst, results, 0) & (u32)GetArg(inst, results, 1); }
OP_HANDLER(And64) { results[inst.result_index] = GetArg(inst, results, 0) & GetArg(inst, results, 1); }
OP_HANDLER(Or32) { results[inst.result_index] = (u32)GetArg(inst, results, 0) | (u32)GetArg(inst, results, 1); }
OP_HANDLER(Or64) { results[inst.result_index] = GetArg(inst, results, 0) | GetArg(inst, results, 1); }
OP_HANDLER(Eor32) { results[inst.result_index] = (u32)GetArg(inst, results, 0) ^ (u32)GetArg(inst, results, 1); }
OP_HANDLER(Eor64) { results[inst.result_index] = GetArg(inst, results, 0) ^ GetArg(inst, results, 1); }
OP_HANDLER(Not32) { results[inst.result_index] = ~(u32)GetArg(inst, results, 0); }
OP_HANDLER(Not64) { results[inst.result_index] = ~GetArg(inst, results, 0); }
OP_HANDLER(LogicalShiftLeft32) { results[inst.result_index] = (u32)GetArg(inst, results, 0) << (GetArg(inst, results, 1) & 31); }
OP_HANDLER(LogicalShiftLeft64) { results[inst.result_index] = GetArg(inst, results, 0) << (GetArg(inst, results, 1) & 63); }
OP_HANDLER(LogicalShiftRight32) { results[inst.result_index] = (u32)GetArg(inst, results, 0) >> (GetArg(inst, results, 1) & 31); }
OP_HANDLER(LogicalShiftRight64) { results[inst.result_index] = GetArg(inst, results, 0) >> (GetArg(inst, results, 1) & 63); }
OP_HANDLER(ArithmeticShiftRight32) { results[inst.result_index] = (u32)((s32)GetArg(inst, results, 0) >> (GetArg(inst, results, 1) & 31)); }
OP_HANDLER(ArithmeticShiftRight64) { results[inst.result_index] = (u64)((s64)GetArg(inst, results, 0) >> (GetArg(inst, results, 1) & 63)); }
OP_HANDLER(A32ReadMemory8) { results[inst.result_index] = self.GetCallbacks().MemoryRead8((u32)GetArg(inst, results, 0)); }
OP_HANDLER(A32ReadMemory16) { results[inst.result_index] = self.GetCallbacks().MemoryRead16((u32)GetArg(inst, results, 0)); }
OP_HANDLER(A32ReadMemory32) { results[inst.result_index] = self.GetCallbacks().MemoryRead32((u32)GetArg(inst, results, 0)); }
OP_HANDLER(A32ReadMemory64) { results[inst.result_index] = self.GetCallbacks().MemoryRead64((u32)GetArg(inst, results, 0)); }
OP_HANDLER(A32WriteMemory8) { self.GetCallbacks().MemoryWrite8((u32)GetArg(inst, results, 0), (u8)GetArg(inst, results, 1)); }
OP_HANDLER(A32WriteMemory16) { self.GetCallbacks().MemoryWrite16((u32)GetArg(inst, results, 0), (u16)GetArg(inst, results, 1)); }
OP_HANDLER(A32WriteMemory32) { self.GetCallbacks().MemoryWrite32((u32)GetArg(inst, results, 0), (u32)GetArg(inst, results, 1)); }
OP_HANDLER(A32WriteMemory64) { self.GetCallbacks().MemoryWrite64((u32)GetArg(inst, results, 0), GetArg(inst, results, 1)); }
OP_HANDLER(A32BXWritePC) {
    u32 val = (u32)GetArg(inst, results, 0);
    next_pc = val & ~1;
    if (val & 1) self.SetCPSR(self.GetCPSR() | 0x20); else self.SetCPSR(self.GetCPSR() & ~0x20);
    branched = true;
}
OP_HANDLER(A32UpdateUpperLocationDescriptor) {}
OP_HANDLER(A32CallSupervisor) { self.GetCallbacks().CallSVC((u32)inst.args[0].value); }
OP_HANDLER(ConditionalSelect32) {
    bool cond_met = false;
    u32 cpsr = self.GetCPSR();
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
    results[inst.result_index] = cond_met ? GetArg(inst, results, 1) : GetArg(inst, results, 2);
}
OP_HANDLER(SignExtendByteToWord) { results[inst.result_index] = (u32)(s32)(s8)GetArg(inst, results, 0); }
OP_HANDLER(SignExtendHalfToWord) { results[inst.result_index] = (u32)(s32)(s16)GetArg(inst, results, 0); }
OP_HANDLER(ZeroExtendByteToWord) { results[inst.result_index] = (u32)(u8)GetArg(inst, results, 0); }
OP_HANDLER(ZeroExtendHalfToWord) { results[inst.result_index] = (u32)(u16)GetArg(inst, results, 0); }
OP_HANDLER(ByteReverseWord) {
    u32 val = (u32)GetArg(inst, results, 0);
    results[inst.result_index] = ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val & 0xFF0000) >> 8) | ((val >> 24) & 0xFF);
}
OP_HANDLER(RotateRight32) {
    u32 val = (u32)GetArg(inst, results, 0);
    u32 amount = (u32)GetArg(inst, results, 1) & 31;
    results[inst.result_index] = amount == 0 ? val : (val >> amount) | (val << (32 - amount));
}
OP_HANDLER(RotateRight64) {
    u64 val = GetArg(inst, results, 0);
    u64 amount = GetArg(inst, results, 1) & 63;
    results[inst.result_index] = amount == 0 ? val : (val >> amount) | (val << (64 - amount));
}
OP_HANDLER(CountLeadingZeros32) {
    u32 val = (u32)GetArg(inst, results, 0);
    results[inst.result_index] = val == 0 ? 32 : __builtin_clz(val);
}
OP_HANDLER(Pack2x32To1x64) { results[inst.result_index] = (GetArg(inst, results, 0) & 0xFFFFFFFF) | (GetArg(inst, results, 1) << 32); }
OP_HANDLER(LeastSignificantWord) { results[inst.result_index] = GetArg(inst, results, 0) & 0xFFFFFFFF; }
OP_HANDLER(MostSignificantWord) { results[inst.result_index] = GetArg(inst, results, 0) >> 32; }
OP_HANDLER(A32GetExtendedRegister64) {
    results[inst.result_index] = ((u64)self.GetVFPReg((int)inst.args[0].value + 1) << 32) | self.GetVFPReg((int)inst.args[0].value);
}
OP_HANDLER(A32SetExtendedRegister64) {
    u64 val = GetArg(inst, results, 1);
    self.SetVFPReg((int)inst.args[0].value, (u32)val);
    self.SetVFPReg((int)inst.args[0].value + 1, (u32)(val >> 32));
}
OP_HANDLER(GetNZCVFromOp) { results[inst.result_index] = self.GetCPSR() >> 28; }

OP_HANDLER(FPAdd32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    float b = std::bit_cast<float>((u32)GetArg(inst, results, 1));
    results[inst.result_index] = std::bit_cast<u32>(a + b);
}
OP_HANDLER(FPAdd64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    double b = std::bit_cast<double>(GetArg(inst, results, 1));
    results[inst.result_index] = std::bit_cast<u64>(a + b);
}
OP_HANDLER(FPSub32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    float b = std::bit_cast<float>((u32)GetArg(inst, results, 1));
    results[inst.result_index] = std::bit_cast<u32>(a - b);
}
OP_HANDLER(FPSub64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    double b = std::bit_cast<double>(GetArg(inst, results, 1));
    results[inst.result_index] = std::bit_cast<u64>(a - b);
}
OP_HANDLER(FPMul32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    float b = std::bit_cast<float>((u32)GetArg(inst, results, 1));
    results[inst.result_index] = std::bit_cast<u32>(a * b);
}
OP_HANDLER(FPMul64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    double b = std::bit_cast<double>(GetArg(inst, results, 1));
    results[inst.result_index] = std::bit_cast<u64>(a * b);
}
OP_HANDLER(FPDiv32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    float b = std::bit_cast<float>((u32)GetArg(inst, results, 1));
    results[inst.result_index] = std::bit_cast<u32>(a / b);
}
OP_HANDLER(FPDiv64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    double b = std::bit_cast<double>(GetArg(inst, results, 1));
    results[inst.result_index] = std::bit_cast<u64>(a / b);
}
OP_HANDLER(FPAbs32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    results[inst.result_index] = std::bit_cast<u32>(std::abs(a));
}
OP_HANDLER(FPAbs64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    results[inst.result_index] = std::bit_cast<u64>(std::abs(a));
}
OP_HANDLER(FPNeg32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    results[inst.result_index] = std::bit_cast<u32>(-a);
}
OP_HANDLER(FPNeg64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    results[inst.result_index] = std::bit_cast<u64>(-a);
}
OP_HANDLER(FPSqrt32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    results[inst.result_index] = std::bit_cast<u32>(std::sqrt(a));
}
OP_HANDLER(FPSqrt64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    results[inst.result_index] = std::bit_cast<u64>(std::sqrt(a));
}
OP_HANDLER(FPCompare32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    float b = std::bit_cast<float>((u32)GetArg(inst, results, 1));
    if (std::isnan(a) || std::isnan(b)) results[inst.result_index] = 3; // Unordered
    else if (a == b) results[inst.result_index] = 0; // Equal
    else if (a < b) results[inst.result_index] = 1; // Less than
    else results[inst.result_index] = 2; // Greater than
}
OP_HANDLER(FPCompare64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    double b = std::bit_cast<double>(GetArg(inst, results, 1));
    if (std::isnan(a) || std::isnan(b)) results[inst.result_index] = 3;
    else if (a == b) results[inst.result_index] = 0;
    else if (a < b) results[inst.result_index] = 1;
    else results[inst.result_index] = 2;
}
OP_HANDLER(FPSingleToFixedS32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    results[inst.result_index] = (u32)(s32)a;
}
OP_HANDLER(FPSingleToFixedU32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    results[inst.result_index] = (u32)(u32)a;
}
OP_HANDLER(FPDoubleToFixedS64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    results[inst.result_index] = (u64)(s64)a;
}
OP_HANDLER(FPDoubleToFixedU64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    results[inst.result_index] = (u64)a;
}
OP_HANDLER(FPFixedS32ToSingle) {
    s32 a = (s32)GetArg(inst, results, 0);
    results[inst.result_index] = std::bit_cast<u32>((float)a);
}
OP_HANDLER(FPFixedU32ToSingle) {
    u32 a = (u32)GetArg(inst, results, 0);
    results[inst.result_index] = std::bit_cast<u32>((float)a);
}
OP_HANDLER(FPFixedS64ToDouble) {
    s64 a = (s64)GetArg(inst, results, 0);
    results[inst.result_index] = std::bit_cast<u64>((double)a);
}
OP_HANDLER(FPFixedU64ToDouble) {
    u64 a = GetArg(inst, results, 0);
    results[inst.result_index] = std::bit_cast<u64>((double)a);
}
OP_HANDLER(FPDoubleToSingle) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    results[inst.result_index] = std::bit_cast<u32>((float)a);
}
OP_HANDLER(FPSingleToDouble) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    results[inst.result_index] = std::bit_cast<u64>((double)a);
}

static const auto op_handlers = []() {
        std::array<OpHandler, 256> table;
        for (int i = 0; i < 256; ++i) table[i] = [](ARM_StaticIR&, const ARM_StaticIR::Instruction& inst, u64*, u32&, bool&) {
            LOG_TRACE(Core_ARM11, "Unimplemented IR opcode: {}", (int)inst.op);
        };
        table[(int)IR::Opcode::A32GetRegister] = HandleA32GetRegister;
        table[(int)IR::Opcode::A32SetRegister] = HandleA32SetRegister;
        table[(int)IR::Opcode::A32GetExtendedRegister32] = HandleA32GetExtendedRegister32;
        table[(int)IR::Opcode::A32SetExtendedRegister32] = HandleA32SetExtendedRegister32;
        table[(int)IR::Opcode::A32GetCpsr] = HandleA32GetCpsr;
        table[(int)IR::Opcode::A32SetCpsr] = HandleA32SetCpsr;
        table[(int)IR::Opcode::A32SetCpsrNZCV] = HandleA32SetCpsrNZCV;
        table[(int)IR::Opcode::Add32] = HandleAdd32;
        table[(int)IR::Opcode::Add64] = HandleAdd64;
        table[(int)IR::Opcode::Sub32] = HandleSub32;
        table[(int)IR::Opcode::Sub64] = HandleSub64;
        table[(int)IR::Opcode::Mul32] = HandleMul32;
        table[(int)IR::Opcode::And32] = HandleAnd32;
        table[(int)IR::Opcode::And64] = HandleAnd64;
        table[(int)IR::Opcode::Or32] = HandleOr32;
        table[(int)IR::Opcode::Or64] = HandleOr64;
        table[(int)IR::Opcode::Eor32] = HandleEor32;
        table[(int)IR::Opcode::Eor64] = HandleEor64;
        table[(int)IR::Opcode::Not32] = HandleNot32;
        table[(int)IR::Opcode::Not64] = HandleNot64;
        table[(int)IR::Opcode::LogicalShiftLeft32] = HandleLogicalShiftLeft32;
        table[(int)IR::Opcode::LogicalShiftLeft64] = HandleLogicalShiftLeft64;
        table[(int)IR::Opcode::LogicalShiftRight32] = HandleLogicalShiftRight32;
        table[(int)IR::Opcode::LogicalShiftRight64] = HandleLogicalShiftRight64;
        table[(int)IR::Opcode::ArithmeticShiftRight32] = HandleArithmeticShiftRight32;
        table[(int)IR::Opcode::ArithmeticShiftRight64] = HandleArithmeticShiftRight64;
        table[(int)IR::Opcode::A32ReadMemory8] = HandleA32ReadMemory8;
        table[(int)IR::Opcode::A32ReadMemory16] = HandleA32ReadMemory16;
        table[(int)IR::Opcode::A32ReadMemory32] = HandleA32ReadMemory32;
        table[(int)IR::Opcode::A32ReadMemory64] = HandleA32ReadMemory64;
        table[(int)IR::Opcode::A32WriteMemory8] = HandleA32WriteMemory8;
        table[(int)IR::Opcode::A32WriteMemory16] = HandleA32WriteMemory16;
        table[(int)IR::Opcode::A32WriteMemory32] = HandleA32WriteMemory32;
        table[(int)IR::Opcode::A32WriteMemory64] = HandleA32WriteMemory64;
        table[(int)IR::Opcode::A32BXWritePC] = HandleA32BXWritePC;
        table[(int)IR::Opcode::A32UpdateUpperLocationDescriptor] = HandleA32UpdateUpperLocationDescriptor;
        table[(int)IR::Opcode::A32CallSupervisor] = HandleA32CallSupervisor;
        table[(int)IR::Opcode::ConditionalSelect32] = HandleConditionalSelect32;
        table[(int)IR::Opcode::SignExtendByteToWord] = HandleSignExtendByteToWord;
        table[(int)IR::Opcode::SignExtendHalfToWord] = HandleSignExtendHalfToWord;
        table[(int)IR::Opcode::ZeroExtendByteToWord] = HandleZeroExtendByteToWord;
        table[(int)IR::Opcode::ZeroExtendHalfToWord] = HandleZeroExtendHalfToWord;
        table[(int)IR::Opcode::ByteReverseWord] = HandleByteReverseWord;
        table[(int)IR::Opcode::RotateRight32] = HandleRotateRight32;
        table[(int)IR::Opcode::RotateRight64] = HandleRotateRight64;
        table[(int)IR::Opcode::CountLeadingZeros32] = HandleCountLeadingZeros32;
        table[(int)IR::Opcode::Pack2x32To1x64] = HandlePack2x32To1x64;
        table[(int)IR::Opcode::LeastSignificantWord] = HandleLeastSignificantWord;
        table[(int)IR::Opcode::MostSignificantWord] = HandleMostSignificantWord;
        table[(int)IR::Opcode::A32GetExtendedRegister64] = HandleA32GetExtendedRegister64;
        table[(int)IR::Opcode::A32SetExtendedRegister64] = HandleA32SetExtendedRegister64;
        table[(int)IR::Opcode::GetNZCVFromOp] = HandleGetNZCVFromOp;
        table[(int)IR::Opcode::FPAdd32] = HandleFPAdd32;
        table[(int)IR::Opcode::FPAdd64] = HandleFPAdd64;
        table[(int)IR::Opcode::FPSub32] = HandleFPSub32;
        table[(int)IR::Opcode::FPSub64] = HandleFPSub64;
        table[(int)IR::Opcode::FPMul32] = HandleFPMul32;
        table[(int)IR::Opcode::FPMul64] = HandleFPMul64;
        table[(int)IR::Opcode::FPDiv32] = HandleFPDiv32;
        table[(int)IR::Opcode::FPDiv64] = HandleFPDiv64;
        table[(int)IR::Opcode::FPAbs32] = HandleFPAbs32;
        table[(int)IR::Opcode::FPAbs64] = HandleFPAbs64;
        table[(int)IR::Opcode::FPNeg32] = HandleFPNeg32;
        table[(int)IR::Opcode::FPNeg64] = HandleFPNeg64;
        table[(int)IR::Opcode::FPSqrt32] = HandleFPSqrt32;
        table[(int)IR::Opcode::FPSqrt64] = HandleFPSqrt64;
        table[(int)IR::Opcode::FPCompare32] = HandleFPCompare32;
        table[(int)IR::Opcode::FPCompare64] = HandleFPCompare64;
        table[(int)IR::Opcode::FPSingleToFixedS32] = HandleFPSingleToFixedS32;
        table[(int)IR::Opcode::FPSingleToFixedU32] = HandleFPSingleToFixedU32;
        table[(int)IR::Opcode::FPDoubleToFixedS64] = HandleFPDoubleToFixedS64;
        table[(int)IR::Opcode::FPDoubleToFixedU64] = HandleFPDoubleToFixedU64;
        table[(int)IR::Opcode::FPFixedS32ToSingle] = HandleFPFixedS32ToSingle;
        table[(int)IR::Opcode::FPFixedU32ToSingle] = HandleFPFixedU32ToSingle;
        table[(int)IR::Opcode::FPFixedS64ToDouble] = HandleFPFixedS64ToDouble;
        table[(int)IR::Opcode::FPFixedU64ToDouble] = HandleFPFixedU64ToDouble;
        table[(int)IR::Opcode::FPDoubleToSingle] = HandleFPDoubleToSingle;
        table[(int)IR::Opcode::FPSingleToDouble] = HandleFPSingleToDouble;
        return table;
    }();
} // namespace

void ARM_StaticIR::ExecuteBlock(const TranslatedBlock& block) {
    size_t num_insts = block.instructions.size();
    if (results_buffer.size() < num_insts) {
        results_buffer.resize(std::max<size_t>(num_insts, 512));
    }
    u64* results_ptr = results_buffer.data();

    u32 next_pc = block.guest_end_pc;
    bool branched = false;

    for (const auto& inst : block.instructions) {
        op_handlers[(int)inst.op](*this, inst, results_ptr, next_pc, branched);
    }
    regs[15] = next_pc;
    cb->AddTicks(10);
}

} // namespace Core
