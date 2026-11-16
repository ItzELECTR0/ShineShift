# Hyprland

SDR brightness is the only compositor-specific feature. Everything here was read from Hyprland's source at commit `2052962` and the parts marked confirmed were also run against that build.

## How the value is set

`hl.monitor({ output = "<selector>", sdrbrightness = <n> })`, sent as `/eval ...` on `.socket.sock`.

- `hl.monitor` looks for an existing rule whose `output` string is exactly equal, copies it and applies only the given fields (`hlMonitor` in `LuaBindingsConfigRules.cpp`). Confirmed: only `sdrBrightness` changed.
- `sdrbrightness` is a soft property (`CMonitorRule::compare`), so there is no modeset. Confirmed.
- If no rule has that exact string, the call creates a new rule with defaults for everything else, and because rules are searched newest first it replaces the user's mode, position, colour settings and the rest. A rule written as `desc:...` or a catch-all `output = ""` does not count as a match for `DP-1`.

Hyprland has no way to ask which rule an output uses. So `compositor.zig` reads the user's config: the main file plus everything it `require`s, comments stripped, looking for `output = "<literal>"` that equals the output name or is a `desc:` prefix of its description. That literal is the selector. No literal, no SDR control: the flyout says so and nothing is sent. Selectors built from variables are not found, which errs on the safe side.

The scan is cached and redone only at start and after a reload (`shine_outputs(..., rescan_config)`).

The config path is `$XDG_CONFIG_HOME/hypr/hyprland.lua`, or `SHINESHIFT_HYPR_CONFIG` when set. The old hyprlang `hyprland.conf` is not supported: there a `monitor` keyword replaces the whole rule.

## Reloads

A reload clears every monitor rule, so a value set at runtime is gone. Hyprland posts `configreloaded` on `.socket2.sock`; `Displays` then reapplies the remembered SDR value and restores brightness. Until that lands, the value from the user's config shows for a moment.

`refreshOutputs(false)` is the 3 second poll. It adopts an SDR value changed by someone else, and it backs off when the event socket is readable so a reload is never mistaken for an outside change.

## The config opt-in

Off by default (the user's choice). When switched on in settings:

1. `shineshift.lua` is written next to `hyprland.lua` (temp file, then rename).
2. `require("shineshift")` is appended to the real file behind `hyprland.lua` with one `O_APPEND` write, so a symlinked dotfile stays a symlink and nothing else in it is touched.

Switching it off writes a `shineshift.lua` with only its header. The `require` line stays, because removing it would mean rewriting the user's file, and the module must keep existing or the config errors.

Going by the source, this build tracks `require`d files for autoreload, so each write to `shineshift.lua` should cause a reload. That has not been confirmed on a running compositor. Writes are debounced 1.5 s and skipped when the content is unchanged.

## HDR state

`monitors -j` reports `colorManagementPreset`; `hdr` and `hdredid` count as HDR. There is no event for a change, which is why it is polled.

## Other calls

- Screens off: `/dispatch hl.dsp.dpms({ action = "off" })`.
- Floating the settings window: an `/eval` that floats, resizes and centres the window by title, so `ShineShift Settings` is part of the contract.
