# xlunch
Graphical app launcher for X, using pure Xlib and Imlib2.

Use the mouse wheel for smooth scrolling, or drag the scrollbar on the right.
Clicking the scrollbar track moves its thumb to the pointer. `--noscroll` disables scrolling.
Use `--nostretchrows` for fixed row gaps set by `--leastvmargin` (or `--leastmargin`).

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
See [the profile](extra/focus.ini.example) and [configuration documentation](docs/focus-design.md).

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

# screenshots

![Screenshot](/../Screenshot/screenshot.png?raw=true "Screenshot")

![Screenshot](/../Screenshot/screenshot2.png?raw=true "Screenshot")

Version 3 of xlunch added a lot of customization options, here are some of them in use:
![Screenshot](/../Screenshot/screenshot3.png?raw=true "Screenshot")
