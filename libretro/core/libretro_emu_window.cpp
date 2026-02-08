#include "libretro_emu_window.h"
#include "core/core.h"
#include "video_core/gpu.h"
#include "video_core/renderer_software/renderer_software.h"
#include <cstring>
#include <algorithm>

namespace Frontend {

LibretroEmuWindow::LibretroEmuWindow(bool is_secondary) : EmuWindow(is_secondary) {
    Layout::FramebufferLayout layout;
    layout.width = 400;
    layout.height = 480;
    NotifyFramebufferLayoutChanged(layout);
    framebuffer.resize(layout.width * layout.height, 0);
}

LibretroEmuWindow::~LibretroEmuWindow() = default;

void LibretroEmuWindow::PollEvents() {}

void LibretroEmuWindow::SwapBuffers() {
    auto& system = Core::System::GetInstance();
    auto& gpu = system.GPU();
    auto& renderer = static_cast<SwRenderer::RendererSoftware&>(gpu.Renderer());

    const auto& top = renderer.Screen(VideoCore::ScreenId::TopLeft);
    const auto& bottom = renderer.Screen(VideoCore::ScreenId::Bottom);

    // Top: 400x240
    // Bottom: 320x240
    // Combined: 400x480

    // Clear background
    std::fill(framebuffer.begin(), framebuffer.end(), 0);

    if (top.pixels.size() >= 400 * 240 * 4) {
        for (u32 y = 0; y < 240; y++) {
            std::memcpy(&framebuffer[y * 400], &top.pixels[y * top.width * 4], 400 * 4);
        }
    }

    if (bottom.pixels.size() >= 320 * 240 * 4) {
        u32 x_offset = (400 - 320) / 2;
        for (u32 y = 0; y < 240; y++) {
            std::memcpy(&framebuffer[(y + 240) * 400 + x_offset], &bottom.pixels[y * bottom.width * 4], 320 * 4);
        }
    }

    frame_done = true;
}

void LibretroEmuWindow::MakeCurrent() {}
void LibretroEmuWindow::DoneCurrent() {}

} // namespace Frontend
