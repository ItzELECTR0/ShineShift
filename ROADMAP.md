# Roadmap

## Confirm on live Hyprland

- The tray icon in a real tray host: left click, scroll, the right click menu, and whether the host passes usable click coordinates.
- The flyout and overlay as layer surfaces on Hyprland, including on the rotated output.
- Brightness written by the running app to real monitors, and restored after a reboot when HDR comes on.
- SDR brightness from the slider, and reapplying it after a config reload.
- Whether an HDR change inside a session shows up in `colorManagementPreset`, which is what the restore relies on.
- The config opt-in against the real `hyprland.lua`, and that Hyprland reloads when `shineshift.lua` is rewritten.
- Screens off through `hl.dsp.dpms`.
- Touchpad scrolling in the flyout and settings still being kinetic.

## Twinkle Tray parity

- Time of day adjustments, with sun position and animated transitions.
- Idle dimming, with fullscreen and media as blockers.
- Profiles, including ones triggered by the focused app.
- Light sensor, through iio-sensor-proxy.
- Laptop panels through the kernel backlight.
- Dimming below a monitor's minimum in software.
- A different VCP code for brightness on monitors that don't use 0x10.
- List type VCP codes, such as colour presets, as a dropdown instead of a slider.
- Opening the flyout at the tray icon instead of a fixed corner.
- Translations.

## Desktops

- SDR brightness on KDE, GNOME and sway. None of their interfaces are verified yet.
- Hyprland with the old `hyprland.conf`.
- A real X11 session. Only X11 through xwayland-satellite has been run, where placement means nothing.
- GNOME, which has no layer-shell.
- In-app shortcuts through the GlobalShortcuts portal.

## Rough edges

- SDR brightness is refused when the output's `hl.monitor` rule builds its selector from a variable, because the config is read as text.
- A command aimed at several displays shows one number in the overlay, the first display's.
- Switching the config opt-in off leaves the `require` line in place.
