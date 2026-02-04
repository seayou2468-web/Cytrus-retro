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
typedef void (*OpHandler)(ARM_StaticIR& self, const ARM_StaticIR::Instruction& inst, unsigned __int128* results, u32& next_pc, bool& branched);

static inline unsigned __int128 GetArg(const ARM_StaticIR::Instruction& inst, const unsigned __int128* results, size_t i) {
    if (inst.args[i].kind == ARM_StaticIR::Operand::Immediate) return inst.args[i].value;
    return results[inst.args[i].value];
}

#define OP_HANDLER(name) static void Handle##name(ARM_StaticIR& self, const ARM_StaticIR::Instruction& inst, unsigned __int128* results, u32& next_pc, bool& branched)

OP_HANDLER(A32GetRegister) { results[inst.result_index] = self.GetReg((int)inst.args[0].value); }
OP_HANDLER(A32SetRegister) { self.SetReg((int)inst.args[0].value, (u32)GetArg(inst, results, 1)); }
OP_HANDLER(A32GetExtendedRegister32) { results[inst.result_index] = self.GetVFPReg((int)inst.args[0].value); }
OP_HANDLER(A32SetExtendedRegister32) { self.SetVFPReg((int)inst.args[0].value, (u32)GetArg(inst, results, 1)); }
OP_HANDLER(A32GetCpsr) { results[inst.result_index] = self.GetCPSR(); }
OP_HANDLER(A32SetCpsr) { self.SetCPSR((u32)GetArg(inst, results, 0)); }
OP_HANDLER(A32SetCpsrNZCV) { self.SetCPSR((self.GetCPSR() & 0x0FFFFFFF) | ((u32)GetArg(inst, results, 0) << 28)); }
OP_HANDLER(A32SetCpsrNZCVRaw) { self.SetCPSR((self.GetCPSR() & 0x0FFFFFFF) | ((u32)GetArg(inst, results, 0) & 0xF0000000)); }
OP_HANDLER(A32SetCpsrNZCVQ) { self.SetCPSR((self.GetCPSR() & 0x07FFFFFF) | ((u32)GetArg(inst, results, 0) & 0xF8000000)); }
OP_HANDLER(A32SetCpsrNZ) { self.SetCPSR((self.GetCPSR() & 0x3FFFFFFF) | (((u32)GetArg(inst, results, 0) & 0xC) << 28)); }
OP_HANDLER(A32SetCpsrNZC) { self.SetCPSR((self.GetCPSR() & 0x1FFFFFFF) | (((u32)GetArg(inst, results, 0) & 0xC) << 28) | (((u32)GetArg(inst, results, 1) & 1) << 29)); }
OP_HANDLER(SetCheckBit) {}
OP_HANDLER(OrQFlag) { if (GetArg(inst, results, 0) & 1) self.SetCPSR(self.GetCPSR() | (1 << 27)); }
OP_HANDLER(A32ExceptionRaised) { self.GetCallbacks().ExceptionRaised(self.GetPC(), (Dynarmic::A32::Exception)GetArg(inst, results, 0)); }
OP_HANDLER(A32GetCFlag) { results[inst.result_index] = (self.GetCPSR() >> 29) & 1; }
OP_HANDLER(Add32) {
    u32 a = (u32)GetArg(inst, results, 0);
    u32 b = (u32)GetArg(inst, results, 1);
    u32 res = a + b;
    results[inst.result_index] = res;
    bool n = (res >> 31) & 1;
    bool z = (res == 0);
    bool c = res < a;
    bool v = (~(a ^ b) & (a ^ res)) >> 31;
    u32 flags = (n << 3) | (z << 2) | (c << 1) | v;
    self.flags_buffer[inst.result_index] = flags;
    if (inst.arg_count > 2 && inst.args[2].kind == ARM_StaticIR::Operand::Immediate && inst.args[2].value) {
        self.SetCPSR((self.GetCPSR() & 0x0FFFFFFF) | (flags << 28));
    }
}
OP_HANDLER(Add64) { results[inst.result_index] = GetArg(inst, results, 0) + GetArg(inst, results, 1); }
OP_HANDLER(Sub32) {
    u32 a = (u32)GetArg(inst, results, 0);
    u32 b = (u32)GetArg(inst, results, 1);
    u32 res = a - b;
    results[inst.result_index] = res;
    bool n = (res >> 31) & 1;
    bool z = (res == 0);
    bool c = a >= b;
    bool v = ((a ^ b) & (a ^ res)) >> 31;
    u32 flags = (n << 3) | (z << 2) | (c << 1) | v;
    self.flags_buffer[inst.result_index] = flags;
    if (inst.arg_count > 2 && inst.args[2].kind == ARM_StaticIR::Operand::Immediate && inst.args[2].value) {
        self.SetCPSR((self.GetCPSR() & 0x0FFFFFFF) | (flags << 28));
    }
}
OP_HANDLER(Sub64) { results[inst.result_index] = GetArg(inst, results, 0) - GetArg(inst, results, 1); }
OP_HANDLER(Mul32) { results[inst.result_index] = (u32)GetArg(inst, results, 0) * (u32)GetArg(inst, results, 1); }
OP_HANDLER(Mul64) { results[inst.result_index] = GetArg(inst, results, 0) * GetArg(inst, results, 1); }
OP_HANDLER(SignedMultiplyHigh64) {
    s64 a = (s64)GetArg(inst, results, 0);
    s64 b = (s64)GetArg(inst, results, 1);
    __int128 res = (__int128)a * b;
    results[inst.result_index] = (u64)(res >> 64);
}
OP_HANDLER(UnsignedMultiplyHigh64) {
    unsigned __int128 a = (u64)GetArg(inst, results, 0);
    unsigned __int128 b = (u64)GetArg(inst, results, 1);
    unsigned __int128 res = a * b;
    results[inst.result_index] = (u64)(res >> 64);
}
OP_HANDLER(SignedDiv32) {
    s32 a = (s32)GetArg(inst, results, 0);
    s32 b = (s32)GetArg(inst, results, 1);
    results[inst.result_index] = (u32)(b == 0 ? 0 : a / b);
}
OP_HANDLER(UnsignedDiv32) {
    u32 a = (u32)GetArg(inst, results, 0);
    u32 b = (u32)GetArg(inst, results, 1);
    results[inst.result_index] = (b == 0 ? 0 : a / b);
}
OP_HANDLER(And32) { results[inst.result_index] = (u32)GetArg(inst, results, 0) & (u32)GetArg(inst, results, 1); }
OP_HANDLER(And64) { results[inst.result_index] = GetArg(inst, results, 0) & GetArg(inst, results, 1); }
OP_HANDLER(AndNot32) { results[inst.result_index] = (u32)GetArg(inst, results, 0) & ~(u32)GetArg(inst, results, 1); }
OP_HANDLER(AndNot64) { results[inst.result_index] = GetArg(inst, results, 0) & ~GetArg(inst, results, 1); }
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
OP_HANDLER(LogicalShiftLeftMasked32) { results[inst.result_index] = (u32)GetArg(inst, results, 0) << (GetArg(inst, results, 1) & 31); }
OP_HANDLER(LogicalShiftRightMasked32) { results[inst.result_index] = (u32)GetArg(inst, results, 0) >> (GetArg(inst, results, 1) & 31); }
OP_HANDLER(ArithmeticShiftRightMasked32) { results[inst.result_index] = (u32)((s32)GetArg(inst, results, 0) >> (GetArg(inst, results, 1) & 31)); }
OP_HANDLER(RotateRightMasked32) {
    u32 val = (u32)GetArg(inst, results, 0);
    u32 amount = (u32)GetArg(inst, results, 1) & 31;
    results[inst.result_index] = amount == 0 ? val : (val >> amount) | (val << (32 - amount));
}
OP_HANDLER(A32ReadMemory8) { results[inst.result_index] = self.GetCallbacks().MemoryRead8((u32)GetArg(inst, results, 0)); }
OP_HANDLER(A32ReadMemory16) { results[inst.result_index] = self.GetCallbacks().MemoryRead16((u32)GetArg(inst, results, 0)); }
OP_HANDLER(A32ReadMemory32) { results[inst.result_index] = self.GetCallbacks().MemoryRead32((u32)GetArg(inst, results, 0)); }
OP_HANDLER(A32ReadMemory64) { results[inst.result_index] = self.GetCallbacks().MemoryRead64((u32)GetArg(inst, results, 0)); }
OP_HANDLER(A32WriteMemory8) { self.GetCallbacks().MemoryWrite8((u32)GetArg(inst, results, 0), (u8)GetArg(inst, results, 1)); }
OP_HANDLER(A32WriteMemory16) { self.GetCallbacks().MemoryWrite16((u32)GetArg(inst, results, 0), (u16)GetArg(inst, results, 1)); }
OP_HANDLER(A32WriteMemory32) { self.GetCallbacks().MemoryWrite32((u32)GetArg(inst, results, 0), (u32)GetArg(inst, results, 1)); }
OP_HANDLER(A32WriteMemory64) { self.GetCallbacks().MemoryWrite64((u32)GetArg(inst, results, 0), GetArg(inst, results, 1)); }

OP_HANDLER(A32ExclusiveReadMemory8) { results[inst.result_index] = self.GetCallbacks().MemoryRead8((u32)GetArg(inst, results, 0)); }
OP_HANDLER(A32ExclusiveReadMemory16) { results[inst.result_index] = self.GetCallbacks().MemoryRead16((u32)GetArg(inst, results, 0)); }
OP_HANDLER(A32ExclusiveReadMemory32) { results[inst.result_index] = self.GetCallbacks().MemoryRead32((u32)GetArg(inst, results, 0)); }
OP_HANDLER(A32ExclusiveReadMemory64) { results[inst.result_index] = self.GetCallbacks().MemoryRead64((u32)GetArg(inst, results, 0)); }

OP_HANDLER(A32ExclusiveWriteMemory8) { results[inst.result_index] = self.GetCallbacks().MemoryWriteExclusive8((u32)GetArg(inst, results, 0), (u8)GetArg(inst, results, 1), (u8)GetArg(inst, results, 2)) ? 0 : 1; }
OP_HANDLER(A32ExclusiveWriteMemory16) { results[inst.result_index] = self.GetCallbacks().MemoryWriteExclusive16((u32)GetArg(inst, results, 0), (u16)GetArg(inst, results, 1), (u16)GetArg(inst, results, 2)) ? 0 : 1; }
OP_HANDLER(A32ExclusiveWriteMemory32) { results[inst.result_index] = self.GetCallbacks().MemoryWriteExclusive32((u32)GetArg(inst, results, 0), (u32)GetArg(inst, results, 1), (u32)GetArg(inst, results, 2)) ? 0 : 1; }
OP_HANDLER(A32ExclusiveWriteMemory64) { results[inst.result_index] = self.GetCallbacks().MemoryWriteExclusive64((u32)GetArg(inst, results, 0), GetArg(inst, results, 1), GetArg(inst, results, 2)) ? 0 : 1; }
OP_HANDLER(MaxSigned32) { results[inst.result_index] = (u32)std::max((s32)GetArg(inst, results, 0), (s32)GetArg(inst, results, 1)); }
OP_HANDLER(MaxUnsigned32) { results[inst.result_index] = (u32)std::max((u32)GetArg(inst, results, 0), (u32)GetArg(inst, results, 1)); }
OP_HANDLER(MinSigned32) { results[inst.result_index] = (u32)std::min((s32)GetArg(inst, results, 0), (s32)GetArg(inst, results, 1)); }
OP_HANDLER(MinUnsigned32) { results[inst.result_index] = (u32)std::min((u32)GetArg(inst, results, 0), (u32)GetArg(inst, results, 1)); }
OP_HANDLER(IsZero32) { results[inst.result_index] = (u32)GetArg(inst, results, 0) == 0; }
OP_HANDLER(TestBit) { results[inst.result_index] = (GetArg(inst, results, 0) >> (u8)GetArg(inst, results, 1)) & 1; }
OP_HANDLER(A32BXWritePC) {
    u32 val = (u32)GetArg(inst, results, 0);
    next_pc = val & ~1;
    if (val & 1) self.SetCPSR(self.GetCPSR() | 0x20); else self.SetCPSR(self.GetCPSR() & ~0x20);
    branched = true;
}
OP_HANDLER(A32UpdateUpperLocationDescriptor) {}
OP_HANDLER(A32ClearExclusive) { self.ClearExclusiveState(); }
OP_HANDLER(A32CallSupervisor) { self.GetCallbacks().CallSVC((u32)inst.args[0].value); }
OP_HANDLER(A32GetGEFlags) { results[inst.result_index] = (self.GetCPSR() >> 16) & 0xF; }
OP_HANDLER(A32SetGEFlags) { self.SetCPSR((self.GetCPSR() & ~0xF0000) | ((u32)GetArg(inst, results, 0) << 16)); }
OP_HANDLER(A32SetGEFlagsCompressed) { self.SetCPSR((self.GetCPSR() & ~0xF0000) | (((u32)GetArg(inst, results, 0) & 0xF) << 16)); }
OP_HANDLER(Barrier) {}
OP_HANDLER(A32GetFpscr) { results[inst.result_index] = self.GetVFPSystemReg(VFP_FPSCR); }
OP_HANDLER(A32SetFpscr) { self.SetVFPSystemReg(VFP_FPSCR, (u32)GetArg(inst, results, 0)); }
OP_HANDLER(A32GetFpscrNZCV) { results[inst.result_index] = self.GetVFPSystemReg(VFP_FPSCR) >> 28; }
OP_HANDLER(A32SetFpscrNZCV) {
    u32 fpscr = self.GetVFPSystemReg(VFP_FPSCR);
    u32 nzcv = (u32)GetArg(inst, results, 0);
    self.SetVFPSystemReg(VFP_FPSCR, (fpscr & 0x0FFFFFFF) | (nzcv << 28));
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

OP_HANDLER(ConditionalSelect) {
    bool cond_met = CheckCondition(self.GetCPSR(), (IR::Cond)inst.args[0].value);
    results[inst.result_index] = cond_met ? GetArg(inst, results, 1) : GetArg(inst, results, 2);
}
OP_HANDLER(SignExtendByteToWord) { results[inst.result_index] = (u32)(s32)(s8)GetArg(inst, results, 0); }
OP_HANDLER(SignExtendHalfToWord) { results[inst.result_index] = (u32)(s32)(s16)GetArg(inst, results, 0); }
OP_HANDLER(ZeroExtendByteToWord) { results[inst.result_index] = (u32)(u8)GetArg(inst, results, 0); }
OP_HANDLER(ZeroExtendHalfToWord) { results[inst.result_index] = (u32)(u16)GetArg(inst, results, 0); }
OP_HANDLER(SignExtendByteToLong) { results[inst.result_index] = (u64)(s64)(s8)GetArg(inst, results, 0); }
OP_HANDLER(SignExtendHalfToLong) { results[inst.result_index] = (u64)(s64)(s16)GetArg(inst, results, 0); }
OP_HANDLER(SignExtendWordToLong) { results[inst.result_index] = (u64)(s64)(s32)GetArg(inst, results, 0); }
OP_HANDLER(ZeroExtendByteToLong) { results[inst.result_index] = (u8)GetArg(inst, results, 0); }
OP_HANDLER(ZeroExtendHalfToLong) { results[inst.result_index] = (u16)GetArg(inst, results, 0); }
OP_HANDLER(ZeroExtendWordToLong) { results[inst.result_index] = (u32)GetArg(inst, results, 0); }
OP_HANDLER(UnsignedSaturation) {
    u32 val = (u32)GetArg(inst, results, 0);
    u32 bits = (u32)GetArg(inst, results, 1);
    u32 max = (bits >= 32) ? 0xFFFFFFFF : ((1U << bits) - 1);
    u32 res = std::min(val, max);
    results[inst.result_index] = res;
    if (res != val) self.SetCPSR(self.GetCPSR() | (1 << 27));
}
OP_HANDLER(SignedSaturation) {
    s32 val = (s32)GetArg(inst, results, 0);
    u32 bits = (u32)GetArg(inst, results, 1);
    s32 max = (1LL << (bits - 1)) - 1;
    s32 min = -(1LL << (bits - 1));
    s32 res = std::clamp(val, min, max);
    results[inst.result_index] = (u32)res;
    if (res != val) self.SetCPSR(self.GetCPSR() | (1 << 27));
}
OP_HANDLER(ByteReverseWord) {
    u32 val = (u32)GetArg(inst, results, 0);
    results[inst.result_index] = ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val & 0xFF0000) >> 8) | ((val >> 24) & 0xFF);
}
OP_HANDLER(ByteReverseHalf) {
    u16 val = (u16)GetArg(inst, results, 0);
    results[inst.result_index] = (u16)((val >> 8) | (val << 8));
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
OP_HANDLER(CountLeadingZeros64) {
    u64 val = GetArg(inst, results, 0);
    results[inst.result_index] = val == 0 ? 64 : __builtin_clzll(val);
}
OP_HANDLER(Pack2x32To1x64) { results[inst.result_index] = (GetArg(inst, results, 0) & 0xFFFFFFFF) | (GetArg(inst, results, 1) << 32); }
OP_HANDLER(LeastSignificantWord) { results[inst.result_index] = GetArg(inst, results, 0) & 0xFFFFFFFF; }
OP_HANDLER(LeastSignificantHalf) { results[inst.result_index] = (u16)GetArg(inst, results, 0); }
OP_HANDLER(LeastSignificantByte) { results[inst.result_index] = (u8)GetArg(inst, results, 0); }
OP_HANDLER(MostSignificantWord) { results[inst.result_index] = GetArg(inst, results, 0) >> 32; }
OP_HANDLER(A32GetExtendedRegister64) {
    results[inst.result_index] = ((u64)self.GetVFPReg((int)inst.args[0].value + 1) << 32) | self.GetVFPReg((int)inst.args[0].value);
}
OP_HANDLER(A32SetExtendedRegister64) {
    u64 val = (u64)GetArg(inst, results, 1);
    self.SetVFPReg((int)inst.args[0].value, (u32)val);
    self.SetVFPReg((int)inst.args[0].value + 1, (u32)(val >> 32));
}
OP_HANDLER(A32GetVector) {
    int index = (int)inst.args[0].value;
    unsigned __int128 res = 0;
    res |= (unsigned __int128)self.GetVFPReg(index * 4);
    res |= (unsigned __int128)self.GetVFPReg(index * 4 + 1) << 32;
    res |= (unsigned __int128)self.GetVFPReg(index * 4 + 2) << 64;
    res |= (unsigned __int128)self.GetVFPReg(index * 4 + 3) << 96;
    results[inst.result_index] = res;
}
OP_HANDLER(A32SetVector) {
    int index = (int)inst.args[0].value;
    unsigned __int128 val = GetArg(inst, results, 1);
    self.SetVFPReg(index * 4, (u32)val);
    self.SetVFPReg(index * 4 + 1, (u32)(val >> 32));
    self.SetVFPReg(index * 4 + 2, (u32)(val >> 64));
    self.SetVFPReg(index * 4 + 3, (u32)(val >> 96));
}
OP_HANDLER(Pack2x64To1x128) {
    results[inst.result_index] = (GetArg(inst, results, 0) & 0xFFFFFFFFFFFFFFFFULL) | (GetArg(inst, results, 1) << 64);
}
OP_HANDLER(GetNZCVFromOp) { results[inst.result_index] = self.flags_buffer[inst.args[0].value]; }
OP_HANDLER(GetCarryFromOp) { results[inst.result_index] = (self.flags_buffer[inst.args[0].value] >> 1) & 1; }
OP_HANDLER(GetOverflowFromOp) { results[inst.result_index] = self.flags_buffer[inst.args[0].value] & 1; }
OP_HANDLER(RotateRightExtended) {
    u32 val = (u32)GetArg(inst, results, 0);
    u32 carry = (u32)GetArg(inst, results, 1) & 1;
    results[inst.result_index] = (val >> 1) | (carry << 31);
}
OP_HANDLER(NZCVFromPackedFlags) {
    u32 val = (u32)GetArg(inst, results, 0);
    results[inst.result_index] = val >> 28;
}

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
OP_HANDLER(FPMulAdd32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    float b = std::bit_cast<float>((u32)GetArg(inst, results, 1));
    float c = std::bit_cast<float>((u32)GetArg(inst, results, 2));
    results[inst.result_index] = std::bit_cast<u32>(a + (b * c));
}
OP_HANDLER(FPMulAdd64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    double b = std::bit_cast<double>(GetArg(inst, results, 1));
    double c = std::bit_cast<double>(GetArg(inst, results, 2));
    results[inst.result_index] = std::bit_cast<u64>(a + (b * c));
}
OP_HANDLER(FPMulSub32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    float b = std::bit_cast<float>((u32)GetArg(inst, results, 1));
    float c = std::bit_cast<float>((u32)GetArg(inst, results, 2));
    results[inst.result_index] = std::bit_cast<u32>(a - (b * c));
}
OP_HANDLER(FPMulSub64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    double b = std::bit_cast<double>(GetArg(inst, results, 1));
    double c = std::bit_cast<double>(GetArg(inst, results, 2));
    results[inst.result_index] = std::bit_cast<u64>(a - (b * c));
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
OP_HANDLER(FPMax32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    float b = std::bit_cast<float>((u32)GetArg(inst, results, 1));
    results[inst.result_index] = std::bit_cast<u32>(std::max(a, b));
}
OP_HANDLER(FPMax64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    double b = std::bit_cast<double>(GetArg(inst, results, 1));
    results[inst.result_index] = std::bit_cast<u64>(std::max(a, b));
}
OP_HANDLER(FPMin32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    float b = std::bit_cast<float>((u32)GetArg(inst, results, 1));
    results[inst.result_index] = std::bit_cast<u32>(std::min(a, b));
}
OP_HANDLER(FPMin64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    double b = std::bit_cast<double>(GetArg(inst, results, 1));
    results[inst.result_index] = std::bit_cast<u64>(std::min(a, b));
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
    int fbits = (int)GetArg(inst, results, 2);
    results[inst.result_index] = (u32)(s32)(a * std::pow(2.0f, fbits));
}
OP_HANDLER(FPSingleToFixedU32) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    int fbits = (int)GetArg(inst, results, 2);
    results[inst.result_index] = (u32)(u32)(a * std::pow(2.0f, fbits));
}
OP_HANDLER(FPDoubleToFixedS64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    int fbits = (int)GetArg(inst, results, 2);
    results[inst.result_index] = (u64)(s64)(a * std::pow(2.0, fbits));
}
OP_HANDLER(FPDoubleToFixedU64) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    int fbits = (int)GetArg(inst, results, 2);
    results[inst.result_index] = (u64)(u64)(a * std::pow(2.0, fbits));
}
OP_HANDLER(FPFixedS32ToSingle) {
    s32 a = (s32)GetArg(inst, results, 0);
    int fbits = (int)GetArg(inst, results, 2);
    results[inst.result_index] = std::bit_cast<u32>(static_cast<float>(static_cast<float>(a) / std::pow(2.0f, fbits)));
}
OP_HANDLER(FPFixedU32ToSingle) {
    u32 a = (u32)GetArg(inst, results, 0);
    int fbits = (int)GetArg(inst, results, 2);
    results[inst.result_index] = std::bit_cast<u32>(static_cast<float>(static_cast<float>(a) / std::pow(2.0f, fbits)));
}
OP_HANDLER(FPFixedS64ToDouble) {
    s64 a = (s64)GetArg(inst, results, 0);
    int fbits = (int)GetArg(inst, results, 2);
    results[inst.result_index] = std::bit_cast<u64>((double)a / std::pow(2.0, fbits));
}
OP_HANDLER(FPFixedU64ToDouble) {
    u64 a = GetArg(inst, results, 0);
    int fbits = (int)GetArg(inst, results, 2);
    results[inst.result_index] = std::bit_cast<u64>((double)a / std::pow(2.0, fbits));
}
OP_HANDLER(FPDoubleToSingle) {
    double a = std::bit_cast<double>(GetArg(inst, results, 0));
    results[inst.result_index] = std::bit_cast<u32>((float)a);
}
OP_HANDLER(FPSingleToDouble) {
    float a = std::bit_cast<float>((u32)GetArg(inst, results, 0));
    results[inst.result_index] = std::bit_cast<u64>((double)a);
}
OP_HANDLER(SignedSaturatedAdd32) {
    s32 a = (s32)GetArg(inst, results, 0);
    s32 b = (s32)GetArg(inst, results, 1);
    s32 res;
    if (__builtin_add_overflow(a, b, &res)) {
        res = (a < 0) ? std::numeric_limits<s32>::min() : std::numeric_limits<s32>::max();
        self.SetCPSR(self.GetCPSR() | (1 << 27));
    }
    results[inst.result_index] = (u32)res;
}
OP_HANDLER(SignedSaturatedSub32) {
    s32 a = (s32)GetArg(inst, results, 0);
    s32 b = (s32)GetArg(inst, results, 1);
    s32 res;
    if (__builtin_sub_overflow(a, b, &res)) {
        res = (a < 0) ? std::numeric_limits<s32>::min() : std::numeric_limits<s32>::max();
        self.SetCPSR(self.GetCPSR() | (1 << 27));
    }
    results[inst.result_index] = (u32)res;
}
OP_HANDLER(UnsignedSaturatedAdd32) {
    u32 a = (u32)GetArg(inst, results, 0);
    u32 b = (u32)GetArg(inst, results, 1);
    u32 res;
    if (__builtin_add_overflow(a, b, &res)) {
        res = std::numeric_limits<u32>::max();
        self.SetCPSR(self.GetCPSR() | (1 << 27));
    }
    results[inst.result_index] = res;
}
OP_HANDLER(UnsignedSaturatedSub32) {
    u32 a = (u32)GetArg(inst, results, 0);
    u32 b = (u32)GetArg(inst, results, 1);
    u32 res;
    if (__builtin_sub_overflow(a, b, &res)) {
        res = 0;
        self.SetCPSR(self.GetCPSR() | (1 << 27));
    }
    results[inst.result_index] = res;
}
OP_HANDLER(Identity) { results[inst.result_index] = GetArg(inst, results, 0); }
OP_HANDLER(Void) {}
OP_HANDLER(GetCFlagFromNZCV) { results[inst.result_index] = (GetArg(inst, results, 0) >> 1) & 1; }
OP_HANDLER(ConditionalSelectNZCV) {
    bool cond_met = false;
    u32 nzcv = (u32)GetArg(inst, results, 0);
    const bool n = (nzcv >> 3) & 1;
    const bool z = (nzcv >> 2) & 1;
    const bool c = (nzcv >> 1) & 1;
    const bool v = nzcv & 1;
    switch ((IR::Cond)inst.args[1].value) {
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
    results[inst.result_index] = cond_met ? GetArg(inst, results, 2) : GetArg(inst, results, 3);
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
        case IR::Opcode::A32BXWritePC: {
            u32 val = (u32)GetArg(inst, results_ptr, 0);
            next_pc = val & ~1;
            if (val & 1) cpsr |= 0x20; else cpsr &= ~0x20;
            branched = true;
        } break;
        case IR::Opcode::A32ReadMemory8: results_ptr[inst.result_index] = cb->MemoryRead8((u32)GetArg(inst, results_ptr, 0)); break;
        case IR::Opcode::A32ReadMemory16: results_ptr[inst.result_index] = cb->MemoryRead16((u32)GetArg(inst, results_ptr, 0)); break;
        case IR::Opcode::A32ReadMemory32: results_ptr[inst.result_index] = cb->MemoryRead32((u32)GetArg(inst, results_ptr, 0)); break;
        case IR::Opcode::A32ReadMemory64: results_ptr[inst.result_index] = cb->MemoryRead64((u32)GetArg(inst, results_ptr, 0)); break;
        case IR::Opcode::A32WriteMemory8: cb->MemoryWrite8((u32)GetArg(inst, results_ptr, 0), (u8)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32WriteMemory16: cb->MemoryWrite16((u32)GetArg(inst, results_ptr, 0), (u16)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32WriteMemory32: cb->MemoryWrite32((u32)GetArg(inst, results_ptr, 0), (u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32WriteMemory64: cb->MemoryWrite64((u32)GetArg(inst, results_ptr, 0), (u64)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::ConditionalSelect32: {
            bool cond_met = CheckCondition(cpsr, (IR::Cond)inst.args[0].value);
            results_ptr[inst.result_index] = cond_met ? GetArg(inst, results_ptr, 1) : GetArg(inst, results_ptr, 2);
        } break;
        case IR::Opcode::GetNZCVFromOp: results_ptr[inst.result_index] = flags_buffer[inst.args[0].value]; break;
        case IR::Opcode::A32GetVector: {
            int index = (int)inst.args[0].value;
            unsigned __int128 res = 0;
            res |= (unsigned __int128)vfp_regs[index * 4];
            res |= (unsigned __int128)vfp_regs[index * 4 + 1] << 32;
            res |= (unsigned __int128)vfp_regs[index * 4 + 2] << 64;
            res |= (unsigned __int128)vfp_regs[index * 4 + 3] << 96;
            results_ptr[inst.result_index] = res;
        } break;
        case IR::Opcode::A32SetVector: {
            int index = (int)inst.args[0].value;
            unsigned __int128 val = GetArg(inst, results_ptr, 1);
            vfp_regs[index * 4] = (u32)val;
            vfp_regs[index * 4 + 1] = (u32)(val >> 32);
            vfp_regs[index * 4 + 2] = (u32)(val >> 64);
            vfp_regs[index * 4 + 3] = (u32)(val >> 96);
        } break;
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
        case IR::Opcode::A32ReadMemory8: results_ptr[inst.result_index] = cb->MemoryRead8((u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32ReadMemory16: results_ptr[inst.result_index] = cb->MemoryRead16((u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32ReadMemory32: results_ptr[inst.result_index] = cb->MemoryRead32((u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32ReadMemory64: results_ptr[inst.result_index] = cb->MemoryRead64((u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32WriteMemory8: cb->MemoryWrite8((u32)GetArg(inst, results_ptr, 1), (u8)GetArg(inst, results_ptr, 2)); break;
        case IR::Opcode::A32WriteMemory16: cb->MemoryWrite16((u32)GetArg(inst, results_ptr, 1), (u16)GetArg(inst, results_ptr, 2)); break;
        case IR::Opcode::A32WriteMemory32: cb->MemoryWrite32((u32)GetArg(inst, results_ptr, 1), (u32)GetArg(inst, results_ptr, 2)); break;
        case IR::Opcode::A32WriteMemory64: cb->MemoryWrite64((u32)GetArg(inst, results_ptr, 1), (u64)GetArg(inst, results_ptr, 2)); break;
        case IR::Opcode::A32ClearExclusive: exclusive_monitor.ClearExclusive(config.processor_id); break;
        case IR::Opcode::A32ExclusiveReadMemory8: results_ptr[inst.result_index] = exclusive_monitor.ExclusiveRead8(config.processor_id, (u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32ExclusiveReadMemory16: results_ptr[inst.result_index] = exclusive_monitor.ExclusiveRead16(config.processor_id, (u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32ExclusiveReadMemory32: results_ptr[inst.result_index] = exclusive_monitor.ExclusiveRead32(config.processor_id, (u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32ExclusiveReadMemory64: results_ptr[inst.result_index] = exclusive_monitor.ExclusiveRead64(config.processor_id, (u32)GetArg(inst, results_ptr, 1)); break;
        case IR::Opcode::A32ExclusiveWriteMemory8: results_ptr[inst.result_index] = exclusive_monitor.ExclusiveWrite8(config.processor_id, (u32)GetArg(inst, results_ptr, 1), (u8)GetArg(inst, results_ptr, 2)) ? 0 : 1; break;
        case IR::Opcode::A32ExclusiveWriteMemory16: results_ptr[inst.result_index] = exclusive_monitor.ExclusiveWrite16(config.processor_id, (u32)GetArg(inst, results_ptr, 1), (u16)GetArg(inst, results_ptr, 2)) ? 0 : 1; break;
        case IR::Opcode::A32ExclusiveWriteMemory32: results_ptr[inst.result_index] = exclusive_monitor.ExclusiveWrite32(config.processor_id, (u32)GetArg(inst, results_ptr, 1), (u32)GetArg(inst, results_ptr, 2)) ? 0 : 1; break;
        case IR::Opcode::A32ExclusiveWriteMemory64: results_ptr[inst.result_index] = exclusive_monitor.ExclusiveWrite64(config.processor_id, (u32)GetArg(inst, results_ptr, 1), (u64)GetArg(inst, results_ptr, 2)) ? 0 : 1; break;
        case IR::Opcode::A32SetCheckBit: break;
        case IR::Opcode::A32UpdateUpperLocationDescriptor: break;
        case IR::Opcode::A32CallSupervisor: cb->CallSVC((u32)inst.args[0].value); break;
        case IR::Opcode::A32ExceptionRaised: cb->ExceptionRaised(regs[15], (Dynarmic::A32::Exception)GetArg(inst, results_ptr, 1)); break;
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
            double a = std::bit_cast<double>(GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>(GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u64>(a + b);
        } break;
        case IR::Opcode::FPSub32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u32>(a - b);
        } break;
        case IR::Opcode::FPSub64: {
            double a = std::bit_cast<double>(GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>(GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u64>(a - b);
        } break;
        case IR::Opcode::FPMul32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u32>(a * b);
        } break;
        case IR::Opcode::FPMul64: {
            double a = std::bit_cast<double>(GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>(GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u64>(a * b);
        } break;
        case IR::Opcode::FPMulAdd32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            float c = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 2));
            results_ptr[inst.result_index] = std::bit_cast<u32>(a + (b * c));
        } break;
        case IR::Opcode::FPMulAdd64: {
            double a = std::bit_cast<double>(GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>(GetArg(inst, results_ptr, 1));
            double c = std::bit_cast<double>(GetArg(inst, results_ptr, 2));
            results_ptr[inst.result_index] = std::bit_cast<u64>(a + (b * c));
        } break;
        case IR::Opcode::FPMulSub32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            float c = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 2));
            results_ptr[inst.result_index] = std::bit_cast<u32>(a - (b * c));
        } break;
        case IR::Opcode::FPMulSub64: {
            double a = std::bit_cast<double>(GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>(GetArg(inst, results_ptr, 1));
            double c = std::bit_cast<double>(GetArg(inst, results_ptr, 2));
            results_ptr[inst.result_index] = std::bit_cast<u64>(a - (b * c));
        } break;
        case IR::Opcode::FPDiv32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u32>(a / b);
        } break;
        case IR::Opcode::FPDiv64: {
            double a = std::bit_cast<double>(GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>(GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u64>(a / b);
        } break;
        case IR::Opcode::FPMax32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u32>(std::max(a, b));
        } break;
        case IR::Opcode::FPMax64: {
            double a = std::bit_cast<double>(GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>(GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u64>(std::max(a, b));
        } break;
        case IR::Opcode::FPMin32: {
            float a = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 0));
            float b = std::bit_cast<float>((u32)GetArg(inst, results_ptr, 1));
            results_ptr[inst.result_index] = std::bit_cast<u32>(std::min(a, b));
        } break;
        case IR::Opcode::FPMin64: {
            double a = std::bit_cast<double>(GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>(GetArg(inst, results_ptr, 1));
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
            double a = std::bit_cast<double>(GetArg(inst, results_ptr, 0));
            results_ptr[inst.result_index] = std::bit_cast<u64>(std::sqrt(a));
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
            double a = std::bit_cast<double>(GetArg(inst, results_ptr, 0));
            double b = std::bit_cast<double>(GetArg(inst, results_ptr, 1));
            if (std::isnan(a) || std::isnan(b)) results_ptr[inst.result_index] = 3;
            else if (a == b) results_ptr[inst.result_index] = 0;
            else if (a < b) results_ptr[inst.result_index] = 1;
            else results_ptr[inst.result_index] = 2;
        } break;
        case IR::Opcode::FPDoubleToSingle: results_ptr[inst.result_index] = std::bit_cast<u32>((float)std::bit_cast<double>(GetArg(inst, results_ptr, 0))); break;
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
    cb->AddTicks(std::max<u64>(1, (u64)executed_count * 2));
}

} // namespace Core
