#include "video_core/pica/regs_lcd.h"
#include "video_core/renderer_software/renderer_software.h"
#include <algorithm>
#include <cstring>
#include "common/color.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/memory.h"
#include "video_core/pica/pica_core.h"
#include "video_core/pica/regs_external.h"

namespace SwRenderer {

struct FramebufferState {
    PAddr address;
    u32 width;
    u32 height;
    u32 stride;
    Pica::PixelFormat format;
    Pica::ColorFill color_fill;
};

// Global states to avoid changing the class definition in the header (ODR fix)
static std::array<FramebufferState, 3> g_fb_states{};

RendererSoftware::RendererSoftware(Core::System& system, Pica::PicaCore& pica,
                                  Frontend::EmuWindow& window)
    : RendererBase(system, window, nullptr), pica(pica), memory(system.Memory()), rasterizer(system.Memory(), pica) {}

RendererSoftware::~RendererSoftware() = default;

void RendererSoftware::SwapBuffers() {
    if (system.perf_stats) system.perf_stats->StartSwap();
    PrepareRenderTarget();
    if (system.perf_stats) system.perf_stats->EndSwap();
}

void RendererSoftware::PrepareRenderTarget() {
    const auto& regs_lcd = pica.regs_lcd;
    for (u32 i = 0; i < 3; i++) {
        const u32 fb_id = (i == 1) ? 1 : 0;
        const auto& framebuffer = pica.regs.framebuffer_config[fb_id];
        auto& state = g_fb_states[i];

        if (i == 0) state.address = (framebuffer.active_fb == 0) ? framebuffer.address_left1 : framebuffer.address_left2;
        else if (i == 1) state.address = (framebuffer.active_fb == 0) ? framebuffer.address_left1 : framebuffer.address_left2;
        else if (i == 2) state.address = (framebuffer.active_fb == 0) ? framebuffer.address_right1 : framebuffer.address_right2;

        state.format = framebuffer.color_format;
        state.stride = framebuffer.stride / Pica::BytesPerPixel(state.format);
        state.height = framebuffer.height; // 240
        state.width = state.stride; // 400 or 320
        state.color_fill = (fb_id == 0) ? regs_lcd.color_fill_top : regs_lcd.color_fill_bottom;
    }
}

template <typename DecodeFunc>
static void RenderScreen(u32* output_data, const u8* src_data, u32 width, u32 height, u32 stride, u32 bpp, u32 dest_x, u32 dest_y, u32 dest_pitch, DecodeFunc decode) {
    for (u32 ly = 0; ly < height; ly++) {
        u32* dest_row = output_data + (dest_y + ly) * (dest_pitch / 4) + dest_x;
        for (u32 lx = 0; lx < width; lx++) {
            // 3DS is column-major: Landscape(lx, ly) = Portrait(239 - ly, lx)
            // Offset = lx * 240 + (239 - ly)
            const u8* pixel = src_data + (lx * height + (height - 1 - ly)) * bpp;
            dest_row[lx] = decode(pixel);
        }
    }
}

// Global helper for Libretro
void LibretroRenderOptimized(Core::System& system, u32* output_data, u32 output_pitch, bool side_by_side) {
    auto& memory = system.Memory();
    for (u32 i = 0; i < 2; i++) {
        const auto& state = g_fb_states[i];
        if (state.address == 0) continue;

        const u8* src_data = memory.GetPhysicalPointer(state.address);
        if (!src_data) continue;

        u32 dx = 0, dy = 0;
        if (i == 1) { // Bottom screen
            dx = side_by_side ? 400 : 40;
            dy = side_by_side ? 0 : 240;
        }

        if (state.color_fill.is_enabled) {
            u32 color = (state.color_fill.color_r) | (state.color_fill.color_g << 8) | (state.color_fill.color_b << 16) | (255 << 24);
            u32 sw = (i == 0) ? 400 : 320;
            u32 sh = 240;
            for (u32 y = 0; y < sh; y++) {
                std::fill_n(output_data + (dy + y) * (output_pitch / 4) + dx, sw, color);
            }
            continue;
        }

        u32 bpp = Pica::BytesPerPixel(state.format);
        switch (state.format) {
        case Pica::PixelFormat::RGBA8:
            RenderScreen(output_data, src_data, state.width, state.height, state.stride, bpp, dx, dy, output_pitch, [](const u8* p) {
                return (p[3] << 0) | (p[2] << 8) | (p[1] << 16) | (p[0] << 24);
            });
            break;
        case Pica::PixelFormat::RGB8:
            RenderScreen(output_data, src_data, state.width, state.height, state.stride, bpp, dx, dy, output_pitch, [](const u8* p) {
                return (p[2] << 0) | (p[1] << 8) | (p[0] << 16) | (255 << 24);
            });
            break;
        case Pica::PixelFormat::RGB565:
            RenderScreen(output_data, src_data, state.width, state.height, state.stride, bpp, dx, dy, output_pitch, [](const u8* p) {
                u16_le pixel; std::memcpy(&pixel, p, 2);
                return (Common::Color::Convert5To8((pixel >> 11) & 0x1F) << 0) |
                       (Common::Color::Convert6To8((pixel >> 5) & 0x3F) << 8) |
                       (Common::Color::Convert5To8(pixel & 0x1F) << 16) | (255 << 24);
            });
            break;
        case Pica::PixelFormat::RGB5A1:
            RenderScreen(output_data, src_data, state.width, state.height, state.stride, bpp, dx, dy, output_pitch, [](const u8* p) {
                u16_le pixel; std::memcpy(&pixel, p, 2);
                return (Common::Color::Convert5To8((pixel >> 11) & 0x1F) << 0) |
                       (Common::Color::Convert5To8((pixel >> 6) & 0x1F) << 8) |
                       (Common::Color::Convert5To8((pixel >> 1) & 0x1F) << 16) |
                       (Common::Color::Convert1To8(pixel & 0x1) << 24);
            });
            break;
        case Pica::PixelFormat::RGBA4:
            RenderScreen(output_data, src_data, state.width, state.height, state.stride, bpp, dx, dy, output_pitch, [](const u8* p) {
                u16_le pixel; std::memcpy(&pixel, p, 2);
                return (Common::Color::Convert4To8((pixel >> 12) & 0xF) << 0) |
                       (Common::Color::Convert4To8((pixel >> 8) & 0xF) << 8) |
                       (Common::Color::Convert4To8((pixel >> 4) & 0xF) << 16) |
                       (Common::Color::Convert4To8(pixel & 0xF) << 24);
            });
            break;
        default:
            break;
        }
    }
}

void RendererSoftware::LoadFBToScreenInfo(int i, const Pica::ColorFill& color_fill) {}

} // namespace SwRenderer
