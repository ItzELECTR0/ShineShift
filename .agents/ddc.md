# DDC/CI

## libddcutil

Linked directly (2.1 or newer, for `ddca_get_display_info2` and its DRM connector name). Zig 0.18-dev has no `@cImport`, so `src/ddc.zig` declares the handful of functions and `DDCA_Display_Info2` by hand. A test pins the struct's size and offsets on x86_64; re-measure with a C `offsetof` program if libddcutil changes it.

`ddca_init2` runs with `DDCA_INIT_OPTIONS_DISABLE_CONFIG_FILE`, so a user's `ddcutilrc` cannot change our behaviour, and verification is off (`ddca_enable_verify(false)`) because we read back ourselves where it matters.

Measured on a GS27QC and an EK240Y: init with detection about 960 ms, a read 105 to 150 ms, a write about 72 ms. The `ddcutil` CLI costs 300 to 750 ms per call, which is why it is not used.

Display handles stay open for the life of the process. A detect closes them all and reopens what answers. `DRM connector` (`card1-DP-1`) is trimmed to `DP-1`, which is how monitors are matched to compositor outputs.

## The worker queue

`DdcWorker::postWrite` replaces a queued write to the same display and feature, so a slider drag sends only the newest value once the monitor is free. Writes run before reads. `postDetect` clears the queue.

`Monitor::m_pending` holds the last value posted per feature until `written` reports that same value. While a feature is pending, reads of it are neither posted nor accepted, otherwise a read that started before a write would snap the slider back and overwrite the remembered value.

## Remembered brightness

The user chose: restore at known moments by default, with Hold as an opt-in per monitor.

- Known moments: daemon start, a detect after a hotplug or resume, a Hyprland config reload, and an HDR on/off flip seen by the output poll. `Displays::reapply` writes the remembered value, then reads it back after 3 and 8 seconds. For 10 seconds (`EnforceWindow`) a read that differs is written back again, because monitors reset their backlight a moment after a mode switch.
- Outside that window, with Hold off, a read that differs is adopted as the new remembered value. Reads happen when the flyout opens.
- With Hold on, the monitor is read every `holdInterval` seconds and a differing value is always written back.
- `reapply` off in settings skips the restore for monitors without Hold.

Hotplug comes from the kernel uevent socket (`SUBSYSTEM=drm`), debounced 2.5 s, so it does not depend on the compositor. Resume comes from logind's `PrepareForSleep(false)`, 4 s later.

Only brightness is remembered and restored. Contrast, volume, input and power are read when shown and written when changed.

## Power

`Monitor::setPowered(false)` sends VCP 0xD6: standby (2) or off (4), falling back to 4 or 5 when the capabilities do not list the first choice. Many monitors stop answering DDC after an off signal and cannot be woken by one.

## Fake displays

`SHINESHIFT_FAKE=<n>` replaces libddcutil with `n` in-memory monitors (brightness, contrast, volume, input, power) and a fake compositor where display 1 is in HDR. Writes sleep 40 ms to behave like a real bus. `shineshift drift <display> <code> <value>` changes a fake monitor behind the app's back, to test restore and Hold. It only exists in fake mode.
