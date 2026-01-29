#ifndef LIBRETRO_EMU_WINDOW_H__
#define LIBRETRO_EMU_WINDOW_H__

#include "core/frontend/emu_window.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_software/renderer_software.h"

namespace Frontend {

class LibretroEmuWindow : public EmuWindow {
public:
    LibretroEmuWindow() : EmuWindow() {}
    ~LibretroEmuWindow() override = default;

    void PollEvents() override {}

    void MakeCurrent() override {}
    void DoneCurrent() override {}

    void Present(const SwRenderer::RendererSoftware& renderer, retro_video_refresh_t video_cb, const std::string& layout_option) {
        const auto& top_left = renderer.Screen(VideoCore::ScreenId::TopLeft);
        const auto& bottom = renderer.Screen(VideoCore::ScreenId::Bottom);

        if (layout_option == "side") {
            unsigned width = 400 + 320;
            unsigned height = 240;
            std::vector<uint32_t> combined_buffer(width * height, 0);

            for (unsigned y = 0; y < 240; y++) {
                for (unsigned x = 0; x < 400; x++) {
                    uint32_t color;
                    std::memcpy(&color, &top_left.pixels[(x * 240 + y) * 4], 4);
                    combined_buffer[y * width + x] = color;
                }
                for (unsigned x = 0; x < 320; x++) {
                    uint32_t color;
                    std::memcpy(&color, &bottom.pixels[(x * 240 + y) * 4], 4);
                    combined_buffer[y * width + (x + 400)] = color;
                }
            }
            video_cb(combined_buffer.data(), width, height, width * 4);
        } else if (layout_option == "single_top") {
            unsigned width = 400;
            unsigned height = 240;
            std::vector<uint32_t> combined_buffer(width * height, 0);
            for (unsigned y = 0; y < 240; y++) {
                for (unsigned x = 0; x < 400; x++) {
                    uint32_t color;
                    std::memcpy(&color, &top_left.pixels[(x * 240 + y) * 4], 4);
                    combined_buffer[y * width + x] = color;
                }
            }
            video_cb(combined_buffer.data(), width, height, width * 4);
        } else if (layout_option == "single_bottom") {
            unsigned width = 320;
            unsigned height = 240;
            std::vector<uint32_t> combined_buffer(width * height, 0);
            for (unsigned y = 0; y < 240; y++) {
                for (unsigned x = 0; x < 320; x++) {
                    uint32_t color;
                    std::memcpy(&color, &bottom.pixels[(x * 240 + y) * 4], 4);
                    combined_buffer[y * width + x] = color;
                }
            }
            video_cb(combined_buffer.data(), width, height, width * 4);
        } else { // default "vertical"
            unsigned width = 400;
            unsigned height = 480;
            std::vector<uint32_t> combined_buffer(width * height, 0);

            for (unsigned y = 0; y < 240; y++) {
                for (unsigned x = 0; x < 400; x++) {
                    uint32_t color;
                    std::memcpy(&color, &top_left.pixels[(x * 240 + y) * 4], 4);
                    combined_buffer[y * width + x] = color;
                }
            }
            unsigned x_offset = (400 - 320) / 2;
            for (unsigned y = 0; y < 240; y++) {
                for (unsigned x = 0; x < 320; x++) {
                    uint32_t color;
                    std::memcpy(&color, &bottom.pixels[(x * 240 + y) * 4], 4);
                    combined_buffer[(y + 240) * width + (x + x_offset)] = color;
                }
            }
            video_cb(combined_buffer.data(), width, height, width * 4);
        }
    }
};

} // namespace Frontend

#endif
