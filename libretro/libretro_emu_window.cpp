#include "libretro_emu_window.h"
#include "common/settings.h"
#include "core/frontend/framebuffer_layout.h"

LibretroEmuWindow::LibretroEmuWindow() : Frontend::EmuWindow() {
    // Force a default stacked layout
    Settings::values.layout_option.SetValue(Settings::LayoutOption::Default);
    Settings::values.swap_screen.SetValue(false);
    Settings::values.upright_screen.SetValue(false);

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
    // 400x480 is the base resolution for stacked 3DS screens.
    // Width = 400 (Top screen width)
    // Height = 240 (Top) + 240 (Bottom) = 480
    UpdateCurrentFramebufferLayout(400, 480);
}

void LibretroEmuWindow::SetWindowInfo(void* surface, float scale) {
    window_info.render_surface = surface;
    window_info.render_surface_scale = scale;
    window_info.type = Frontend::WindowSystemType::Headless; // We'll handle Vulkan manually
}
