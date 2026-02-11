#include "emu_window.h"
#include <cstring>
#include "common/settings.h"
#include "core/core.h"
#include "video_core/gpu.h"
#include "video_core/renderer_software/renderer_software.h"

LibretroEmuWindow::LibretroEmuWindow() : EmuWindow() {
    video_width = 400;
    video_height = 480;
    video_buffer.resize(video_width * video_height, 0xFF000000);
}

LibretroEmuWindow::~LibretroEmuWindow() = default;

void LibretroEmuWindow::PollEvents() {
}

void LibretroEmuWindow::SetVideoBuffer(const void* data, unsigned width, unsigned height) {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    if (width == video_width && height == video_height) {
        std::memcpy(video_buffer.data(), data, width * height * 4);
    }
}

// We need to override SwapBuffers to pull from Software Renderer if it's being used
// Wait, EmuWindow doesn't have SwapBuffers as virtual?
// Yes it does, inherited from GraphicsContext.
void LibretroEmuWindow::SwapBuffers() {
    auto& system = Core::System::GetInstance();
    auto& renderer = system.GPU().Renderer();

    // Check if it's Software Renderer
    auto* sw_renderer = dynamic_cast<SwRenderer::RendererSoftware*>(&renderer);
    if (sw_renderer) {
        std::lock_guard<std::mutex> lock(buffer_mutex);

        const auto& top = sw_renderer->Screen(VideoCore::ScreenId::TopLeft);
        const auto& bottom = sw_renderer->Screen(VideoCore::ScreenId::Bottom);

        // 3DS screens are native 240x400 and 240x320.
        // In RendererSoftware, they are converted to RGBA8.

        // Top screen: 400x240 (after rotation)
        if (!top.pixels.empty()) {
            for (u32 y = 0; y < 240; ++y) {
                for (u32 x = 0; x < 400; ++x) {
                    // RendererSoftware stores pixels as (x * height + y)
                    // We want (y * 400 + x)
                    u32 src_idx = (x * 240 + (239 - y)) * 4;
                    if (src_idx + 3 < top.pixels.size()) {
                        u32 r = top.pixels[src_idx + 0];
                        u32 g = top.pixels[src_idx + 1];
                        u32 b = top.pixels[src_idx + 2];
                        u32 a = top.pixels[src_idx + 3];
                        video_buffer[y * 400 + x] = (a << 24) | (r << 16) | (g << 8) | b;
                    }
                }
            }
        }

        // Bottom screen: 320x240 (after rotation), centered in 400x240
        if (!bottom.pixels.empty()) {
            u32 x_offset = (400 - 320) / 2;
            for (u32 y = 0; y < 240; ++y) {
                for (u32 x = 0; x < 320; ++x) {
                    u32 src_idx = (x * 240 + (239 - y)) * 4;
                    if (src_idx + 3 < bottom.pixels.size()) {
                        u32 r = bottom.pixels[src_idx + 0];
                        u32 g = bottom.pixels[src_idx + 1];
                        u32 b = bottom.pixels[src_idx + 2];
                        u32 a = bottom.pixels[src_idx + 3];
                        video_buffer[(y + 240) * 400 + (x + x_offset)] = (a << 24) | (r << 16) | (g << 8) | b;
                    }
                }
            }
        }
    }
}
