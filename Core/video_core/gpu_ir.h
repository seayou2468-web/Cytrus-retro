#pragma once

#include <vector>
#include "common/common_types.h"
#include "video_core/pica/regs_external.h"

namespace VideoCore {

enum class GpuOpcode {
    DrawArrays,
    DrawElements,
    SetState,
    SetTexture,
    SetShader,
    Clear,
    // Add more as needed
};

struct GpuInstruction {
    GpuOpcode opcode;
    std::vector<u32> data;
};

class GpuIr {
public:
    void AddInstruction(GpuInstruction inst) {
        instructions.push_back(std::move(inst));
    }

    void Clear() {
        instructions.clear();
    }

    const std::vector<GpuInstruction>& GetInstructions() const {
        return instructions;
    }

private:
    std::vector<GpuInstruction> instructions;
};

} // namespace VideoCore
