# lunch
Graphical app launcher for X with little dependencies

Current version has the following limitations:
- background wallpaper is used from X's root window. If you do not have any image set, it fails to run.
- font is hardcoded to DejaVuSans
- all parameters (margin, padding, positions) are hardcoded for now. My plan is to let user specify them on command line
- config file must be generated manually by running ./genconf > conf
- the app is currently looking for config file only in current directory where the executable is

![Screenshot](/../Screenshot/screenshot.png?raw=true "Screenshot")

![Screenshot](/../Screenshot/screenshot2.png?raw=true "Screenshot")
