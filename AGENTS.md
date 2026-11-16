# ShineShift agent guide

ShineShift is a monitor brightness tray app for Linux, modelled on Twinkle Tray. It talks DDC/CI to monitors through libddcutil and, on Hyprland, sets SDR brightness for HDR outputs. The UI is Qt Quick/QML, the Qt host is C++ in `app/`, and DDC, compositor IPC and the pure logic live in Zig in `src/`.

- Every DDC call goes through `DdcWorker` on its own thread. Never call `shine_get`, `shine_set`, `shine_detect` or `shine_capabilities` from the GUI thread.
- Never send Hyprland an `hl.monitor` call for an output whose rule selector is unknown. It replaces the user's whole monitor rule. See `.agents/hyprland.md`.
- The look is shared with SnipShot and GlyphPop, not Fluent or Windows styling. See `.agents/theming.md`.
- Do not claim desktop support beyond what has been exercised. `ROADMAP.md` lists what is still unconfirmed.
- Zig 0.17.1 is the minimum and 0.18-dev builds too. 0.18 has no `@cImport`, so C libraries are declared by hand.
- Build with CMake (see README). Core tests are `zig build test`.

## Topics

- `.agents/architecture.md`: read before changing the process model, the threads, the Zig/C++/QML split, the CLI or where settings and state are stored.
- `.agents/ddc.md`: read before touching libddcutil, the worker queue, remembered brightness, Hold or the fake displays.
- `.agents/hyprland.md`: read before touching SDR brightness, the Hyprland sockets, config scanning or the `shineshift.lua` opt-in.
- `.agents/shell.md`: read before touching the tray item, the flyout, the overlay or window placement.
- `.agents/theming.md`: read before changing colours, controls, icons or the theme file format.
- `.agents/testing.md`: read before running the app, especially anything that writes to a real monitor.
- `.agents/packaging.md`: read before changing anything in `packaging/` or `.github/workflows/`, or cutting a release.
