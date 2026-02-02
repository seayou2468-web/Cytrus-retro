// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/arch.h"
#include "video_core/shader/shader_interpreter.h"
#include "video_core/shader/shader.h"

namespace Pica {

std::unique_ptr<ShaderEngine> CreateEngine() {
    return std::make_unique<Shader::InterpreterEngine>();
}

} // namespace Pica
