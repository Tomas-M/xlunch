# lunch
Graphical app launcher for X with little dependencies.
Should require only Imlib2

Commandline options:

    -r         use root window's background image
               Fails if your root window has no image set
    -g [file]  Image to set as background (jpg/png)
    -m [i]     margin (integer) specifies margin in pixels between icons
    -p [i]     padding (integer) specifies padding inside icons in pixels
    -b [i]     border (integer) specifies spacing border size in pixels
    -i [i]     icon size (integer) in pixels
    -c [file]  path to config file which describes titles, icons and commands.
    -f         Disable fullscreen
    -t         Top position (integer) in pixels for the Run commandline
    -x [text]  String to display instead of 'Run: '


    Format of config file (default is /etc/lunch/icons.conf) is:
    title;icon_path;cmd
    title;icon_path;cmd
    title;icon_path;cmd
    title;icon_path;cmd


Current version has the following limitations:
- font is hardcoded to DejaVuSans
- config file must be generated manually or by running ./genconf > /etc/lunch/icons.conf

![Screenshot](/../Screenshot/screenshot.png?raw=true "Screenshot")

![Screenshot](/../Screenshot/screenshot2.png?raw=true "Screenshot")

TODO:
add here examples how to configure lunch to start on Alt+F2 on openbox, i3wm, etc
