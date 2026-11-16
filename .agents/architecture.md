# Architecture

## Process model

One resident instance owns the session bus name `net.electris.ShineShift` (object `/net/electris/ShineShift`, method `Activate(as) -> s`). Any later `shineshift ...` fails to register the name, forwards its arguments, prints the reply and exits. A reply starting with `error: ` goes to stderr with exit code 1. `Displays::handleCommand` is the single entry point for both paths. A cold start with a command becomes the daemon and runs the command after the first detect, so `shineshift set all 40` works as an autostart line. The app never quits when windows close; `shineshift quit` ends it.

Commands that need monitors are queued until the first detect finishes. Their output then leaves through `Displays::output`, which only the first process prints.

## Layers

- `src/logic.zig`: pure functions with the tests. Brightness range mapping, capability string parsing, Lua comment stripping, rule selector and `require` detection, event names.
- `src/ddc.zig`: libddcutil bindings and the fake displays. See `ddc.md`.
- `src/compositor.zig`: Hyprland sockets, config scanning and the `shineshift.lua` opt-in, the kernel hotplug socket, screens off and window floating. See `hyprland.md`.
- `src/core.zig`: the C ABI in `src/core.h`. Every call is blocking and returns small text or plain numbers.
- `app/worker.*`: `DdcWorker`, the only caller of the DDC functions.
- `app/displays.*`: `Monitor` (one per display, what QML binds to) and `Displays` (the controller, registered as the `Displays` singleton in `ShineShift.Core`). Owns remembered values, Hold, SDR, the CLI and the overlay state.
- `app/shell.*`, `app/tray.*`: windows and the tray item. See `shell.md`.
- `app/settings.*`: a `QQmlPropertyMap` over QSettings, registered as `Config`. Defaults live in its table; QML writes persist immediately.
- `app/themesource.*`: the `Appearance` singleton. See `theming.md`.
- `ui/App.qml`: a `QtObject` holding every window as a property. C++ finds `flyout` and `overlay` by property name.

## Threads and descriptors

The GUI thread never blocks on a monitor. `DdcWorker` lives on its own `QThread` and drains one queue; results come back as queued signals carrying a generation number, and anything from before the latest detect is dropped because display numbers change with a detect.

Hyprland requests are made from the GUI thread. They answer in a few milliseconds and have a 3 second receive timeout.

Zig opens the Hyprland event socket and the kernel uevent socket and hands back plain descriptors. The host watches them with `QSocketNotifier` and calls `shine_events_read` or `shine_hotplug_read` when they are readable, so Zig needs no threads of its own.

## Settings and state

- `~/.config/ShineShift/ShineShift.conf`: preferences. `[General]` belongs to `Settings` and `ThemeSource`; `[monitors]` holds per-monitor keys as `<id>\name`, `hidden`, `hold`, `minimum`, `maximum`, `order`, `contrast`, `volume`, `input`, `power`, `features`.
- `~/.local/state/ShineShift/state.conf`: remembered values, `<id>/brightness` (raw monitor units) and `<id>/sdr`. Kept apart because it changes on every slider move.

A monitor id is `<mfg>-<model>-<serial>` from the EDID, so settings follow the monitor across ports. Two identical monitors without serials get `-2`, `-3` suffixes in detection order.

## Values

Sliders work in percent. `Monitor::brightness` maps the raw value through the monitor's reported maximum and then through its `minimum`/`maximum` range (`shine_to_percent`, `shine_to_raw`). Remembered brightness is stored raw so changing the range never rewrites it.
