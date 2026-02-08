#pragma once

#include <array>
#include <vector>
#include <unordered_map>
#include <functional>
#include <type_traits>
#include "common/common_types.h"
#include "teakra/src/matcher.h"
#include "teakra/src/operand.h"
#include "teakra/src/decoder.h"
#include "teakra/include/teakra/impl/register.h"

namespace AudioCore::Teak {

enum class IROp : u8 {
    Nop,
    LoadImm, Move,
    AccAdd, AccSub, AccAnd, AccOr, AccXor, AccNeg, AccAbs, AccSat, AccNot, AccShl, AccShr,
    Add16, Sub16, Mul16, And16, Or16, Xor16, Not16, Shl16, Shr16,
    LoadMem, StoreMem,
    AddAddr, ModAddr,
    Jump, JumpCond, Call, Ret,
    Push, Pop,
    UpdateFlags,
    Multiply, ProductSum,
    ModifyRn,
    TeakGeneric,
};

enum class IRReg : u8 {
    a0, a0l, a0h, a0e,
    a1, a1l, a1h, a1e,
    b0, b0l, b0h, b0e,
    b1, b1l, b1h, b1e,
    r0, r1, r2, r3, r4, r5, r6, r7,
    x0, x1, y0, y1,
    p0h, p0l, p1h, p1l,
    pc, sp, sv, lc,
    st0, st1, st2,
    mod0, mod1, mod2, mod3,
    cfgi, cfgj, page,
    temp0, temp1, temp2, temp3, temp4, temp5,
};

struct IRInst {
    IROp op;
    u8 dest;
    u8 src1;
    u8 src2;
    u64 imm;
};

struct IRBlock {
    std::vector<IRInst> insts;
    u32 next_pc_default;
    u32 cycle_count;
};

} // namespace AudioCore::Teak

namespace Teakra {
    using namespace AudioCore::Teak;

    struct IRGenerator {
        using instruction_return_type = void;
        IRBlock& block;
        IRGenerator(IRBlock& b) : block(b) {}
        void undefined(u16 opcode) {}
        void AddInst(IROp op, u8 dest = 0, u8 src1 = 0, u8 src2 = 0, u64 imm = 0) {
            block.insts.push_back({op, dest, src1, src2, imm});
        }

        template<typename T> u8 RegToU8(T name) {
            RegName r;
            if constexpr (std::is_same_v<T, RegName>) {
                r = name;
            } else {
                r = name.GetName();
            }
            switch (r) {
            case RegName::a0: return (u8)IRReg::a0;
            case RegName::a1: return (u8)IRReg::a1;
            case RegName::b0: return (u8)IRReg::b0;
            case RegName::b1: return (u8)IRReg::b1;
            case RegName::r0: return (u8)IRReg::r0;
            case RegName::r1: return (u8)IRReg::r1;
            case RegName::r2: return (u8)IRReg::r2;
            case RegName::r3: return (u8)IRReg::r3;
            case RegName::r4: return (u8)IRReg::r4;
            case RegName::r5: return (u8)IRReg::r5;
            case RegName::r6: return (u8)IRReg::r6;
            case RegName::r7: return (u8)IRReg::r7;
            case RegName::y0: return (u8)IRReg::y0;
            case RegName::pc: return (u8)IRReg::pc;
            case RegName::sp: return (u8)IRReg::sp;
            case RegName::sv: return (u8)IRReg::sv;
            case RegName::lc: return (u8)IRReg::lc;
            default: return (u8)IRReg::temp0;
            }
        }

#include "audio_core/lle/ir_generator.h"
    };
}

namespace AudioCore::Teak {

class IRInterpreter {
public:
    using State = Teakra::RegisterState;
    IRInterpreter(State& state);
    virtual ~IRInterpreter() = default;
    void Run(u32 cycles);
    void RegisterHLE(u32 address, std::function<void(IRInterpreter&)> func);
    virtual u16 ReadMemory(u32 addr) { return 0; }
    virtual void WriteMemory(u32 addr, u16 value) {}
protected:
    State& state;
private:
    std::array<u64, 64> ir_regs;
    std::unordered_map<u32, IRBlock> block_cache;
    std::unordered_map<u32, std::function<void(IRInterpreter&)>> hle_functions;
    std::vector<Matcher<Teakra::IRGenerator>> decoder_table;
    void ExecuteBlock(const IRBlock& block);
    IRBlock CompileBlock(u32 pc);
};

} // namespace AudioCore::Teak
