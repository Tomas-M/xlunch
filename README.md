# xlunch
Graphical app launcher for X, using pure Xlib and Imlib2.

Use the mouse wheel for smooth scrolling, or drag the scrollbar on the right.
Clicking the scrollbar track moves its thumb to the pointer. `--noscroll` disables scrolling.
Use `--nostretchrows` for fixed row gaps set by `--leastvmargin` (or `--leastmargin`).

Use `--windowed --width 800 --height 600 --center-window` to center a window on
the X screen. This overrides explicit coordinates; `--center` still centers icons.
Use `--class Pi-Apps` to set the WM_CLASS class independently of `--name`.

Focus adds an optional interface with an INI profile, category navigation, fixed favorites,
search, session actions and configurable colors, fonts, spacing and component visibility.
The default classic interface and existing configurations remain supported.

```sh
make xlunch
./xlunch --check-ui-config extra/focus.ini.example
./xlunch --ui-config extra/focus.ini.example
```

Adjust the example's favorite desktop IDs, logo, icons and session commands for your system.
Native desktop launching uses `gio launch` by default (Debian package `libglib2.0-bin`),
or the executable and argument list configured under `[desktop]`.
Use `--input entries.dsv` after the profile to display an existing DSV instead.
Bundled interface SVGs use librsvg/Cairo at runtime for sharp rendering at the requested size.
Their license is included in `extra/focus-icons`.
Clickable items use the system theme's hand cursor through libXcursor when
available, respecting its configured theme and size. Other areas inherit the
desktop cursor.
See [the profile](extra/focus.ini.example) and [configuration documentation](docs/focus-design.md).

Desktop catalogs load from `~/.cache/xlunch` by default (`$XDG_CACHE_HOME/xlunch` when set), including resolved icons and
uncompressed pixels at the configured sizes. After the first frame, a separate process
refreshes the snapshot at nice priority 19. A completed refresh updates the open menu
while preserving its query, category, scroll and selected desktop ID. Launching an
application or closing the menu cancels the refresh; incomplete files never replace
the previous snapshot. Missing or damaged snapshots fall back to scanning desktop files.

Create a snapshot during image preparation without a running X server:

```sh
xlunch --ui-config /etc/xlunch/focus.ini --rebuild-cache
```

Use the same HOME, locale, PATH, XDG environment and appearance settings as the target
session. Different contexts get separate snapshots. Runtime buttons do not affect the
cache key. This cache applies to `entries.source=desktop`; streamed DSV input stays live.

# website

detailed info at: http://www.xlunch.org/

# compile and test with:

    make test

on some distros `libimlib2-dev` and `libx11` are needed for compilation.

For Ubuntu and other Debian-based distros, do: 

``` sh
sudo apt install libimlib2-dev libx11-dev
```

For Fedora: 

```sh
sudo dnf install imlib2-devel libX11-devel
```

Package builders can use `make GENERATE_ENTRIES=0` and
`make install DESTDIR=/path/to/package GENERATE_ENTRIES=0` to skip generating
`entries.dsv` from the build host. Generate or supply the target system's entries
separately, or use a Focus profile with `entries.source=desktop`.

`genentries --theme` expects an **icon theme** from `/usr/share/icons` (or a user
icon directory), such as `Adwaita`, rather than a GTK theme such as `Adwaita-dark`
from `/usr/share/themes`. Diagnostics go to stderr so redirected DSV stays clean.

# screenshots

![Screenshot](/../Screenshot/screenshot.png?raw=true "Screenshot")

![Screenshot](/../Screenshot/screenshot2.png?raw=true "Screenshot")

Version 3 of xlunch added a lot of customization options, here are some of them in use:
![Screenshot](/../Screenshot/screenshot3.png?raw=true "Screenshot")
