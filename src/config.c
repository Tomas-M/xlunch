#include "ui.h"

void parse_button(char *button_spec) {
    int parsing = 0; // section currently being read
    int position = 0; // position in the current entry
    int i = 0;
    button_t *new_button = malloc(sizeof(button_t));
    char b = button_spec[0];
    char x[256];
    char y[256];
    while (b != '\0') {
        if ((b == ';' && parsing != 4) || (parsing == 2 && b == ',') ) {
            b = '\0';
        }
        switch(parsing){
            case 0:
                new_button->icon_normal[position] = b;
                break;
            case 1:
                new_button->icon_highlight[position] = b;
                break;
            case 2:
                x[position] = b;
                break;
            case 3:
                y[position] = b;
                break;
            case 4:
                new_button->cmd[position] = b;
                break;
        }
        position ++;
        if(b == '\0') {
            position = 0;
            parsing++;
        }
        int maxlen = (parsing == 4 ? 511 : 255);
        if(position == maxlen){
            fprintf(stderr, "Entry too long, maximum length is %d characters!\n", maxlen);
            break;
        }
        i++;
        b = button_spec[i];
    }
    if (x[0] == '-') {
        new_button->x = atoi(x)-1;
    } else {
        new_button->x = atoi(x);
    }
    if (y[0] == '-') {
        new_button->y = atoi(y)-1;
    } else {
        new_button->y = atoi(y);
    }
    imlib_context_set_image(imlib_load_image(new_button->icon_normal));
    new_button->w = imlib_image_get_width();
    new_button->h = imlib_image_get_height();
    imlib_free_image();
    new_button->cmd[position] = '\0';
    new_button->next = buttons;
    buttons = new_button;
}

void parse_config(FILE *input) {
    int readstatus;
    int position = 0;
    int size = 0;
    int eol = 0;
    int fileline = 1;
    char *optarg = NULL;
    char matching[(option_count) - 1];
    char *entries_word = "entries";
    int matching_entries = 1;
    int matched = '?';
    int comment = 0;
    memset(matching, 1, option_count - 1);

    struct pollfd fds;
    fds.fd = fileno(input);
    fds.events = POLLIN;
    node_t * current_entry;
    while(poll(&fds, 1, 0)) {
        char b;
        readstatus = read(fds.fd, &b, 1);
        if(readstatus <= 0){
            break;
        }
        if((b == ':' && optarg == NULL) || b == '\n') {
            if (b == '\n') eol = 1;
            b = '\0';
        }
        if(b == '#' && optarg == NULL && position == 0) comment = 1;
        if(comment == 1 && eol != 1) continue;
        if(b == ' ' && position == 0) continue;
        if(optarg == NULL) {
            for(int i = 0; i < option_count - 1; i++) {
                if (long_options[i].name[position] != b){
                    matching[i] = 0;
                }
            }
            if (entries_word[position] != b && eol != 1) {
                matching_entries = 0;
            }
            if(b == '\0') {
                for(int i = 0; i < option_count - 1; i++) {
                    if (matching[i] == 1) {
                        optarg = malloc(1);
                        position = -1;
                        matched = long_options[i].val;
                    }
                }
            }
        } else {
            optarg = realloc(optarg, ++size);
            optarg[position] = b;
        }
        position++;
        if(eol == 1) {
            if(position != 1) {
                if(matching_entries){
                    input_source = input;
                    break;
                } else {
                    if(matched == '?') {
                        fprintf(stderr, "Got unknown option in config file on line %d\n", fileline);
                    }
                    handle_option(matched, optarg);
                }
            }
            position = 0;
            size = 0;
            eol = 0;
            comment = 0;
            matching_entries = 1;
            memset(matching, 1, option_count - 1);
            if(optarg != NULL){
                //free(optarg);
                optarg = NULL;
            }
            matched = '?';
            fileline++;
        }
    }
    if(position > 1) {
        if(matching_entries){
            input_source = input;
        } else {
            if (matched == '?') {
                for(int i = 0; i < option_count - 1; i++) {
                    if (matching[i] == 1) {
                        matched = long_options[i].val;
                        break;
                    }
                }
            }
            if(matched == '?') {
                fprintf(stderr, "Got unknown option in config file on line %d\n", fileline);
            }
            handle_option(matched, optarg);
        }
    }
    if(input_source != input)
        close(fds.fd);
}

void handle_option(int c, char *optarg) {
    switch (c) {
        case 'v':
            fprintf(stderr, "xlunch graphical program launcher, version %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
            exit(OKAY);

        case 'd':
            desktop_mode = 1;
            break;

        case 'G':
            use_root_img = 1;
            if(background_color_set == 0){
                background_color.r = 0;
                background_color.g = 0;
                background_color.b = 0;
                background_color.a = 100;
            }
            break;

        case 'n':
            no_prompt = 1;
            prompt_spacing = 0;
            prompt_font_height = 0;
            break;

        case 'N':
            no_title = 1;
            font_height = 0;
            text_padding = 0;
            break;

        case 'g':
            background_file = optarg;
            if(background_color_set == 0){
                background_color.r = 0;
                background_color.g = 0;
                background_color.b = 0;
                background_color.a = 100;
            }
            break;

        case 'L':
            highlight_file = optarg;
            break;

        case 'I':
            icon_padding = atoi(optarg);
            break;

        case 1020:
            icon_v_padding = atoi(optarg);
            break;

        case 'T':
            text_padding = atoi(optarg);
            break;

        case 'c':
            ucolumns = atoi(optarg);
            break;

        case 'r':
            urows = atoi(optarg);
            break;

        case 'b':
            if(strcmp(optarg, "auto") == 0){
                uborder.value = -1;
            } else {
                if (optarg[strlen(optarg)-1] == '%') {
                    uborder.percent = atoi(optarg);
                } else {
                    uborder.value = atoi(optarg);
                }
            }
            break;

        case 'B':
            if(strcmp(optarg, "auto") == 0){
                uside_border.value = -1;
            } else {
                if (optarg[strlen(optarg)-1] == '%') {
                    uside_border.percent = atoi(optarg);
                } else {
                    uside_border.value = atoi(optarg);
                }
            }
            break;

        case 'P':
            prompt_spacing = atoi(optarg);
            break;

        case 's':
            icon_size = atoi(optarg);
            break;

        case 'i':
            ui_set("entries.source","legacy","argv",0);
            input_file = optarg;
            break;

        case 'W':
            windowed = 1;
            break;

        case 'p':
            prompt = optarg;
            break;

        case 'f':
            font_name = optarg;
            break;

        case 'F':
            prompt_font_name = optarg;
            break;

        case 'm':
            multiple_instances = 1;
            break;

        case 't':
            void_click_terminate = 1;
            break;

        case 'x':
            uposx = atoi(optarg);
            force_reposition = 1;
            break;

        case 'y':
            uposy = atoi(optarg);
            force_reposition = 1;
            break;

        case 'w':
            uwidth = atoi(optarg);
            break;

        case 'h':
            uheight = atoi(optarg);
            break;

        case 'o':
            output_only = 1;
            break;

        case 'S':
            select_only = 1;
            break;

        case 1009:
            sscanf(optarg, "%02x%02x%02x%02x", &text_color.r, &text_color.g, &text_color.b, &text_color.a);
            break;

        case 1010:
            sscanf(optarg, "%02x%02x%02x%02x", &prompt_color.r, &prompt_color.g, &prompt_color.b, &prompt_color.a);
            break;

        case 1011:
            sscanf(optarg, "%02x%02x%02x%02x", &background_color.r, &background_color.g, &background_color.b, &background_color.a);
            background_color_set = 1;
            break;

        case 1021:
            sscanf(optarg, "%02x%02x%02x%02x", &shadow_color.r, &shadow_color.g, &shadow_color.b, &shadow_color.a);
            break;

        case 1012:
            sscanf(optarg, "%02x%02x%02x%02x", &highlight_color.r, &highlight_color.g, &highlight_color.b, &highlight_color.a);
            break;

        case 1023:
            sscanf(optarg, "%02x%02x%02x%02x", &scrollbar_color.r, &scrollbar_color.g, &scrollbar_color.b, &scrollbar_color.a);
            break;

        case 1024:
            sscanf(optarg, "%02x%02x%02x%02x", &scrollindicator_color.r, &scrollindicator_color.g, &scrollindicator_color.b, &scrollindicator_color.a);
            break;

        case 1029:
            ui_set("ui.mode",optarg,"argv",0);
            break;
        case 1030:
            ui_read(optarg);
            break;
        case 1031:;
            char *assignment=strdup(optarg),*equal=strchr(assignment,'=');
            if (!equal) { fprintf(stderr,"--set expects section.key=value\n"); exit(CONFIGERROR); }
            *equal=0; ui_set(assignment,equal+1,"argv",0); free(assignment);
            break;
        case 1032:
            ui_read(optarg); ui_check_file=optarg;
            break;
        case 1033:
            rebuild_cache=1;
            break;
        case 1034:
            window_class=optarg;
            break;
        case 1035:
            center_window=1;
            break;

        case 'a':
            text_after = 1;
            break;

        case 1013:
            program_name = optarg;
            break;

        case 1022:
            window_title = optarg;
            break;

        case 1025:
            window_icon = optarg;
            break;

        case 'q':
            dont_quit = 1;
            break;

        case 'R':
            reverse = 1;
            break;

        case 'O':
            text_other_side = 1;
            break;

        case 'M':
            clear_memory = 1;
            break;

        case 'u':
            upside_down = 1;
            break;

        case 'X':
            padding_swap = 1;
            break;

        case 'l':
            least_margin = atoi(optarg);
            break;

        case 'V':
            least_v_margin = atoi(optarg);
            break;

        case 'C':
            center_icons = 1;
            break;

        case 'e':
            hide_missing = 1;
            break;

        case 'A':
            parse_button(optarg);
            break;

        case 'U': ;
            unsigned char lb = optarg[0];
            int i = 0;
            shortcut_t *current_shortcut = NULL;
            while (lb != '\0') {
                if ( current_shortcut == NULL ) {
                    current_shortcut = malloc(sizeof(shortcut_t));
                } else {
                    current_shortcut->next = malloc(sizeof(shortcut_t));
                    current_shortcut = current_shortcut->next;
                }
                if ( shortcuts == NULL)
                    shortcuts = current_shortcut;
                current_shortcut->entry = NULL;
                current_shortcut->next = NULL;
                if (( lb & 0x80 ) == 0 ) {          // lead bit is zero, must be a single ascii
                    current_shortcut->key = malloc(sizeof(char));
                    memcpy(current_shortcut->key, &optarg[i], sizeof(char));
                    i+=1;
                } else if (( lb & 0xE0 ) == 0xC0 ) {  // 110x xxxx
                    current_shortcut->key = malloc(sizeof(char)*2);
                    memcpy(current_shortcut->key, &optarg[i], sizeof(char)*2);
                    i+=2;
                } else if (( lb & 0xF0 ) == 0xE0 ) { // 1110 xxxx
                    current_shortcut->key = malloc(sizeof(char)*3);
                    memcpy(current_shortcut->key, &optarg[i], sizeof(char)*3);
                    i+=3;
                } else if (( lb & 0xF8 ) == 0xF0 ) { // 1111 0xxx
                    current_shortcut->key = malloc(sizeof(char)*4);
                    memcpy(current_shortcut->key, &optarg[i], sizeof(char)*4);
                    i+=4;
                } else {
                   printf( "Unrecognized lead byte in shortcut (%02x)\n", lb );
                }
                lb = optarg[i];
            }
            break;

        case 1014:
            parse_config(fopen(optarg, "rb"));
            read_config = 1;
            break;

        case 1015:
            bg_fill = 1;
            break;

        case 1016:
            focus_lost_terminate = 1;
            break;

        case 1017:
            border_ratio = atoi(optarg);
            break;

        case 1018:
            side_border_ratio = atoi(optarg);
            break;

        case 1019:
            noscroll = 1;
            break;

        case '?':
            fprintf(stderr, "See --help for usage documentation\n");
            exit(CONFIGERROR);
            break;

        case 1026:
            stdin_poll_timeout = atoi(optarg);
            break;

        case 1027:
            nostdin = 1;
            break;

        case 1028:
            no_row_stretch = 1;
            break;

        case 'H':
            fprintf (stderr,"usage: xlunch [options]\n"
                            "    xlunch is a program launcher/option selector similar to dmenu or rofi.\n"
                            "    By default it launches in full-screen mode and terminates after a selection is made,\n"
                            "    it is also possible to close xlunch by pressing Esc or the right mouse button.\n"
                            "    Some options changes this behaviour, the most notable being the desktop mode switch:\n"
                            "        -d, --desktop                      Desktop mode, always keep the launcher at\n"
                            "                                           background(behind other windows), and ignore ESC\n"
                            "                                           and right mouse click. Combined with --dontquit\n"
                            "                                           xlunch never exits (behaviour of --desktop from\n"
                            "                                           previous versions).\n"
                            "    Functional options:\n"
                            "        --config [file]                    Reads configuration options from a file. The\n"
                            "                                           options are the same as the long options\n"
                            "                                           specified here. Options that take a value must\n"
                            "                                           have a colon (':') before it's option. It is\n"
                            "                                           also possible to pass the entries along with the\n"
                            "                                           configuration file by using \"entries:\"\n"
                            "                                           followed by a newline and the regular contents\n"
                            "                                           of an input file\n"
                            "        -v, --version                      Returns the version of xlunch\n"
                            "        --ui [classic|focus]               Select layout (default: classic)\n"
                            "        --ui-config [file]                 Apply a UTF-8 INI appearance profile\n"
                            "        --set [section.key=value]          Override one INI property\n"
                            "        --check-ui-config [file]           Validate INI without opening X11\n"
                            "        --rebuild-cache                    Rebuild desktop/icon cache without X11\n"
                            "        -H, --help                         Shows this help message\n"
                            "        --name                             POSIX-esque way to specify the first part of\n"
                            "                                           WM_CLASS (default: environment variable \n"
                            "                                           RESOURCE_NAME or argv[0])\n"
                            "        --class [name]                     Set the second part of WM_CLASS\n"
                            "                                           (default: xlunch-windowed/desktop/fullscreen)\n"
                            "        -N, --notitle                      Do not display any titles under/around icons\n"
                            "        -n, --noprompt                     Hides the prompt, only allowing selection\n"
                            "                                           by icon\n"
                            "        -o, --outputonly                   Do not run the selected entry, only output it\n"
                            "                                           to stdout\n"
                            "        -S, --selectonly                   Only allow an actual entry and not free-typed\n"
                            "                                           commands. Nice for scripting.\n"
                            "        -i, --input [file]                 File to read entries from, defaults to stdin\n"
                            "                                           if data is available otherwise it reads from\n"
                            "                                           $HOME/.config/xlunch/entries.dsv or\n"
                            "                                           /etc/xlunch/entries.dsv\n"
                            "        -m, --multiple                     Allow multiple instances running\n"
                            "        -t, --voidclickterminate           Clicking anywhere that's not an entry terminates\n"
                            "                                           xlunch, practical for touch screens.\n"
                            "        --focuslostterminate               When the window loses focus xlunch will quit,\n"
                            "                                           practical for menus\n"
                            "        -q, --dontquit                     When an option is selected, don't close xlunch.\n"
                            "                                           Combined with --desktop xlunch\n"
                            "                                           never exits (behaviour of --desktop from\n"
                            "                                           previous versions).\n"
                            "        -R, --reverse                      All entries in xlunch as reversly ordered.\n"
                            "        -W, --windowed                     Start in windowed mode\n"
                            "        --center-window                    Center a window on the X screen, overriding\n"
                            "                                           --xposition and --yposition (requires -W)\n"
                            "        --title                            Specifies the title to put on the window when\n"
                            "                                           running in windowed mode.\n"
                            "        --icon                             Specifies the icon to put on the window when\n"
                            "                                           running in windowed mode.\n"
                            "        -M, --clearmemory                  Set the memory of each entry to null before\n"
                            "                                           exiting. Used for passing sensitive information\n"
                            "                                           through xlunch.\n"
                            "        -U, --shortcuts [shortcuts]        Sets shortcuts for the entries, 'shortcuts' is a\n"
                            "                                           string of UTF-8 characters to use sequentially\n"
                            "                                           for the entries provided.\n"
                            "        -A, --button [button]              Adds a button to the window. The argument\n"
                            "                                            \"button\" is a semicolon-separated list on the\n"
                            "                                           form \"<icon>;<highlight icon>;<x>,<y>;<command>\"\n"
                            "                                           If x or y is negative positioning is relative\n"
                            "                                           to the other side of the screen.\n"
                            "        --noscroll                         Disable scroll in xlunch. Ignore entries\n"
                            "                                           that can't fit the screen.\n"
                            "        --stdinpolltimeout [i]             How long xlunch should wait (in milliseconds)\n"
                            "                                           for data on stdin before assuming it is\n"
                            "                                           invalid. Use if you find that your xlunch\n"
                            "                                           menu ends up empty on occasion, even though\n"
                            "                                           the entries you pass to xlunch through stdin\n"
                            "                                           are sound. Defaults to 10.\n"
                            "        --nostdin                          Disable stdin feature.\n\n"
                            "    Multi monitor setup: xlunch cannot detect your output monitors, it sees your monitors\n"
                            "    as a big single screen. You can customize this manually by setting windowed mode and\n"
                            "    providing the top/left coordinates and width/height of your monitor screen which\n"
                            "    effectively positions xlunch on the desired monitor. Use the following options:\n"
                            "        -x, --xposition [i]                The x coordinate of the launcher window\n"
                            "                                           Use negative number to align from right\n"
                            "        -y, --yposition [i]                The y coordinate of the launcher window\n"
                            "                                           Use negative number to align from bottom\n"
                            "        -w, --width [i]                    The width of the launcher window\n"
                            "        -h, --height [i]                   The height of the launcher window\n\n"
                            "    Style options:\n"
                            "        -p, --prompt [text]                The prompt asking for input (default: \"Run: \")\n"
                            "        -f, --font [name]                  Font name including size after slash (default:\n"
                            "                                           \"OpenSans-Regular/10\" and  \"DejaVuSans/10\")\n"
                            "        -F, --promptfont [name]            Font to use for the prompt\n"
                            "                                           (default: same as --font)\n"
                            "        -G, --rootwindowbackground         Use root windows background image\n"
                            "        -g, --background [file]            Image to set as background (jpg/png). NOTE: the\n"
                            "                                           background color will be drawn over this image\n"
                            "                                           so use a fully transparent background color if\n"
                            "                                           the image should be drawn as-is.\n"
                            "        --bgfill                           Makes the background keep aspect ratio\n"
                            "                                           while stretching\n"
                            "        -L, --highlight [file]             Image set as highlighting under selected icon\n"
                            "                                           (jpg/png)\n"
                            "        -I, --iconpadding [i]              Padding around icons (default: 10)\n"
                            "            --iconvpadding [i]             Vertical padding around icons (default: same as\n"
                            "                                           iconpadding)\n"
                            "        -T, --textpadding [i]              Padding around entry titles (default: 10)\n"
                            "        -c, --columns [i]                  Number of columns to show (without this the max\n"
                            "                                           amount possible is used)\n"
                            "        -r, --rows [i]                     Numbers of rows to show (without this the max\n"
                            "                                           amount possible is used)\n"
                            "        -b, --border [i]                   Size of the border around the icons and prompt\n"
                            "                                           (default: 1/10th of screen width)\n"
                            "                                           This can also be set to 'auto' in order to\n"
                            "                                           automatically calculate a border taking into\n"
                            "                                           account the margin settings and the\n"
                            "                                           configured columns and rows. You can also specify\n"
                            "                                           border in terms of percentage of screen width by\n"
                            "                                           appending a %% sign to the value\n"
                            "        -B, --sideborder [i]               Size of the border on the sides, if this is used\n"
                            "                                           --border will be only top and bottom. Similarily\n"
                            "                                           this can be set to 'auto' or a percentage but\n"
                            "                                           then only side borders are calculated\n"
                            "        --borderratio [i]                  The ratio of the border to apply above the\n"
                            "                                           content. 0 is no top border, only bottom. 100 is\n"
                            "                                           only top border, no bottom\n"
                            "        --sideborderratio [i]              The ratio of the side border to apply to the\n"
                            "                                           left of the content. 0 is no left border, only\n"
                            "                                           right. 100 is only left border, no right\n"
                            "        -C, --center                       Center entries when there are fewer entries on a\n"
                            "                                           row than the maximum\n"
                            "        -P, --promptspacing [i]            Distance between the prompt and the icons\n"
                            "                                           (default: 48)\n"
                            "        -s, --iconsize [i]                 Size of the icons (default: 48)\n"
                            "        -a, --textafter                    Draw the title to the right of the icon instead\n"
                            "                                           of below, this option automatically sets\n"
                            "                                           --columns to 1 but this can be overridden.\n"
                            "        -O, --textotherside                Draw the text on the other side of the icon from\n"
                            "                                           where it is normally drawn.\n"
                            "        -u, --upsidedown                   Draw the prompt on the bottom and have icons\n"
                            "                                           sort from bottom to top.\n"
                            "        -X, --paddingswap                  Icon padding and text padding swaps order\n"
                            "                                           around text.\n"
                            "        -l, --leastmargin [i]              Adds a margin to the calculation of\n"
                            "                                           application sizes.\n"
                            "        -V, --leastvmargin [i]             Adds a vertical margin to the calculation of\n"
                            "                                           application sizes.\n"
                            "        --nostretchrows                    Keep row gaps at leastvmargin instead of\n"
                            "                                           stretching them to fill the available height.\n"
                            "        -e, --hidemissing                  Hide entries with missing or broken icon images\n"
                            "        --tc, --textcolor [color]          Color to use for the text on the format rrggbbaa\n"
                            "                                           (default: ffffffff)\n"
                            "        --pc, --promptcolor [color]        Color to use for the prompt text\n"
                            "                                           (default: ffffffff)\n"
                            "        --bc, --backgroundcolor [color]    Color to use for the background\n"
                            "                                           (default: 2e3440ff) NOTE: transparent background\n"
                            "                                           color requires a compositor\n"
                            "        --sc, --shadowcolor [color]        Color to use for text shadows (default: 00000030)\n"
                            "        --hc, --highlightcolor [color]     Color to use for the highlight box\n"
                            "                                           (default: ffffff32)\n"
                            "        --scrollbarcolor [color]           Color to use for the scrollbar\n"
                            "                                           (default: ffffff20)\n"
                            "        --scrollindicatorcolor [color]     Color to use for the scrollbar indicator\n"
                            "                                           (default: ffffffa0)\n\n");
            // Check if we came from the error block above or if this was a call with --help
            if(c == '?'){
                exit(CONFIGERROR);
            } else {
                exit(OKAY);
            }
            break;
    }
}
