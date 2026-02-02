#include "libretro_emu_window.h"

LibretroEmuWindow::LibretroEmuWindow() {
    // Initialize window
}

LibretroEmuWindow::~LibretroEmuWindow() = default;

void LibretroEmuWindow::PollEvents() {}
void LibretroEmuWindow::DoneCurrent() {}
void LibretroEmuWindow::MakeCurrent() {}

std::unique_ptr<Frontend::GraphicsContext> LibretroEmuWindow::CreateSharedContext() const {
    return nullptr;
}

void LibretroEmuWindow::SetFramebuffer(void* data, unsigned width, unsigned height) {
    framebuffer_data = data;
    fb_width = width;
    fb_height = height;
}

void LibretroEmuWindow::Present() {
    // This will be called by the renderer when it's time to swap buffers
}
