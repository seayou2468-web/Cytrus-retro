#include "audio_core/lle/ir_interpreter.h"
#include "common/logging/log.h"
#include "teakra/src/decoder.h"

namespace AudioCore::Teak {

IRInterpreter::IRInterpreter(State& state) : state(state) {
    decoder_table = ::GetDecoderTable<Teakra::IRGenerator>();
}

void IRInterpreter::Run(u32 cycles) {
    while (cycles > 0) {
        if (auto it = hle_functions.find(state.pc); it != hle_functions.end()) {
            it->second(*this);
            if (cycles > 0) cycles--;
            continue;
        }

        auto it = block_cache.find(state.pc);
        if (it == block_cache.end()) {
            it = block_cache.emplace(state.pc, CompileBlock(state.pc)).first;
        }

        const auto& block = it->second;
        ExecuteBlock(block);

        if (cycles >= block.cycle_count) {
            cycles -= block.cycle_count;
        } else {
            cycles = 0;
        }
    }
}

static u64 SignExtend40(u64 val) {
    if (val & 0x8000000000ULL)
        return val | 0xFFFFFF0000000000ULL;
    return val & 0xFFFFFFFFFFULL;
}

bool CheckCondition(const Teakra::RegisterState& state, u8 cond_raw) {
    CondValue cond = static_cast<CondValue>(cond_raw);
    switch (cond) {
    case CondValue::True: return true;
    case CondValue::Eq: return state.fz == 1;
    case CondValue::Neq: return state.fz == 0;
    case CondValue::Gt: return state.fz == 0 && state.fm == 0;
    case CondValue::Ge: return state.fm == 0;
    case CondValue::Lt: return state.fm == 1;
    case CondValue::Le: return state.fm == 1 || state.fz == 1;
    case CondValue::Nn: return state.fn == 0;
    case CondValue::C: return state.fc0 == 1;
    case CondValue::V: return state.fv == 1;
    case CondValue::E: return state.fe == 1;
    case CondValue::L: return state.flm == 1 || state.fvl == 1;
    case CondValue::Nr: return state.fr == 0;
    case CondValue::Niu0: return state.iu[0] == 0;
    case CondValue::Iu0: return state.iu[0] == 1;
    case CondValue::Iu1: return state.iu[1] == 1;
    default: return true;
    }
}

void IRInterpreter::ExecuteBlock(const IRBlock& block) {
    auto GetReg = [&](u8 reg) -> u64 {
        IRReg r = (IRReg)reg;
        switch (r) {
        case IRReg::a0: return state.a[0];
        case IRReg::a1: return state.a[1];
        case IRReg::b0: return state.b[0];
        case IRReg::b1: return state.b[1];
        case IRReg::a0l: return state.a[0] & 0xFFFF;
        case IRReg::a1l: return state.a[1] & 0xFFFF;
        case IRReg::b0l: return state.b[0] & 0xFFFF;
        case IRReg::b1l: return state.b[1] & 0xFFFF;
        case IRReg::a0h: return (state.a[0] >> 16) & 0xFFFF;
        case IRReg::a1h: return (state.a[1] >> 16) & 0xFFFF;
        case IRReg::b0h: return (state.b[0] >> 16) & 0xFFFF;
        case IRReg::b1h: return (state.b[1] >> 16) & 0xFFFF;
        case IRReg::r0: case IRReg::r1: case IRReg::r2: case IRReg::r3:
        case IRReg::r4: case IRReg::r5: case IRReg::r6: case IRReg::r7:
            return state.r[reg - (u8)IRReg::r0];
        case IRReg::x0: return state.x[0];
        case IRReg::x1: return state.x[1];
        case IRReg::y0: return state.y[0];
        case IRReg::y1: return state.y[1];
        case IRReg::pc: return state.pc;
        case IRReg::sp: return state.sp;
        case IRReg::sv: return state.sv;
        case IRReg::lc: return state.Lc();
        case IRReg::page: return state.page;
        case IRReg::p0l: return state.p[0] & 0xFFFF;
        case IRReg::p0h: return (state.p[0] >> 16) & 0xFFFF;
        case IRReg::p1l: return state.p[1] & 0xFFFF;
        case IRReg::p1h: return (state.p[1] >> 16) & 0xFFFF;
        default:
            if (reg >= (u8)IRReg::temp0) return ir_regs[reg - (u8)IRReg::temp0];
            return 0;
        }
    };

    auto SetReg = [&](u8 reg, u64 val) {
        IRReg r = (IRReg)reg;
        switch (r) {
        case IRReg::a0: state.a[0] = AudioCore::Teak::SignExtend40(val); break;
        case IRReg::a1: state.a[1] = AudioCore::Teak::SignExtend40(val); break;
        case IRReg::b0: state.b[0] = AudioCore::Teak::SignExtend40(val); break;
        case IRReg::b1: state.b[1] = AudioCore::Teak::SignExtend40(val); break;
        case IRReg::r0: case IRReg::r1: case IRReg::r2: case IRReg::r3:
        case IRReg::r4: case IRReg::r5: case IRReg::r6: case IRReg::r7:
            state.r[reg - (u8)IRReg::r0] = (u16)val; break;
        case IRReg::x0: state.x[0] = (u16)val; break;
        case IRReg::x1: state.x[1] = (u16)val; break;
        case IRReg::y0: state.y[0] = (u16)val; break;
        case IRReg::y1: state.y[1] = (u16)val; break;
        case IRReg::pc: state.pc = (u32)val; break;
        case IRReg::sp: state.sp = (u16)val; break;
        case IRReg::sv: state.sv = (u16)val; break;
        case IRReg::page: state.page = (u16)val; break;
        default:
            if (reg >= (u8)IRReg::temp0) ir_regs[reg - (u8)IRReg::temp0] = val;
            break;
        }
    };

    u32 next_pc = block.next_pc_default;
    state.pc = next_pc;

    for (const auto& inst : block.insts) {
        switch (inst.op) {
        case IROp::LoadImm:
            SetReg(inst.dest, inst.imm);
            break;
        case IROp::AccAdd:
            SetReg(inst.dest, GetReg(inst.src1) + GetReg(inst.src2));
            break;
        case IROp::AccSub:
            SetReg(inst.dest, GetReg(inst.src1) - GetReg(inst.src2));
            break;
        case IROp::AccAnd:
            SetReg(inst.dest, GetReg(inst.src1) & GetReg(inst.src2));
            break;
        case IROp::Move:
            SetReg(inst.dest, GetReg(inst.src1));
            break;
        case IROp::LoadMem:
            SetReg(inst.dest, ReadMemory((u32)GetReg(inst.src1)));
            break;
        case IROp::StoreMem:
            WriteMemory((u32)GetReg(inst.dest), (u16)GetReg(inst.src1));
            break;
        case IROp::Add16:
            SetReg(inst.dest, (u16)(GetReg(inst.src1) + (u16)inst.imm));
            break;
        case IROp::Shl16:
            SetReg(inst.dest, (u16)(GetReg(inst.src1) << inst.imm));
            break;
        case IROp::Or16:
            SetReg(inst.dest, (u16)(GetReg(inst.src1) | inst.imm));
            break;
        case IROp::Jump:
            if (inst.src2 == (u8)IRReg::temp1) { // Relative
                 state.pc = next_pc + (s32)inst.imm;
            } else {
                 state.pc = (u32)inst.imm;
            }
            break;
        case IROp::JumpCond:
            if (CheckCondition(state, inst.src1)) {
                if (inst.src2 == (u8)IRReg::temp1) {
                    state.pc = next_pc + (s32)inst.imm;
                } else {
                    state.pc = (u32)inst.imm;
                }
            }
            break;
        case IROp::Call:
            if (CheckCondition(state, inst.src1)) {
                u16 l = (u16)(next_pc & 0xFFFF);
                u16 h = (u16)(next_pc >> 16);
                WriteMemory(--state.sp, h);
                WriteMemory(--state.sp, l);
                state.pc = (u32)inst.imm;
            }
            break;
        case IROp::Ret:
            if (CheckCondition(state, inst.src1)) {
                u16 l = ReadMemory(state.sp++);
                u16 h = ReadMemory(state.sp++);
                state.pc = l | ((u32)h << 16);
            }
            break;
        case IROp::Push:
            WriteMemory(--state.sp, (u16)GetReg(inst.src1));
            break;
        case IROp::Pop:
            SetReg(inst.dest, ReadMemory(state.sp++));
            break;
        case IROp::ModifyRn:
            {
                u8 unit = inst.dest;
                u8 step = inst.src1;
                if (step == 1) state.r[unit]++;
                else if (step == 2) state.r[unit]--;
            }
            break;
        case IROp::Multiply:
            state.p[0] = (u32)state.x[0] * (u32)state.y[0];
            break;
        case IROp::UpdateFlags:
            {
                u64 val = GetReg(inst.src1);
                state.fz = (val == 0);
                state.fm = (val >> 39) & 1;
            }
            break;
        case IROp::TeakGeneric:
            break;
        case IROp::Nop:
            break;
        default:
            LOG_ERROR(Audio_DSP, "Unimplemented IR Op: {}", (int)inst.op);
            break;
        }
    }
}

IRBlock IRInterpreter::CompileBlock(u32 pc) {
    IRBlock block;
    Teakra::IRGenerator generator(block);

    u16 opcode = ReadMemory(pc);
    auto& matcher = decoder_table[opcode];

    u16 expansion = 0;
    if (matcher.NeedExpansion()) {
        expansion = ReadMemory(pc + 1);
        block.cycle_count = 2;
    } else {
        block.cycle_count = 1;
    }

    matcher.call(generator, opcode, expansion);

    block.next_pc_default = pc + (matcher.NeedExpansion() ? 2 : 1);

    return block;
}

void IRInterpreter::RegisterHLE(u32 address, std::function<void(IRInterpreter&)> func) {
    hle_functions[address] = std::move(func);
}

} // namespace AudioCore::Teak
