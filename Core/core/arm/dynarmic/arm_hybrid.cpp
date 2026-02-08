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
#include "common/swap.h"
#include "core/arm/dynarmic/arm_dynarmic_cp15.h"
#include "core/arm/dynarmic/arm_exclusive_monitor.h"
#include "core/arm/dynarmic/arm_hybrid.h"
#include "core/arm/dynarmic/arm_tick_counts.h"
#include "core/arm/dyncom/arm_dyncom_interpreter.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/svc.h"
#include "core/memory.h"

namespace Core {

using namespace Dynarmic;

class ARM_Hybrid_Callbacks final : public Dynarmic::A32::UserCallbacks {
public:
    explicit ARM_Hybrid_Callbacks(ARM_Hybrid& parent)
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
        return parent.exclusive_monitor.ExclusiveWrite8(parent.config.processor_id, vaddr, value);
    }
    bool MemoryWriteExclusive16(Dynarmic::A32::VAddr vaddr, u16 value, u16 expected) override {
        return parent.exclusive_monitor.ExclusiveWrite16(parent.config.processor_id, vaddr, value);
    }
    bool MemoryWriteExclusive32(Dynarmic::A32::VAddr vaddr, u32 value, u32 expected) override {
        return parent.exclusive_monitor.ExclusiveWrite32(parent.config.processor_id, vaddr, value);
    }
    bool MemoryWriteExclusive64(Dynarmic::A32::VAddr vaddr, u64 value, u64 expected) override {
        return parent.exclusive_monitor.ExclusiveWrite64(parent.config.processor_id, vaddr, value);
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
        u64 ticks = Core::TicksForInstruction(is_thumb, instruction);
        ticks_accumulator += ticks;
        return ticks;
    }

    void ResetAccumulator() { ticks_accumulator = 0; }
    u64 GetAccumulatedTicks() const { return ticks_accumulator; }

    ARM_Hybrid& parent;
    u64 ticks_accumulator = 0;
    Memory::MemorySystem& memory;
    Kernel::SVCContext svc_context;
};

ARM_Hybrid::ARM_Hybrid(Core::System& system_, Memory::MemorySystem& memory_, u32 core_id_,
                           std::shared_ptr<Core::Timing::Timer> timer_,
                           Core::ExclusiveMonitor& exclusive_monitor_)
    : ARM_Interface(core_id_, timer_), system(system_), memory(memory_),
      cb(std::make_unique<ARM_Hybrid_Callbacks>(*this)),
      exclusive_monitor{dynamic_cast<Core::DynarmicExclusiveMonitor&>(exclusive_monitor_)} {

    state = std::make_unique<ARMul_State>(system, memory, USER32MODE);

    config.callbacks = cb.get();
    config.processor_id = core_id_;
    config.arch_version = Dynarmic::A32::ArchVersion::v6K; // ARM11 is ARMv6K
}

ARM_Hybrid::~ARM_Hybrid() = default;

namespace {

using IRHandler = ARM_Hybrid::IRHandler;
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


static inline void SetNZ(u32& flags, u32 res) {
    flags &= ~0xC;
    if (res & 0x80000000) flags |= 0x8;
    if (res == 0) flags |= 0x4;
}

static inline void SetNZ64(u32& flags, u64 res) {
    flags &= ~0xC;
    if (res & 0x8000000000000000ULL) flags |= 0x8;
    if (res == 0) flags |= 0x4;
}

static inline u32 CalculateNZCV32_Add(u32 a, u32 b, u32 res, bool carry) {
    u32 nzcv = 0;
    if (res & 0x80000000) nzcv |= 8;
    if (res == 0) nzcv |= 4;
    if (carry) nzcv |= 2;
    if ((~(a ^ b) & (a ^ res)) & 0x80000000) nzcv |= 1;
    return nzcv;
}

static inline u32 CalculateNZCV32_Sub(u32 a, u32 b, u32 res, bool carry) {
    u32 nzcv = 0;
    if (res & 0x80000000) nzcv |= 8;
    if (res == 0) nzcv |= 4;
    if (carry) nzcv |= 2;
    if (((a ^ b) & (a ^ res)) & 0x80000000) nzcv |= 1;
    return nzcv;
}

static inline u32 CalculateNZCV32_Logic(u32 res, bool carry) {
    u32 nzcv = 0;
    if (res & 0x80000000) nzcv |= 8;
    if (res == 0) nzcv |= 4;
    if (carry) nzcv |= 2;
    // V is unaffected
    return nzcv;
}
} // namespace


static inline U128 GetArg(const ARM_Hybrid::Instruction& inst, const U128* results, size_t i) {
    if (inst.args[i].kind == ARM_Hybrid::Operand::Immediate) return U128(inst.args[i].value);
    return results[inst.args[i].value];
}

#define HANDLER(name) static void Handler_##name(ARM_Hybrid& cpu, const ARM_Hybrid::Instruction& inst, U128* results)

HANDLER(A32GetRegister) { results[inst.result_index] = U128(cpu.GetReg((int)inst.args[0].value)); }
HANDLER(A32SetRegister) {
    u32 val = GetArg(inst, results, 1).lo;
    int reg = (int)inst.args[0].value;
    cpu.SetReg(reg, val);
}
HANDLER(A32GetCpsr) { results[inst.result_index] = U128(cpu.GetCPSR()); }
HANDLER(A32SetCpsr) { cpu.SetCPSR(GetArg(inst, results, 0).lo); }

HANDLER(Add32) {
    u32 a = (u32)GetArg(inst, results, 0).lo;
    u32 b = (u32)GetArg(inst, results, 1).lo;
    u32 carry_in = GetArg(inst, results, 2).lo & 1;
    u64 res64 = (u64)a + b + carry_in;
    u32 res = (u32)res64;
    results[inst.result_index] = U128(res);
    cpu.flags_buffer[inst.result_index] = CalculateNZCV32_Add(a, b, res, res64 >> 32);
}

HANDLER(Sub32) {
    u32 a = (u32)GetArg(inst, results, 0).lo;
    u32 b = (u32)GetArg(inst, results, 1).lo;
    u32 carry_in = GetArg(inst, results, 2).lo & 1;
    u64 res64 = (u64)a + ~((u64)b) + carry_in;
    u32 res = (u32)res64;
    results[inst.result_index] = U128(res);
    cpu.flags_buffer[inst.result_index] = CalculateNZCV32_Sub(a, b, res, res64 >> 32);
}

HANDLER(And32) {
    u32 res = (u32)GetArg(inst, results, 0).lo & (u32)GetArg(inst, results, 1).lo;
    results[inst.result_index] = U128(res);
    cpu.flags_buffer[inst.result_index] = CalculateNZCV32_Logic(res, (cpu.GetCPSR() >> 29) & 1);
}

HANDLER(Or32) {
    u32 res = (u32)GetArg(inst, results, 0).lo | (u32)GetArg(inst, results, 1).lo;
    results[inst.result_index] = U128(res);
    cpu.flags_buffer[inst.result_index] = CalculateNZCV32_Logic(res, (cpu.GetCPSR() >> 29) & 1);
}

HANDLER(Eor32) {
    u32 res = (u32)GetArg(inst, results, 0).lo ^ (u32)GetArg(inst, results, 1).lo;
    results[inst.result_index] = U128(res);
    cpu.flags_buffer[inst.result_index] = CalculateNZCV32_Logic(res, (cpu.GetCPSR() >> 29) & 1);
}

HANDLER(LSL32) {
    u32 val = (u32)GetArg(inst, results, 0).lo;
    u32 shift = (u32)GetArg(inst, results, 1).lo & 0xFF;
    u32 res = (shift >= 32) ? 0 : (val << shift);
    results[inst.result_index] = U128(res);
    bool carry = (shift == 0) ? ((cpu.GetCPSR() >> 29) & 1) : (shift > 32 ? 0 : ((val >> (32 - shift)) & 1));
    cpu.flags_buffer[inst.result_index] = CalculateNZCV32_Logic(res, carry);
}

HANDLER(LSR32) {
    u32 val = (u32)GetArg(inst, results, 0).lo;
    u32 shift = (u32)GetArg(inst, results, 1).lo & 0xFF;
    u32 res = (shift >= 32) ? 0 : (val >> shift);
    results[inst.result_index] = U128(res);
    bool carry = (shift == 0) ? ((cpu.GetCPSR() >> 29) & 1) : (shift > 32 ? 0 : ((val >> (shift - 1)) & 1));
    cpu.flags_buffer[inst.result_index] = CalculateNZCV32_Logic(res, carry);
}

HANDLER(ASR32) {
    u32 val = (u32)GetArg(inst, results, 0).lo;
    u32 shift = (u32)GetArg(inst, results, 1).lo & 0xFF;
    u32 res = (shift >= 32) ? ((val & 0x80000000) ? 0xFFFFFFFF : 0) : (u32)((s32)val >> shift);
    results[inst.result_index] = U128(res);
    bool carry = (shift == 0) ? ((cpu.GetCPSR() >> 29) & 1) : (shift > 32 ? ((val >> 31) & 1) : ((val >> (shift - 1)) & 1));
    cpu.flags_buffer[inst.result_index] = CalculateNZCV32_Logic(res, carry);
}

HANDLER(ROR32) {
    u32 val = (u32)GetArg(inst, results, 0).lo;
    u32 shift = (u32)GetArg(inst, results, 1).lo & 31;
    u32 res = (shift == 0) ? val : ((val >> shift) | (val << (32 - shift)));
    results[inst.result_index] = U128(res);
    bool carry = (shift == 0) ? ((cpu.GetCPSR() >> 29) & 1) : ((val >> (shift - 1)) & 1);
    cpu.flags_buffer[inst.result_index] = CalculateNZCV32_Logic(res, carry);
}

HANDLER(A32GetExtendedRegister32) { results[inst.result_index] = U128(cpu.GetVFPReg((int)inst.args[0].value)); }
HANDLER(A32SetExtendedRegister32) { cpu.SetVFPReg((int)inst.args[0].value, GetArg(inst, results, 1).lo); }
HANDLER(A32GetExtendedRegister64) {
    int reg = (int)inst.args[0].value;
    int idx = (reg - 32) * 2;
    results[inst.result_index] = U128(((u64)cpu.GetVFPReg(idx + 1) << 32) | cpu.GetVFPReg(idx));
}
HANDLER(A32SetExtendedRegister64) {
    int reg = (int)inst.args[0].value;
    int idx = (reg - 32) * 2;
    u64 val = GetArg(inst, results, 1).lo;
    cpu.SetVFPReg(idx, (u32)val);
    cpu.SetVFPReg(idx + 1, (u32)(val >> 32));
}
HANDLER(A32GetVector) {
    int reg = (int)inst.args[0].value;
    U128 res;
    if (reg >= 64) { // Q0-Q15
        int idx = (reg - 64) * 4;
        res.lo = (u64)cpu.GetVFPReg(idx) | ((u64)cpu.GetVFPReg(idx+1) << 32);
        res.hi = (u64)cpu.GetVFPReg(idx+2) | ((u64)cpu.GetVFPReg(idx+3) << 32);
    } else if (reg >= 32) { // D0-D31
        int idx = (reg - 32) * 2;
        res.lo = (u64)cpu.GetVFPReg(idx) | ((u64)cpu.GetVFPReg(idx+1) << 32);
        res.hi = 0;
    }
    results[inst.result_index] = res;
}
HANDLER(A32SetVector) {
    int reg = (int)inst.args[0].value;
    U128 val = GetArg(inst, results, 1);
    if (reg >= 64) {
        int idx = (reg - 64) * 4;
        cpu.SetVFPReg(idx, (u32)val.lo); cpu.SetVFPReg(idx+1, (u32)(val.lo >> 32));
        cpu.SetVFPReg(idx+2, (u32)val.hi); cpu.SetVFPReg(idx+3, (u32)(val.hi >> 32));
    } else if (reg >= 32) {
        int idx = (reg - 32) * 2;
        cpu.SetVFPReg(idx, (u32)val.lo); cpu.SetVFPReg(idx+1, (u32)(val.lo >> 32));
    }
}
HANDLER(A32SetCpsrNZCV) { cpu.SetCPSR((cpu.GetCPSR() & 0x0FFFFFFF) | (GetArg(inst, results, 0).lo << 28)); }
HANDLER(A32GetCFlag) { results[inst.result_index] = U128((cpu.GetCPSR() >> 29) & 1); }
HANDLER(A32SetCpsrNZCVRaw) { cpu.SetCPSR((cpu.GetCPSR() & 0x0FFFFFFF) | (GetArg(inst, results, 0).lo & 0xF0000000)); }
HANDLER(A32SetCpsrNZCVQ) { cpu.SetCPSR((cpu.GetCPSR() & 0x07FFFFFF) | (GetArg(inst, results, 0).lo & 0xF8000000)); }
HANDLER(A32SetCpsrNZ) { cpu.SetCPSR((cpu.GetCPSR() & 0x3FFFFFFF) | ((GetArg(inst, results, 0).lo & 0xC) << 28)); }
HANDLER(A32SetCpsrNZC) { cpu.SetCPSR((cpu.GetCPSR() & 0x1FFFFFFF) | ((GetArg(inst, results, 0).lo & 0xC) << 28) | ((GetArg(inst, results, 1).lo & 1) << 29)); }
HANDLER(A32OrQFlag) { if (GetArg(inst, results, 0).lo & 1) cpu.SetCPSR(cpu.GetCPSR() | (1 << 27)); }
HANDLER(A32GetGEFlags) { results[inst.result_index] = U128((cpu.GetCPSR() >> 16) & 0xF); }
HANDLER(A32SetGEFlags) { cpu.SetCPSR((cpu.GetCPSR() & ~0xF0000) | (GetArg(inst, results, 0).lo << 16)); }
HANDLER(A32SetGEFlagsCompressed) { cpu.SetCPSR((cpu.GetCPSR() & ~0xF0000) | ((GetArg(inst, results, 0).lo & 0xF) << 16)); }
HANDLER(A32GetFpscr) { results[inst.result_index] = U128(cpu.GetVFPSystemReg(VFP_FPSCR)); }
HANDLER(A32SetFpscr) { cpu.SetVFPSystemReg(VFP_FPSCR, GetArg(inst, results, 0).lo); }
HANDLER(A32GetFpscrNZCV) { results[inst.result_index] = U128(cpu.GetVFPSystemReg(VFP_FPSCR) >> 28); }
HANDLER(A32SetFpscrNZCV) { cpu.SetVFPSystemReg(VFP_FPSCR, (cpu.GetVFPSystemReg(VFP_FPSCR) & 0x0FFFFFFF) | (GetArg(inst, results, 0).lo << 28)); }
HANDLER(A32BXWritePC) {
    u32 val = GetArg(inst, results, 0).lo;
    cpu.SetPC(val & ~1);
    if (val & 1) cpu.SetCPSR(cpu.GetCPSR() | 0x20); else cpu.SetCPSR(cpu.GetCPSR() & ~0x20);
}
HANDLER(A32CallSupervisor) { cpu.GetCallbacks().CallSVC(GetArg(inst, results, 0).lo); }
HANDLER(A32ExceptionRaised) { cpu.GetCallbacks().ExceptionRaised(cpu.GetPC(), (Dynarmic::A32::Exception)GetArg(inst, results, 1).lo); }
HANDLER(A32ReadMemory8) { results[inst.result_index] = U128(cpu.GetCallbacks().MemoryRead8(GetArg(inst, results, 0).lo)); }
HANDLER(A32ReadMemory16) { results[inst.result_index] = U128(cpu.GetCallbacks().MemoryRead16(GetArg(inst, results, 0).lo)); }
HANDLER(A32ReadMemory32) { results[inst.result_index] = U128(cpu.GetCallbacks().MemoryRead32(GetArg(inst, results, 0).lo)); }
HANDLER(A32ReadMemory64) { results[inst.result_index] = U128(cpu.GetCallbacks().MemoryRead64(GetArg(inst, results, 0).lo)); }
HANDLER(A32WriteMemory8) { cpu.GetCallbacks().MemoryWrite8(GetArg(inst, results, 0).lo, (u8)GetArg(inst, results, 1).lo); }
HANDLER(A32WriteMemory16) { cpu.GetCallbacks().MemoryWrite16(GetArg(inst, results, 0).lo, (u16)GetArg(inst, results, 1).lo); }
HANDLER(A32WriteMemory32) { cpu.GetCallbacks().MemoryWrite32(GetArg(inst, results, 0).lo, (u32)GetArg(inst, results, 1).lo); }
HANDLER(A32WriteMemory64) { cpu.GetCallbacks().MemoryWrite64(GetArg(inst, results, 0).lo, GetArg(inst, results, 1).lo); }
HANDLER(A32ExclusiveReadMemory8) { results[inst.result_index] = U128(cpu.GetExclusiveMonitor().ExclusiveRead8(cpu.config.processor_id, GetArg(inst, results, 1).lo)); }
HANDLER(A32ExclusiveReadMemory16) { results[inst.result_index] = U128(cpu.GetExclusiveMonitor().ExclusiveRead16(cpu.config.processor_id, GetArg(inst, results, 1).lo)); }
HANDLER(A32ExclusiveReadMemory32) { results[inst.result_index] = U128(cpu.GetExclusiveMonitor().ExclusiveRead32(cpu.config.processor_id, GetArg(inst, results, 1).lo)); }
HANDLER(A32ExclusiveReadMemory64) { results[inst.result_index] = U128(cpu.GetExclusiveMonitor().ExclusiveRead64(cpu.config.processor_id, GetArg(inst, results, 1).lo)); }
HANDLER(A32ExclusiveWriteMemory8) { results[inst.result_index] = U128(cpu.GetExclusiveMonitor().ExclusiveWrite8(cpu.config.processor_id, GetArg(inst, results, 1).lo, (u8)GetArg(inst, results, 2).lo) ? 0 : 1); }
HANDLER(A32ExclusiveWriteMemory16) { results[inst.result_index] = U128(cpu.GetExclusiveMonitor().ExclusiveWrite16(cpu.config.processor_id, GetArg(inst, results, 1).lo, (u16)GetArg(inst, results, 2).lo) ? 0 : 1); }
HANDLER(A32ExclusiveWriteMemory32) { results[inst.result_index] = U128(cpu.GetExclusiveMonitor().ExclusiveWrite32(cpu.config.processor_id, GetArg(inst, results, 1).lo, (u32)GetArg(inst, results, 2).lo) ? 0 : 1); }
HANDLER(A32ExclusiveWriteMemory64) { results[inst.result_index] = U128(cpu.GetExclusiveMonitor().ExclusiveWrite64(cpu.config.processor_id, GetArg(inst, results, 1).lo, GetArg(inst, results, 2).lo) ? 0 : 1); }
HANDLER(A32ClearExclusive) { cpu.ClearExclusiveState(); }
HANDLER(A32CoprocGetOneWord) {
    u64 info_raw = inst.args[0].value;
    u8 coproc_no = info_raw & 0xFF; u8 opc1 = (info_raw >> 16) & 0xFF; u8 CRn = (info_raw >> 24) & 0xFF; u8 CRm = (info_raw >> 32) & 0xFF; u8 opc2 = (info_raw >> 40) & 0xFF;
    if (coproc_no == 15) results[inst.result_index] = U128(cpu.GetCP15Register((CP15Register)0)); /* Needs mapping */
    else results[inst.result_index] = U128(0);
}
HANDLER(A32CoprocSendOneWord) { /* Needs mapping */ }
HANDLER(A32CoprocGetTwoWords) {
    u64 info_raw = inst.args[0].value;
    u8 coproc_no = (u8)(info_raw & 0xFF); u8 opc = (u8)((info_raw >> 16) & 0xFF); u8 CRm = (u8)((info_raw >> 24) & 0xFF);
    if (coproc_no == 15 && opc == 0 && CRm == 14) results[inst.result_index] = U128(cpu.GetTimer().GetTicks());
    else results[inst.result_index] = U128(0);
}
HANDLER(Add64) {
    u64 a = GetArg(inst, results, 0).lo; u64 b = GetArg(inst, results, 1).lo; u64 carry_in = GetArg(inst, results, 2).lo & 1;
    U128 res128 = U128(a) + U128(b) + U128(carry_in); u64 res = res128.lo;
    results[inst.result_index] = U128(res);
    u32 nzcv = 0; if (res & 0x8000000000000000ULL) nzcv |= 8; if (res == 0) nzcv |= 4; if (res128.hi != 0) nzcv |= 2; if ((~(a ^ b) & (a ^ res)) & 0x8000000000000000ULL) nzcv |= 1;
    cpu.flags_buffer[inst.result_index] = nzcv;
}
HANDLER(Sub64) {
    u64 a = GetArg(inst, results, 0).lo; u64 b = GetArg(inst, results, 1).lo; u64 carry_in = GetArg(inst, results, 2).lo & 1;
    U128 res128 = U128(a) + (~U128(b)) + U128(carry_in); u64 res = res128.lo;
    results[inst.result_index] = U128(res);
    u32 nzcv = 0; if (res & 0x8000000000000000ULL) nzcv |= 8; if (res == 0) nzcv |= 4; if (res128.hi != 0) nzcv |= 2; if (((a ^ b) & (a ^ res)) & 0x8000000000000000ULL) nzcv |= 1;
    cpu.flags_buffer[inst.result_index] = nzcv;
}
HANDLER(Mul32) { results[inst.result_index] = U128((u32)GetArg(inst, results, 0).lo * (u32)GetArg(inst, results, 1).lo); }
HANDLER(Mul64) { results[inst.result_index] = U128(GetArg(inst, results, 0).lo * GetArg(inst, results, 1).lo); }
HANDLER(UnsignedDiv32) { u32 a = (u32)GetArg(inst, results, 0).lo; u32 b = (u32)GetArg(inst, results, 1).lo; results[inst.result_index] = U128(b == 0 ? 0 : a / b); }
HANDLER(SignedDiv32) { s32 a = (s32)GetArg(inst, results, 0).lo; s32 b = (s32)GetArg(inst, results, 1).lo; results[inst.result_index] = U128(b == 0 ? 0 : (a == -2147483648 && b == -1) ? a : a / b); }
HANDLER(And64) { results[inst.result_index] = U128(GetArg(inst, results, 0).lo & GetArg(inst, results, 1).lo); }
HANDLER(Or64) { results[inst.result_index] = U128(GetArg(inst, results, 0).lo | GetArg(inst, results, 1).lo); }
HANDLER(Eor64) { results[inst.result_index] = U128(GetArg(inst, results, 0).lo ^ GetArg(inst, results, 1).lo); }
HANDLER(Not32) { results[inst.result_index] = U128(~(u32)GetArg(inst, results, 0).lo); }
HANDLER(Not64) { results[inst.result_index] = U128(~GetArg(inst, results, 0).lo); }
HANDLER(AndNot32) { results[inst.result_index] = U128((u32)GetArg(inst, results, 0).lo & ~(u32)GetArg(inst, results, 1).lo); }
HANDLER(SignExtendByteToWord) { results[inst.result_index] = U128((u32)(s32)(s8)GetArg(inst, results, 0).lo); }
HANDLER(SignExtendHalfToWord) { results[inst.result_index] = U128((u32)(s32)(s16)GetArg(inst, results, 0).lo); }
HANDLER(ZeroExtendByteToWord) { results[inst.result_index] = U128((u32)(u8)GetArg(inst, results, 0).lo); }
HANDLER(ZeroExtendHalfToWord) { results[inst.result_index] = U128((u32)(u16)GetArg(inst, results, 0).lo); }
HANDLER(ByteReverseWord) { results[inst.result_index] = U128(::Common::swap32((u32)GetArg(inst, results, 0).lo)); }
HANDLER(ByteReverseHalf) { results[inst.result_index] = U128(::Common::swap16((u16)GetArg(inst, results, 0).lo)); }
HANDLER(CountLeadingZeros32) { u32 val = (u32)GetArg(inst, results, 0).lo; results[inst.result_index] = U128(val == 0 ? 32 : __builtin_clz(val)); }
HANDLER(GetNZCVFromOp) { results[inst.result_index] = U128(cpu.flags_buffer[GetArg(inst, results, 0).lo]); }
HANDLER(GetCarryFromOp) { results[inst.result_index] = U128((cpu.flags_buffer[GetArg(inst, results, 0).lo] >> 1) & 1); }
HANDLER(GetOverflowFromOp) { results[inst.result_index] = U128(cpu.flags_buffer[GetArg(inst, results, 0).lo] & 1); }
HANDLER(GetNZFromOp) { results[inst.result_index] = U128(cpu.flags_buffer[GetArg(inst, results, 0).lo] & 0xC); }
HANDLER(FPAdd32) { float a = std::bit_cast<float>((u32)GetArg(inst, results, 0).lo); float b = std::bit_cast<float>((u32)GetArg(inst, results, 1).lo); results[inst.result_index] = U128(std::bit_cast<u32>(a + b)); }
HANDLER(FPSub32) { float a = std::bit_cast<float>((u32)GetArg(inst, results, 0).lo); float b = std::bit_cast<float>((u32)GetArg(inst, results, 1).lo); results[inst.result_index] = U128(std::bit_cast<u32>(a - b)); }
HANDLER(FPMul32) { float a = std::bit_cast<float>((u32)GetArg(inst, results, 0).lo); float b = std::bit_cast<float>((u32)GetArg(inst, results, 1).lo); results[inst.result_index] = U128(std::bit_cast<u32>(a * b)); }
HANDLER(FPDiv32) { float a = std::bit_cast<float>((u32)GetArg(inst, results, 0).lo); float b = std::bit_cast<float>((u32)GetArg(inst, results, 1).lo); results[inst.result_index] = U128(std::bit_cast<u32>(a / b)); }
HANDLER(FPAbs32) { results[inst.result_index] = U128(GetArg(inst, results, 0).lo & 0x7FFFFFFF); }
HANDLER(FPNeg32) { results[inst.result_index] = U128(GetArg(inst, results, 0).lo ^ 0x80000000); }
HANDLER(FPSqrt32) { float a = std::bit_cast<float>((u32)GetArg(inst, results, 0).lo); results[inst.result_index] = U128(std::bit_cast<u32>(std::sqrt(a))); }
HANDLER(FPMax32) { float a = std::bit_cast<float>((u32)GetArg(inst, results, 0).lo); float b = std::bit_cast<float>((u32)GetArg(inst, results, 1).lo); results[inst.result_index] = U128(std::bit_cast<u32>(std::max(a, b))); }
HANDLER(FPMin32) { float a = std::bit_cast<float>((u32)GetArg(inst, results, 0).lo); float b = std::bit_cast<float>((u32)GetArg(inst, results, 1).lo); results[inst.result_index] = U128(std::bit_cast<u32>(std::min(a, b))); }
HANDLER(FPCompare32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0).lo); float b = std::bit_cast<float>((u32)GetArg(inst, results, 1).lo);
    u32 nzcv = 0; if (std::isnan(a) || std::isnan(b)) nzcv = 0x3; else if (a == b) nzcv = 0x6; else if (a < b) nzcv = 0x8; else nzcv = 0x2;
    results[inst.result_index] = U128(nzcv);
}
HANDLER(FPSingleToDouble) { float a = std::bit_cast<float>((u32)GetArg(inst, results, 0).lo); results[inst.result_index] = U128(std::bit_cast<u64>((double)a)); }
HANDLER(FPDoubleToSingle) { double a = std::bit_cast<double>(GetArg(inst, results, 0).lo); results[inst.result_index] = U128(std::bit_cast<u32>((float)a)); }
HANDLER(Pack2x32To1x64) { u64 lo = (u32)GetArg(inst, results, 0).lo; u64 hi = (u32)GetArg(inst, results, 1).lo; results[inst.result_index] = U128(lo | (hi << 32)); }
HANDLER(LeastSignificantWord) { results[inst.result_index] = U128((u32)GetArg(inst, results, 0).lo); }
HANDLER(MostSignificantWord) { results[inst.result_index] = U128((u32)(GetArg(inst, results, 0).lo >> 32)); }
HANDLER(ConditionalSelect32) { results[inst.result_index] = CheckCondition(cpu.GetCPSR(), (IR::Cond)inst.args[0].value) ? GetArg(inst, results, 1) : GetArg(inst, results, 2); }
HANDLER(Barrier) {}

static IRHandler GetHandler(IR::Opcode op) {
    switch (op) {
    case IR::Opcode::A32GetRegister: return Handler_A32GetRegister;
    case IR::Opcode::A32SetRegister: return Handler_A32SetRegister;
    case IR::Opcode::A32GetCpsr: return Handler_A32GetCpsr;
    case IR::Opcode::A32SetCpsr: return Handler_A32SetCpsr;
    case IR::Opcode::Add32: return Handler_Add32;
    case IR::Opcode::Sub32: return Handler_Sub32;
    case IR::Opcode::And32: return Handler_And32;
    case IR::Opcode::Or32: return Handler_Or32;
    case IR::Opcode::Eor32: return Handler_Eor32;
    case IR::Opcode::LogicalShiftLeft32: return Handler_LSL32;
    case IR::Opcode::LogicalShiftRight32: return Handler_LSR32;
    case IR::Opcode::ArithmeticShiftRight32: return Handler_ASR32;
    case IR::Opcode::RotateRight32: return Handler_ROR32;
    case IR::Opcode::A32GetExtendedRegister32: return Handler_A32GetExtendedRegister32;
    case IR::Opcode::A32SetExtendedRegister32: return Handler_A32SetExtendedRegister32;
    case IR::Opcode::A32GetExtendedRegister64: return Handler_A32GetExtendedRegister64;
    case IR::Opcode::A32SetExtendedRegister64: return Handler_A32SetExtendedRegister64;
    case IR::Opcode::A32GetVector: return Handler_A32GetVector;
    case IR::Opcode::A32SetVector: return Handler_A32SetVector;
    case IR::Opcode::A32SetCpsrNZCV: return Handler_A32SetCpsrNZCV;
    case IR::Opcode::A32GetCFlag: return Handler_A32GetCFlag;
    case IR::Opcode::A32SetCpsrNZCVRaw: return Handler_A32SetCpsrNZCVRaw;
    case IR::Opcode::A32SetCpsrNZCVQ: return Handler_A32SetCpsrNZCVQ;
    case IR::Opcode::A32SetCpsrNZ: return Handler_A32SetCpsrNZ;
    case IR::Opcode::A32SetCpsrNZC: return Handler_A32SetCpsrNZC;
    case IR::Opcode::A32OrQFlag: return Handler_A32OrQFlag;
    case IR::Opcode::A32GetGEFlags: return Handler_A32GetGEFlags;
    case IR::Opcode::A32SetGEFlags: return Handler_A32SetGEFlags;
    case IR::Opcode::A32SetGEFlagsCompressed: return Handler_A32SetGEFlagsCompressed;
    case IR::Opcode::A32GetFpscr: return Handler_A32GetFpscr;
    case IR::Opcode::A32SetFpscr: return Handler_A32SetFpscr;
    case IR::Opcode::A32GetFpscrNZCV: return Handler_A32GetFpscrNZCV;
    case IR::Opcode::A32SetFpscrNZCV: return Handler_A32SetFpscrNZCV;
    case IR::Opcode::A32BXWritePC: return Handler_A32BXWritePC;
    case IR::Opcode::A32CallSupervisor: return Handler_A32CallSupervisor;
    case IR::Opcode::A32ExceptionRaised: return Handler_A32ExceptionRaised;
    case IR::Opcode::A32ReadMemory8: return Handler_A32ReadMemory8;
    case IR::Opcode::A32ReadMemory16: return Handler_A32ReadMemory16;
    case IR::Opcode::A32ReadMemory32: return Handler_A32ReadMemory32;
    case IR::Opcode::A32ReadMemory64: return Handler_A32ReadMemory64;
    case IR::Opcode::A32WriteMemory8: return Handler_A32WriteMemory8;
    case IR::Opcode::A32WriteMemory16: return Handler_A32WriteMemory16;
    case IR::Opcode::A32WriteMemory32: return Handler_A32WriteMemory32;
    case IR::Opcode::A32WriteMemory64: return Handler_A32WriteMemory64;
    case IR::Opcode::A32ExclusiveReadMemory8: return Handler_A32ExclusiveReadMemory8;
    case IR::Opcode::A32ExclusiveReadMemory16: return Handler_A32ExclusiveReadMemory16;
    case IR::Opcode::A32ExclusiveReadMemory32: return Handler_A32ExclusiveReadMemory32;
    case IR::Opcode::A32ExclusiveReadMemory64: return Handler_A32ExclusiveReadMemory64;
    case IR::Opcode::A32ExclusiveWriteMemory8: return Handler_A32ExclusiveWriteMemory8;
    case IR::Opcode::A32ExclusiveWriteMemory16: return Handler_A32ExclusiveWriteMemory16;
    case IR::Opcode::A32ExclusiveWriteMemory32: return Handler_A32ExclusiveWriteMemory32;
    case IR::Opcode::A32ExclusiveWriteMemory64: return Handler_A32ExclusiveWriteMemory64;
    case IR::Opcode::A32ClearExclusive: return Handler_A32ClearExclusive;
    case IR::Opcode::A32CoprocGetOneWord: return Handler_A32CoprocGetOneWord;
    case IR::Opcode::A32CoprocSendOneWord: return Handler_A32CoprocSendOneWord;
    case IR::Opcode::A32CoprocGetTwoWords: return Handler_A32CoprocGetTwoWords;
    case IR::Opcode::Add64: return Handler_Add64;
    case IR::Opcode::Sub64: return Handler_Sub64;
    case IR::Opcode::Mul32: return Handler_Mul32;
    case IR::Opcode::Mul64: return Handler_Mul64;
    case IR::Opcode::UnsignedDiv32: return Handler_UnsignedDiv32;
    case IR::Opcode::SignedDiv32: return Handler_SignedDiv32;
    case IR::Opcode::And64: return Handler_And64;
    case IR::Opcode::Or64: return Handler_Or64;
    case IR::Opcode::Eor64: return Handler_Eor64;
    case IR::Opcode::Not32: return Handler_Not32;
    case IR::Opcode::Not64: return Handler_Not64;
    case IR::Opcode::AndNot32: return Handler_AndNot32;
    case IR::Opcode::SignExtendByteToWord: return Handler_SignExtendByteToWord;
    case IR::Opcode::SignExtendHalfToWord: return Handler_SignExtendHalfToWord;
    case IR::Opcode::ZeroExtendByteToWord: return Handler_ZeroExtendByteToWord;
    case IR::Opcode::ZeroExtendHalfToWord: return Handler_ZeroExtendHalfToWord;
    case IR::Opcode::ByteReverseWord: return Handler_ByteReverseWord;
    case IR::Opcode::ByteReverseHalf: return Handler_ByteReverseHalf;
    case IR::Opcode::CountLeadingZeros32: return Handler_CountLeadingZeros32;
    case IR::Opcode::GetNZCVFromOp: return Handler_GetNZCVFromOp;
    case IR::Opcode::GetCarryFromOp: return Handler_GetCarryFromOp;
    case IR::Opcode::GetOverflowFromOp: return Handler_GetOverflowFromOp;
    case IR::Opcode::GetNZFromOp: return Handler_GetNZFromOp;
    case IR::Opcode::FPAdd32: return Handler_FPAdd32;
    case IR::Opcode::FPSub32: return Handler_FPSub32;
    case IR::Opcode::FPMul32: return Handler_FPMul32;
    case IR::Opcode::FPDiv32: return Handler_FPDiv32;
    case IR::Opcode::FPAbs32: return Handler_FPAbs32;
    case IR::Opcode::FPNeg32: return Handler_FPNeg32;
    case IR::Opcode::FPSqrt32: return Handler_FPSqrt32;
    case IR::Opcode::FPMax32: return Handler_FPMax32;
    case IR::Opcode::FPMin32: return Handler_FPMin32;
    case IR::Opcode::FPCompare32: return Handler_FPCompare32;
    case IR::Opcode::FPSingleToDouble: return Handler_FPSingleToDouble;
    case IR::Opcode::FPDoubleToSingle: return Handler_FPDoubleToSingle;
    case IR::Opcode::Pack2x32To1x64: return Handler_Pack2x32To1x64;
    case IR::Opcode::LeastSignificantWord: return Handler_LeastSignificantWord;
    case IR::Opcode::MostSignificantWord: return Handler_MostSignificantWord;
    case IR::Opcode::ConditionalSelect32: return Handler_ConditionalSelect32;
    case IR::Opcode::A32DataSynchronizationBarrier:
    case IR::Opcode::A32DataMemoryBarrier:
    case IR::Opcode::A32InstructionSynchronizationBarrier: return Handler_Barrier;
    default: return nullptr;
    }
}


void ARM_Hybrid::Run() {
    while (timer->GetDowncount() > 0) {
        auto it = hle_functions.find(state->Reg[15]);
        if (it != hle_functions.end()) {
            it->second(*this);
            continue;
        }

        const auto& block = GetOrTranslateBlock(state->Reg[15]);
        if (block.use_ir) {
            ExecuteBlock(block);
        } else {
            state->NumInstrsToExecute = std::max<s64>(timer->GetDowncount(), 1);
            u32 ticks = InterpreterMainLoop(state.get());
            timer->AddTicks(ticks);
        }

        if (timer->GetDowncount() <= 0) {
            break;
        }
    }
}

void ARM_Hybrid::Step() {
    auto it = hle_functions.find(state->Reg[15]);
    if (it != hle_functions.end()) {
        it->second(*this);
        return;
    }

    const auto& block = GetOrTranslateBlock(state->Reg[15]);
    if (block.use_ir) {
        ExecuteBlock(block);
    } else {
        state->NumInstrsToExecute = 1;
        u32 ticks = InterpreterMainLoop(state.get());
        timer->AddTicks(ticks);
    }
}

void ARM_Hybrid::RegisterHLEFunction(u32 address, HLEFunction func) {
    hle_functions[address] = std::move(func);
    InvalidateCacheRange(address, 4);
}

void ARM_Hybrid::SetPC(u32 pc) { state->Reg[15] = pc; }
u32 ARM_Hybrid::GetPC() const { return state->Reg[15]; }
u32 ARM_Hybrid::GetReg(int index) const { return state->Reg[index]; }
void ARM_Hybrid::SetReg(int index, u32 value) { state->Reg[index] = value; }
u32 ARM_Hybrid::GetVFPReg(int index) const { return state->ExtReg[index]; }
void ARM_Hybrid::SetVFPReg(int index, u32 value) { state->ExtReg[index] = value; }
u32 ARM_Hybrid::GetVFPSystemReg(VFPSystemRegister reg) const { return state->VFP[reg]; }
void ARM_Hybrid::SetVFPSystemReg(VFPSystemRegister reg, u32 value) { state->VFP[reg] = value; }
u32 ARM_Hybrid::GetCPSR() const { return state->Cpsr; }
void ARM_Hybrid::SetCPSR(u32 cpsr) {
    state->Cpsr = cpsr;
    state->Mode = cpsr & 0x1F;
    state->TFlag = (cpsr >> 5) & 1;
}
u32 ARM_Hybrid::GetCP15Register(CP15Register reg) const { return state->CP15[reg]; }
void ARM_Hybrid::SetCP15Register(CP15Register reg, u32 value) { state->CP15[reg] = value; }

void ARM_Hybrid::SaveContext(ThreadContext& ctx) {
    for (int i = 0; i < 16; i++) ctx.cpu_registers[i] = state->Reg[i];
    ctx.cpsr = state->Cpsr;
    for (int i = 0; i < 64; i++) ctx.fpu_registers[i] = state->ExtReg[i];
    ctx.fpscr = state->VFP[VFP_FPSCR];
    ctx.fpexc = state->VFP[VFP_FPEXC];
}

void ARM_Hybrid::LoadContext(const ThreadContext& ctx) {
    for (int i = 0; i < 16; i++) state->Reg[i] = ctx.cpu_registers[i];
    SetCPSR(ctx.cpsr);
    for (int i = 0; i < 64; i++) state->ExtReg[i] = ctx.fpu_registers[i];
    state->VFP[VFP_FPSCR] = ctx.fpscr;
    state->VFP[VFP_FPEXC] = ctx.fpexc;
}

void ARM_Hybrid::PrepareReschedule() {}
void ARM_Hybrid::ClearInstructionCache() {
    block_cache.clear();
    fast_block_cache.fill({0xFFFFFFFF, nullptr});
}
void ARM_Hybrid::InvalidateCacheRange(u32 start_address, std::size_t length) {
    for (u32 addr = start_address; addr < start_address + length; addr += 4) {
        block_cache.erase(addr);
        fast_block_cache[(addr >> 2) & (FAST_BLOCK_CACHE_SIZE - 1)] = {0xFFFFFFFF, nullptr};
    }
}
void ARM_Hybrid::ClearExclusiveState() { exclusive_monitor.ClearExclusive(config.processor_id); }
void ARM_Hybrid::SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) { current_page_table = page_table; }
std::shared_ptr<Memory::PageTable> ARM_Hybrid::GetPageTable() const { return current_page_table; }

void ARM_Hybrid::SetIRRegion(u32 start, u32 size) {
    ir_regions.push_back({start, size});
    InvalidateCacheRange(start, size);
}

void ARM_Hybrid::SetHLERegion(u32 start, u32 size) {
    hle_regions.push_back({start, size});
    InvalidateCacheRange(start, size);
}

u32 ARM_Hybrid::ReadCP15(u32 crn, u32 op1, u32 crm, u32 op2) const {
    return state->ReadCP15Register(crn, op1, crm, op2);
}

void ARM_Hybrid::WriteCP15(u32 value, u32 crn, u32 op1, u32 crm, u32 op2) {
    state->WriteCP15Register(value, crn, op1, crm, op2);
}

const ARM_Hybrid::TranslatedBlock& ARM_Hybrid::GetOrTranslateBlock(u32 pc) {
    const u32 index = (pc >> 2) & (FAST_BLOCK_CACHE_SIZE - 1);
    if (fast_block_cache[index].pc == pc) return *fast_block_cache[index].block;

    auto it = block_cache.find(pc);
    if (it != block_cache.end()) {
        fast_block_cache[index].pc = pc;
        fast_block_cache[index].block = &it->second;
        return it->second;
    }

    // Hybrid logic: determine if we should use IR or HLE for this block
    bool use_ir = true;

    // Check custom regions
    bool in_custom_region = false;
    for (const auto& region : hle_regions) { if (pc >= region.start && pc < region.start + region.size) { in_custom_region = true; use_ir = false; break; } }
    if (!in_custom_region) {
        for (const auto& region : ir_regions) { if (pc >= region.start && pc < region.start + region.size) { in_custom_region = true; use_ir = true; break; } }
    }

    if (!in_custom_region) {
        if (pc >= 0xFFFF0000) {
            use_ir = false;
        }
    }

    TranslatedBlock tb;
    tb.use_ir = use_ir;

    if (use_ir) {
        Dynarmic::A32::LocationDescriptor ld{pc, Dynarmic::A32::PSR(state->Cpsr), Dynarmic::A32::FPSCR(state->VFP[VFP_FPSCR])};

        cb->ResetAccumulator();
        IR::Block block = A32::Translate(ld, cb.get(), {config.arch_version, config.define_unpredictable_behaviour, config.hook_hint_instructions});

        tb.guest_end_pc = Dynarmic::A32::LocationDescriptor(block.EndLocation()).PC();
        tb.total_ticks = cb->GetAccumulatedTicks();
        tb.instruction_count = static_cast<u32>(block.Instructions().size()); // This is IR instructions, but good enough for generic count

        std::map<IR::Inst*, u16> inst_to_index;
        u16 next_index = 0;

        for (auto& inst : block.Instructions()) {
            Instruction decoded;
            decoded.op = inst.GetOpcode();
            decoded.handler = GetHandler(decoded.op);
            decoded.result_index = next_index;
            decoded.arg_count = static_cast<u8>(std::min<size_t>(inst.NumArgs(), decoded.args.size()));
            inst_to_index[&inst] = next_index++;

            for (size_t i = 0; i < decoded.arg_count; ++i) {
                IR::Value arg = inst.GetArg(i);
                Operand op;
                if (arg.IsImmediate()) {
                    op.kind = Operand::Immediate;
                    op.value = arg.GetImmediateAsU64();
                } else {
                    IR::Type type = arg.GetType();
                    if (type == IR::Type::A32Reg) {
                        op.kind = Operand::Register;
                        op.value = (u64)arg.GetA32RegRef();
                    } else if (type == IR::Type::A32ExtReg) {
                        op.kind = Operand::ExtReg;
                        op.value = (u64)arg.GetA32ExtRegRef();
                    } else if (type == IR::Type::Cond) {
                        op.kind = Operand::Cond;
                        op.value = (u64)arg.GetCond();
                    } else if (type == IR::Type::AccType) {
                        op.kind = Operand::AccType;
                        op.value = (u64)arg.GetAccType();
                    } else if (type == IR::Type::CoprocInfo) {
                        op.kind = Operand::CoprocInfo;
                        auto info = arg.GetCoprocInfo();
                        u64 info_raw = 0;
                        for (int j = 0; j < 8; ++j) info_raw |= (u64)info[j] << (j * 8);
                        op.value = info_raw;
                    } else {
                        op.kind = Operand::Result;
                        op.value = inst_to_index[arg.GetInst()];
                    }
                }
                decoded.args[i] = op;
            }
            tb.instructions.push_back(decoded);
        }
        tb.guest_end_pc = Dynarmic::A32::LocationDescriptor(block.EndLocation()).PC();
    }

    block_cache[pc] = std::move(tb);
    fast_block_cache[index].pc = pc;
    fast_block_cache[index].block = &block_cache[pc];
    return block_cache[pc];
}

void ARM_Hybrid::ExecuteBlock(const TranslatedBlock& block) {
    timer->AddTicks(block.total_ticks);
    size_t num_insts = block.instructions.size();
    if (results_buffer.size() < num_insts) {
        results_buffer.resize(std::max<size_t>(num_insts, 1024));
        flags_buffer.resize(std::max<size_t>(num_insts, 1024));
    }
    U128* results_ptr = results_buffer.data();

    u32 start_pc = state->Reg[15];

    for (const auto& inst : block.instructions) {
        if (inst.handler) {
            inst.handler(*this, inst, results_ptr);
        }
    }
    if (state->Reg[15] == start_pc) {
        state->Reg[15] = block.guest_end_pc;
    }
}

} // namespace Core
