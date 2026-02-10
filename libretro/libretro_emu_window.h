#pragma once

#include <memory>
#include "core/frontend/emu_window.h"
#include "video_core/renderer_base.h"

class LibretroEmuWindow : public Frontend::EmuWindow {
public:
    LibretroEmuWindow();
    ~LibretroEmuWindow() override;

    void PollEvents() override;

    void MakeCurrent() override;
    void DoneCurrent() override;

    std::unique_ptr<Frontend::GraphicsContext> CreateSharedContext() const override;

    void OnFramebufferSizeChanged();

    // Libretro specific
    void SetWindowInfo(void* surface, float scale);
    void SetLibretroVulkanContext(void* ctx) { vk_context = ctx; }
    void* GetLibretroVulkanContext() override { return vk_context; }

private:
    void* vk_context = nullptr;
};
