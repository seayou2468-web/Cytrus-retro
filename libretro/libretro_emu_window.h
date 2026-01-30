#pragma once

#include "core/frontend/emu_window.h"
#include "video_core/renderer_base.h"

class LibretroEmuWindow : public Frontend::EmuWindow {
public:
    LibretroEmuWindow();
    ~LibretroEmuWindow();

    void PollEvents() override;
    void DoneCurrent() override;
    void MakeCurrent() override;

    std::unique_ptr<Frontend::GraphicsContext> CreateSharedContext() const override;

    void SetFramebuffer(void* data, unsigned width, unsigned height);
    void Present();

private:
    void* framebuffer_data = nullptr;
    unsigned fb_width = 0;
    unsigned fb_height = 0;
};
