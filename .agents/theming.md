# Theming

The look is the one SnipShot and GlyphPop use: a quiet monochrome surface with one accent, small radii, 1 px outlines and the system font. It must not imitate Windows or Fluent (the Fluent icon set is the one exception) and must not read any particular shell's or bar's config. Every colour, radius and font size in QML comes from `ui/Theme.qml`.

`app/themesource.*` and the controls in `ui/` (`Surface`, `IconButton`, `Toggle`, `Segmented`, `Dropdown`, `Field`, `Chip`, `Tip`, `Scroller`, `Spinner`, `ColorPicker`, `FluentIcon`, `WheelScroll`) are copies of SnipShot's. Keep them in step with it rather than letting them drift; `LevelSlider`, `MonitorCard`, `Flyout`, `LevelOverlay` and `SettingsWindow` are ShineShift's own.

## Settings

`ThemeSource` (the `Appearance` singleton) owns these `[General]` keys and writes them as soon as the settings window changes them.

- `theme`: `material` (default) or the name of `~/.config/ShineShift/themes/<name>.json`.
- `mode`: `system` (default), `light` or `dark`. System follows the portal's `color-scheme`; no preference means dark.
- `accent`: `default`, `system`, `orange`, `red`, `pink`, `purple`, `blue`, `teal`, `green`, `yellow`, `custom`.
- `customAccent`: `#rrggbb`.

## Material and theme files

The built-in theme is Prismora's Material: surface and text `#121212`/`#e6e6e6` dark, `#f4f4f4`/`#161616` light, primary white or black. Containers, outlines and muted text are mixes of surface and text, computed by `material()` in `themesource.cpp`.

```json
{ "base": "dark", "radius": 10, "colors": { "surface": "#101820", "on_surface": "#dfe8f0", "primary": "#8ecfd4" },
  "light": { "surface": "#f0f4f8" } }
```

Everything is optional. Token names are Material roles: `surface`, `surface_container_low`, `surface_container`, `surface_container_high`, `surface_container_highest`, `on_surface`, `on_surface_variant`, `outline`, `outline_variant`, `primary`, `on_primary`, `error`, `shadow`. `data/themes/Midnight.json` is the shipped example.

## Controls

The Quick Controls style is `Basic`, set once in `main.cpp`. Anything from Quick Controls is wrapped in a component that replaces its background, content and indicator from Theme. Use those rather than a bare Quick Controls type.

`LevelSlider` binds the backend value to its handle only while the handle is not held, and reports drags and wheel notches through one `moved` signal.

`LevelOverlay` is not called `Overlay` because Quick Controls has an attached type of that name.

Scroll views take no mouse buttons (`acceptedButtons: Qt.NoButton`), so a mouse drag never scrolls them; touch and touchpads still do. `WheelScroll` moves `wheelScrollLines` times 24 px per wheel notch, because Flickable turns each wheel event into a speed-capped flick that crawls, worst with high-resolution wheels. It is a MouseArea under the content with `scrollGestureEnabled: false`, so controls get the wheel first and touchpad scrolls (synthesized events) fall through to Flickable's own kinetic scrolling. A WheelHandler cannot do this: on Wayland, wheel events carry Qt's touchpad device whenever the compositor offers pointer gestures, and the handler blocks before `onWheel` runs.

## Icons

`ui/icons/Icons.js` holds SVG path data from Microsoft's Fluent UI System Icons (MIT, licence beside it), rendered by `FluentIcon` with `QtQuick.Shapes`. To add one, download `ic_fluent_<name>_24_regular.svg` and append its `d` attributes to `paths` under `<name>_regular`. Only icons that are used are kept.
