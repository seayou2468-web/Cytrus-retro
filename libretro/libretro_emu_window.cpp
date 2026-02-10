#include "libretro_emu_window.h"
#include "common/settings.h"
#include "core/frontend/framebuffer_layout.h"

LibretroEmuWindow::LibretroEmuWindow() : Frontend::EmuWindow() {
    OnFramebufferSizeChanged();
}

LibretroEmuWindow::~LibretroEmuWindow() = default;

void LibretroEmuWindow::PollEvents() {
}

void LibretroEmuWindow::MakeCurrent() {
}

void LibretroEmuWindow::DoneCurrent() {
}

std::unique_ptr<Frontend::GraphicsContext> LibretroEmuWindow::CreateSharedContext() const {
    return nullptr;
}

void LibretroEmuWindow::OnFramebufferSizeChanged() {
    u32 base_w = 400;
    u32 base_h = 240;
    u32 factor = Settings::values.resolution_factor.GetValue();

    switch (Settings::values.layout_option.GetValue()) {
    case Settings::LayoutOption::Default:
        base_w = 400;
        base_h = 480;
        break;
    case Settings::LayoutOption::SideScreen:
        base_w = 720;
        base_h = 240;
        break;
    case Settings::LayoutOption::SingleScreen:
        if (Settings::values.swap_screen.GetValue()) {
            base_w = 320;
            base_h = 240;
        } else {
            base_w = 400;
            base_h = 240;
        }
        break;
    default:
        base_w = 400;
        base_h = 480;
        break;
    }

    UpdateCurrentFramebufferLayout(base_w * factor, base_h * factor);
}

void LibretroEmuWindow::SetWindowInfo(void* surface, float scale) {
    window_info.render_surface = surface;
    window_info.render_surface_scale = scale;
    window_info.type = Frontend::WindowSystemType::Headless; // We'll handle Vulkan manually
}
