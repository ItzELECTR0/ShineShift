# Tray and windows

## Tray item

`app/tray.*` implements `org.kde.StatusNotifierItem` and a six entry `com.canonical.dbusmenu` directly on QtDBus. `QSystemTrayIcon` was not used because it never delivers scroll events, and scrolling on the icon is a feature.

- The item registers `org.kde.StatusNotifierItem-<pid>-1` and announces itself to `org.kde.StatusNotifierWatcher`, again every time a watcher appears, because shells get restarted.
- `Scroll` deltas below 15 are taken as whole steps, larger ones as wheel units of 120 per notch. Hosts differ.
- `IconName` is `display-brightness-symbolic` when the icon theme has it; `IconPixmap` always carries a drawn sun in the theme's text colour.
- `Activate` toggles the flyout. The click coordinates are ignored for now.
- The `tray` setting (on by default) claims or releases the bus name at runtime. Hosts drop an item when its name goes away, so releasing it is how the icon hides. The objects stay registered.

## Flyout

On Wayland the flyout is a layer-shell surface on the overlay layer, anchored to all four edges with exclusive zone 0, so it covers the usable area of the active output and a click beside the card closes it. The card sits in the corner from `flyoutCorner`. Keyboard interactivity is exclusive while open, for Escape.

Clicks on other outputs never reach the flyout's surface, so while it is open each other output gets a catcher: a transparent overlay layer surface with no keyboard that closes the flyout on any press (queued, since closing destroys the catcher). The compositor picks the flyout's output, so its `QWindow::screen()` can be stale until the surface enters an output; `screenChanged` rebuilds the catchers, and they are mapped before the flyout so a wrong guess stacks below it.

Closing drops keyboard interactivity to none and hides 50 ms later. Unmapping a surface that holds exclusive keyboard focus leaves sway's pointer stuck on it (learned in SnipShot).

On X11 the window is the size of the card, a frameless tool window placed in the chosen corner of the screen under the cursor, and it closes when it loses focus.

`Displays::layered` (set from the platform name before QML loads) tells QML which of the two it is.

## Overlay

A small layer surface with no keyboard and no input region, anchored top or bottom. A mapped layer surface keeps its anchor, so changing the position hides it first. Commands and tray scrolling show it; it never shows while the flyout is open.

## Settings window

A normal toplevel. Hyprland and sway float a toplevel whose minimum and maximum sizes match when it maps, so the window pins both to 680x780 until its first frame has been swapped, then lifts them. As a fallback it is also floated by title 80 ms after it shows and again 300 ms later. The Hyprland call must say `action = 'enable'`: anything but `enable`, `disable` or `toggle` parses as a toggle, which made the second call tile it again.
