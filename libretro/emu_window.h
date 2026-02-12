#pragma once

#include <vector>
#include <mutex>
#include <cassert>
#include <climits>
#include "common/common_types.h"
#include "core/frontend/emu_window.h"

class LibretroEmuWindow : public Frontend::EmuWindow {
public:
    LibretroEmuWindow();
    ~LibretroEmuWindow();

    void PollEvents() override;
    void SwapBuffers() override;

    // Libretro doesn't need these usually as it's not managing the window itself
    void OnMinimalClientAreaChangeRequest(std::pair<u32, u32> minimal_size) override {}

    // Framebuffer access for Libretro
    const void* GetVideoBuffer() const { return video_buffer.data(); }
    unsigned GetVideoWidth() const { return video_width; }
    unsigned GetVideoHeight() const { return video_height; }
    std::size_t GetVideoPitch() const { return video_width * 4; }

    void SetVideoBuffer(const void* data, unsigned width, unsigned height);

private:
    std::vector<u32> video_buffer;
    unsigned video_width = 400;
    unsigned video_height = 480;
    std::mutex buffer_mutex;
};
