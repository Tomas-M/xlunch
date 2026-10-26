# Focus

Focus is an optional INI-configured layout. The classic layout, existing CLI
options and DSV files remain supported.

## Start

```sh
make xlunch
./xlunch --check-ui-config extra/focus.ini.example
./xlunch --ui-config extra/focus.ini.example
```

Copy [the example profile](../extra/focus.ini.example) and edit it for your system.
Use `--set section.key=value` after the profile for a command-line override.
Native desktop entries use `gio launch` by default; use `--input entries.dsv`
after the profile to display a legacy list instead.

## Configure

The example contains the available component settings: header, sidebar, search,
heading, grid, footer, background, scrollbar, fonts and interaction colors.
Use `visible=false` to hide a component. Colors are `RRGGBB` or `RRGGBBAA`, sizes
are pixels multiplied by `ui.scale`, and fonts use names such as `DejaVuSans/12`.
Icon values can be theme names or file paths, relative to the profile.
Set `footer.align=grid` to place selection details and shortcuts under the
application area, leaving the space under the sidebar available for image buttons.
`footer.bottom_margin` overrides the shared window margin at the bottom. Setting
it to `0` places the footer against the bottom edge and gives the grid the freed
height. When omitted, it follows `ui.margin` plus `ui.padding`.
`grid.fade_height` fades tiles, labels and highlights into the background at the
top and bottom of the grid while content extends beyond either edge. The height
is in logical pixels; `0` disables it. Each fade stays at its viewport edge during
scrolling and shrinks away as the first or final row becomes fully visible.
Sidebar, footer and scrollbar keep their normal appearance.

Installer launchers can declare their target program with
`X-Xlunch-Executable=program` in the desktop entry, using a command name or an
absolute path. `grid.uninstalled_icon_opacity` sets the opacity of icons
with a missing executable, from `0` (invisible) to `100` (opaque), default `50`.
Hovering restores the full icon. Labels and footer descriptions stay unchanged.
Availability is checked when rendering, including with cached catalogs, so
installing or removing the program updates its indication on redraw. Transparency
is applied while compositing; cached icon pixels retain their original alpha.
`grid.install_icon` optionally adds an indicator in the upper-right corner of a
missing application's tile. `grid.install_icon_size` defaults to `24` and
`grid.install_icon_padding` to `12` logical pixels, independently of grid padding.
It appears on hover or keyboard focus by default; set
`grid.install_icon_hover_only=false` to show it on every missing application.
`grid.install_icon_opacity` controls the indicator outside hover or keyboard
focus, from `0` (invisible) to `100` (opaque, the default). Hover or keyboard focus
always restores full opacity; application icon opacity remains independent.
Set `grid.uninstalled_icon_opacity=100` to keep application icons fully opaque.
`grid.installed_first=true` places available applications before missing ones,
preserving the original order within each group, including Favorites. Its default
is `false`. Ordering follows executable availability, so hovering never moves a tile.

Groups match `.desktop` categories through `[group.<id>] categories=Network`.
Favorites are an ordered list of desktop IDs in `[favorites] ids=...`.
With `search.scope=all`, search includes applications from any category, even
while Session is open; matching Session actions remain available there too.

Session is disabled in the example. Define `[action.<id>]` sections with `label`,
`icon` and `command`, then enable `[session] visible=true` and list their IDs in
`items`. `view=grid` uses the same tiles as applications; `view=menu` uses a popup.
Icons and commands are supplied by the user's configuration.

## Commands

`search.allow_commands=true` enables commands prefixed by `search.command_prefix`
(default `>`). An empty prefix recognizes an executable as the first word: an
application with the same executable is launched through its icon's command;
otherwise the entered command runs. Unknown executables remain ordinary searches.

An optional `search.command_launcher` receives the command and resolved executable
path as separate arguments; `command_launcher_args` supplies a comma-separated
argument list before them. This lets distributions provide terminal or GUI routing
without embedding it in xlunch. `--outputonly` returns selections without launching
native entries, custom actions or typed commands.
