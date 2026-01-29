#pragma once

#include <vector>
#include <map>
#include <dynarmic/interface/A32/a32.h>
#include "common/common_types.h"

namespace Core {

class ARM_Dynarmic;

enum class IROpcode : u8 {
    SetRegister,
    GetRegister,
    Add,
    Sub,
    Mul,
    Div,
    Ldr,
    Str,
    Svc,
    Branch,
    // Add more as needed
};

struct IRInstruction {
    IROpcode opcode;
    u32 arg1;
    u32 arg2;
    u32 dest;
};

struct IRBlock {
    std::vector<IRInstruction> instructions;
};

class StaticIRExecutor {
public:
    explicit StaticIRExecutor(ARM_Dynarmic& parent);
    ~StaticIRExecutor();

    void Execute(u32 pc);

private:
    ARM_Dynarmic& parent;
    std::map<u32, IRBlock> block_cache;

    IRBlock& GetOrCompileBlock(u32 pc);
};

} // namespace Core
