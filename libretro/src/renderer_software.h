// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <vector>
#include "common/common_types.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_software/sw_rasterizer.h"
#include "video_core/pica/regs_lcd.h"
#include "video_core/pica/regs_external.h"

namespace Core {
class System;
}

namespace SwRenderer {

struct ScreenInfo {
    u32 width;
    u32 height;
    std::vector<u8> pixels;
};

struct FramebufferState {
    PAddr address;
    u32 width;
    u32 height;
    u32 stride;
    Pica::PixelFormat format;
    Pica::ColorFill color_fill;
};

class RendererSoftware : public VideoCore::RendererBase {
public:
    explicit RendererSoftware(Core::System& system, Pica::PicaCore& pica,
                              Frontend::EmuWindow& window);
    ~RendererSoftware() override;

    [[nodiscard]] VideoCore::RasterizerInterface* Rasterizer() override {
        return &rasterizer;
    }

    [[nodiscard]] const ScreenInfo& Screen(VideoCore::ScreenId id) const noexcept {
        return screen_infos[static_cast<u32>(id)];
    }

    void SwapBuffers() override;
    void TryPresent(int timeout_ms, bool is_secondary) override {}

    void RenderToLibretro(u32* output_data, u32 output_pitch, bool side_by_side);

private:
    void PrepareRenderTarget();
    void LoadFBToScreenInfo(int i, const Pica::ColorFill& color_fill);

private:
    Memory::MemorySystem& memory;
    Pica::PicaCore& pica;
    RasterizerSoftware rasterizer;
    std::array<ScreenInfo, 3> screen_infos{};
    std::array<FramebufferState, 3> fb_states{};
};

} // namespace SwRenderer
