# ShineShift

ShineShift is a monitor brightness tray app for Linux, modelled on Twinkle Tray.
It's written in Zig with a Qt Quick interface and talks DDC/CI to your monitors, so it doesn't care much which desktop you run (Hyprland is the one I use and test on).

## LICENSE

[GPL-3.0-or-later](LICENSE)

## What can it do?

| What? | How? |
| --- | --- |
| Brightness | A slider per monitor in a flyout from the tray, linked levels, and a range per monitor so mismatched screens line up |
| More DDC/CI | Contrast, volume, input and power, plus any VCP code you add yourself |
| SDR brightness | On Hyprland, how bright SDR content is on a monitor that's in HDR |
| Remembering | Puts your brightness back at login, when a monitor connects, after sleep and when HDR switches. Hold keeps it there all the time |
| Commands | Everything is a command, so your desktop's keybinds are the shortcuts |
| Themes | Material light or dark with an accent colour, or your own JSON theme |

## Usage

ShineShift keeps a daemon running, so commands from a keybind are instant.
Start it at login, for example in Hyprland:

```lua
hl.on("hyprland.start", function ()
    hl.exec_cmd("shineshift --daemon")
end)
```

Click the tray icon for the sliders, scroll on it to change brightness, right click it for the rest.

```sh
shineshift                    # open or close the flyout
shineshift list               # displays and their levels
shineshift set DP-1 50        # brightness in percent
shineshift offset all -10     # change it by an amount
shineshift contrast 1 60      # +5 or -5 works too
shineshift volume 1 20
shineshift sdr DP-1 1.3       # SDR brightness, 1.0 is the default
shineshift input 2 hdmi1
shineshift set DP-1 0,50,100  # steps on from wherever the monitor is now
shineshift power 2 on,standby # or off, some monitors only wake from standby
shineshift hold DP-1 on
shineshift vcp 1 0x14 5       # any raw DDC/CI value
shineshift off                # turn off all displays
shineshift settings           # open the settings window
shineshift quit               # close the flyout and kill the daemon
```

A display is its number or connector from `shineshift list`, its name, or `all`. Add `--quiet` to skip the little level overlay.

Bind keys in your compositor, for example:

```lua
hl.bind("XF86MonBrightnessUp", hl.dsp.exec_cmd("shineshift offset all +10"))
hl.bind("XF86MonBrightnessDown", hl.dsp.exec_cmd("shineshift offset all -10"))
```

### Remembering your brightness

Some monitors jump back to full brightness when HDR turns on (mine does), or forget their level after standby. ShineShift writes your level back at the moments that usually cause it.
If your monitor resets itself at other times, switch Hold to "on" for it in settings and ShineShift keeps checking and correcting it.
With Hold "off", a change you make with the monitor's own buttons is simply picked up as the new level.

### SDR brightness on Hyprland

The slider shows up for monitors that are in HDR. It needs the Lua config and a `hl.monitor` rule that names the output, like `output = "DP-1"` or `output = "desc:..."`.
ShineShift only adds to a rule that's already there, because anything else would replace your monitor settings.

By default nothing in your Hyprland config is touched. ShineShift sets the value while it runs and sets it again after a reload.
If you'd rather have it in the config, switch that on in settings. It adds `require("shineshift")` to the end of your `hyprland.lua` and a `shineshift.lua` next to it.

### Monitors that don't show up

DDC/CI has to be switched on in the monitor's own menu, and you need access to `/dev/i2c-*`.
The `ddcutil` package sets that up for the logged-in user on most distros. `ddcutil detect` tells you what it can see.

### Themes

Custom themes are JSON files in `~/.config/ShineShift/themes/`. Anything you leave out comes from Material. There's an example in `data/themes/Midnight.json`.

## Prerequisites

- Zig 0.17 or higher
- Qt 6.8 or newer with Quick, Quick Controls, QML and D-Bus
- LayerShellQt
- ddcutil 2.2.1 or newer, for libddcutil
- CMake, Ninja and a C++20 compiler

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/shineshift
```

## Install on Arch

```sh
cd packaging
makepkg -si
```

The package is built from your checkout as it is, uncommitted changes included.

## Test

```sh
zig build test
```
