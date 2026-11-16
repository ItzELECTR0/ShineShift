# Testing

## Core

```sh
zig build test
```

## Headless first

Run GUI checks in a Wardian session with fake displays and scratch folders, so nothing touches a real monitor or the real config.

```sh
wardian -s NAME run --name app -e SHINESHIFT_FAKE=2 -e XDG_CONFIG_HOME="$SCRATCH/cfg" -e XDG_STATE_HOME="$SCRATCH/state" -- build/shineshift --daemon
wardian -s NAME exec -- build/shineshift list
wardian -s NAME exec -- build/shineshift          # open the flyout
```

- The flyout and the overlay are layer surfaces, so `wardian windows` does not list them. Look at a screenshot.
- `shineshift drift 1 0x10 100` moves a fake monitor behind the app's back. `list` shows what the app believes, so to prove a value really went back to the monitor, switch Hold off, wait out the 10 second restore window and open the flyout, which reads it fresh.
- There is no tray host in the session. Exercise the item with `busctl --user call <name> /StatusNotifierItem org.kde.StatusNotifierItem Scroll is 120 vertical`, `Activate ii 0 0`, and the menu with `/MenuBar com.canonical.dbusmenu Event isvu 5 clicked s "" 0`. A negative argument needs `--` before the arguments.
- Wardian types faster than Qt focuses a field. Wait about 150 ms after clicking a field and use `type --delay 30`.
- `run --x11` exercises the X11 path through xwayland-satellite, where the flyout arrives as an ordinary window that sway tiles. Add `for_window [title="^ShineShift$"] floating enable` to see it. Placement there says nothing about a real X11 session.

## Real monitors and Hyprland

Reading is harmless. The Zig core can be probed from a small C program linked against `build/zig-out/lib/libshineshift_core.a`. To test the config opt-in, copy `~/.config/hypr` somewhere and point `SHINESHIFT_HYPR_CONFIG` at the copy's `hyprland.lua`.

Hyprland's sockets cannot be reached from inside a Wardian session: the session's runtime directory makes the socket path longer than a `sockaddr_un` allows.

Anything that changes a real monitor's brightness, sends `hl.monitor` to the live compositor, reloads its config or shows a window or tray icon on the live desktop needs the user's go-ahead for that run. What has and has not been confirmed live is in `ROADMAP.md`.
