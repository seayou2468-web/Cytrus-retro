#include "core/arm/dynarmic/static_ir_executor.h"
#include "core/arm/dynarmic/arm_dynarmic.h"
#include "common/logging/log.h"

namespace Core {

StaticIRExecutor::StaticIRExecutor(ARM_Dynarmic& parent_) : parent(parent_) {}
StaticIRExecutor::~StaticIRExecutor() = default;

void StaticIRExecutor::Execute(u32 pc) {
    auto& block = GetOrCompileBlock(pc);

    for (const auto& instr : block.instructions) {
        switch (instr.opcode) {
        case IROpcode::SetRegister:
            parent.SetReg(instr.dest, instr.arg1);
            break;
        case IROpcode::GetRegister:
            // instr.dest = parent.GetReg(instr.arg1);
            break;
        case IROpcode::Add:
            parent.SetReg(instr.dest, parent.GetReg(instr.arg1) + parent.GetReg(instr.arg2));
            break;
        case IROpcode::Sub:
            parent.SetReg(instr.dest, parent.GetReg(instr.arg1) - parent.GetReg(instr.arg2));
            break;
        case IROpcode::Svc:
            // Handle SVC
            break;
        case IROpcode::Branch:
            parent.SetPC(instr.arg1);
            return;
        default:
            LOG_ERROR(Core_ARM11, "Unknown IR Opcode");
            break;
        }
    }
}

IRBlock& StaticIRExecutor::GetOrCompileBlock(u32 pc) {
    if (block_cache.find(pc) != block_cache.end()) {
        return block_cache[pc];
    }

    // "Compile" (Translate ARM to IR)
    // This is a placeholder for a real translator.
    // For now, we fetch the actual ARM instruction to demonstrate the flow.
    u32 instruction = parent.system.Memory().Read32(pc);
    (void)instruction;

    IRBlock block;
    // Dummy: just set PC to next instruction for now
    block.instructions.push_back({IROpcode::Branch, pc + 4, 0, 0});

    block_cache[pc] = block;
    return block_cache[pc];
}

} // namespace Core
