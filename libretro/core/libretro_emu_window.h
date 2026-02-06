#pragma once

#include "core/frontend/emu_window.h"

namespace Frontend {

class LibretroEmuWindow : public EmuWindow {
public:
    explicit LibretroEmuWindow(bool is_secondary = false);
    ~LibretroEmuWindow() override;

    void PollEvents() override;
    void SwapBuffers() override;
    void MakeCurrent() override;
    void DoneCurrent() override;

    // Buffer for RetroArch to read from
    const u32* GetFramebuffer() const { return framebuffer.data(); }
    u32 GetWidth() const { return GetFramebufferLayout().width; }
    u32 GetHeight() const { return GetFramebufferLayout().height; }

private:
    std::vector<u32> framebuffer;
};

} // namespace Frontend
