void retro_run(void) {
    input_poll_cb();

    auto& system = Core::System::GetInstance();

    s64 starting_ticks = system.CoreTiming().GetGlobalTicks();

    if (emu_window) {
        emu_window->ResetFrameDone();

        // Run until GPU triggers VBlank (SwapBuffers)
        while (!emu_window->IsFrameDone()) {
            system.RunLoop();
        }
    } else {
        system.RunLoop();
    }

    s64 ending_ticks = system.CoreTiming().GetGlobalTicks();
    u64 ticks_passed = (u64)(ending_ticks - starting_ticks);

    // Audio Integration: Use emulated cycles to determine how many samples to submit to Libretro.
    // The DSP is ticked automatically via CoreTiming events during system.RunLoop().
    auto& dsp = system.DSP();
    static_cast<AudioCore::LibretroSink&>(dsp.GetSink()).Flush(ticks_passed);

    if (emu_window) {
        video_cb(emu_window->GetFramebuffer(), emu_window->GetWidth(), emu_window->GetHeight(), emu_window->GetWidth() * sizeof(u32));
    }
}
