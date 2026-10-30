#include "ui.h"

// this code is not optimal in many ways, but works just nicely.
// Licence: GNU GPL v3
// Authors: Tomas Matejicek <www.slax.org>
//          Peter Munch-Ellingsen <www.peterme.net>
const int VERSION_MAJOR = 5; // Major version, changes when breaking backwards compatability
const int VERSION_MINOR = 9; // Minor version, changes when new functionality is added
const int VERSION_PATCH = 1; // Patch version, changes when something is changed without changing deliberate functionality (eg. a bugfix or an optimisation)

/* open and O_RDWR,O_CREAT */
/* include X11 stuff */
/* include Imlib2 stuff */
/* basename include */
/* sprintf include */
/* strcpy include */
/* exit include */
/* exec include */
/* long options include*/
/* X utils */
/* parse commandline arguments */
/* one instance */
/* check stdin */

/* some globals for our window & X display */
Display *disp;
Window   win;
XVisualInfo vinfo;
XSetWindowAttributes attr;
int x11_fd;
struct pollfd eventfds[3]={{.fd=-1},{.fd=-1},{.fd=-1}};

XIM im;
XIC ic;

int screen;
int screen_width;
int screen_height;

// Let's define a linked list node:












int entries_count = 0;
node_t * entries = NULL;
button_t * buttons = NULL;
shortcut_t * shortcuts = NULL;
keynode_t * cmdline = NULL;



struct option long_options[] =
    {
        {"tc",                    required_argument, 0, 1009},
        {"textcolor",             required_argument, 0, 1009},
        {"pc",                    required_argument, 0, 1010},
        {"promptcolor",           required_argument, 0, 1010},
        {"backgroundcolor",       required_argument, 0, 1011},
        {"bc",                    required_argument, 0, 1011},
        {"hc",                    required_argument, 0, 1012},
        {"highlightcolor",        required_argument, 0, 1012},
        {"name",                  required_argument, 0, 1013},
        {"config",                required_argument, 0, 1014},
        {"bgfill",                no_argument,       0, 1015},
        {"focuslostterminate",    no_argument,       0, 1016},
        {"borderratio",           required_argument, 0, 1017},
        {"sideborderratio",       required_argument, 0, 1018},
        {"noscroll",              no_argument,       0, 1019},
        {"iconvpadding",          required_argument, 0, 1020},
        {"shadowcolor",           required_argument, 0, 1021},
        {"sc",                    required_argument, 0, 1021},
        {"title",                 required_argument, 0, 1022},
        {"icon",                  required_argument, 0, 1025},
        {"scrollbarcolor",        required_argument, 0, 1023},
        {"scrollindicatorcolor",  required_argument, 0, 1024},
        {"stdinpolltimeout",      required_argument, 0, 1026},
        {"nostdin",               no_argument,       0, 1027},
        {"nostretchrows",          no_argument,       0, 1028},
        {"ui",                     required_argument, 0, 1029},
        {"ui-config",              required_argument, 0, 1030},
        {"set",                    required_argument, 0, 1031},
        {"check-ui-config",        required_argument, 0, 1032},
        {"rebuild-cache",          no_argument,       0, 1033},
        {"class",                  required_argument, 0, 1034},
        {"center-window",          no_argument,       0, 1035},
        {"button",                required_argument, 0, 'A'},
        {"textafter",             no_argument,       0, 'a'},
        {"border",                required_argument, 0, 'b'},
        {"sideborder",            required_argument, 0, 'B'},
        {"center",                no_argument,       0, 'C'},
        {"columns",               required_argument, 0, 'c'},
        {"desktop",               no_argument,       0, 'd'},
        {"hidemissing",           no_argument,       0, 'e'},
        {"font",                  required_argument, 0, 'f'},
        {"promptfont",            required_argument, 0, 'F'},
        {"background",            required_argument, 0, 'g'},
        {"rootwindowbackground",  no_argument,       0, 'G'},
        {"height",                required_argument, 0, 'h'},
        {"help",                  no_argument,       0, 'H'},
        {"iconpadding",           required_argument, 0, 'I'},
        {"input",                 required_argument, 0, 'i'},
        {"highlight",             required_argument, 0, 'L'},
        {"leastmargin",           required_argument, 0, 'l'},
        {"clearmemory",           no_argument,       0, 'M'},
        {"multiple",              no_argument,       0, 'm'},
        {"noprompt",              no_argument,       0, 'n'},
        {"notitle",               no_argument,       0, 'N'},
        {"outputonly",            no_argument,       0, 'o'},
        {"textotherside",         no_argument,       0, 'O'},
        {"prompt",                required_argument, 0, 'p'},
        {"promptspacing",         required_argument, 0, 'P'},
        {"dontquit",              no_argument,       0, 'q'},
        {"reverse",               no_argument,       0, 'R'},
        {"rows",                  required_argument, 0, 'r'},
        {"iconsize",              required_argument, 0, 's'},
        {"selectonly",            no_argument,       0, 'S'},
        {"textpadding",           required_argument, 0, 'T'},
        {"voidclickterminate",    no_argument,       0, 't'},
        {"shortcuts",             required_argument, 0, 'U'},
        {"upsidedown",            no_argument,       0, 'u'},
        {"leastvmargin",          required_argument, 0, 'V'},
        {"version",               no_argument,       0, 'v'},
        {"width",                 required_argument, 0, 'w'},
        {"windowed",              no_argument,       0, 'W'},
        {"paddingswap",           no_argument,       0, 'X'},
        {"xposition",             required_argument, 0, 'x'},
        {"yposition",             required_argument, 0, 'y'},
        {0, 0, 0, 0}
    };
int icon_size = 48;
int ucolumns = 0;
int columns;
int urows = 0;
int rows;
int column_margin = 0;
int row_margin = 0;
int no_row_stretch = 0;
int icon_padding = 40;
int icon_v_padding = -1;
int text_padding = 10;
int border;
int side_border = 0;
int border_ratio = 50;
int side_border_ratio = 50;
int cell_width;
int cell_height;
int font_height;
int prompt_font_height;
int use_root_img = 0;
char commandline[10024];
char commandlinetext[10024];
int prompt_x;
int prompt_y;
int mouse_moves=0;
char * background_file = "";
char * highlight_file = "";
char * input_file = "";
int read_config = 0;
FILE * input_source = NULL;
char * prompt = "";
char * font_name = "";
char * prompt_font_name = "";
char * program_name;
char * window_title;
char * window_class;
char * window_icon;
int bg_fill = 0;
int no_prompt = 0;
int no_title = 0;
int prompt_spacing = 48;
int windowed = 0;
int center_window = 0;
int multiple_instances = 0;
int uposx = 0;
int uposy = 0;
int force_reposition = 0;
int uwidth = 0;
int uheight = 0;
percentable_t uborder = { .percent = -1, .value = 0 };
percentable_t uside_border = { .percent = -1, .value = 0 };
int void_click_terminate = 0;
int focus_lost_terminate = 0;
int dont_quit = 0;
int reverse = 0;
int output_only = 0;
int select_only = 0;
int text_after = 0;
int text_other_side = 0;
int clear_memory = 0;
int upside_down = 0;
int padding_swap = 0;
int least_margin = 0;
int least_v_margin = -1;
int hide_missing = 0;
int center_icons = 0;
int noscroll = 0;
double scrolled_past=0;
double scroll_target=0;
double scroll_start=0;
struct timespec scroll_time;
int scrollbar_drag_y=-1;
double scrollbar_drag_scroll;

int hovered_entry = 0;
color_t text_color = {.r = 255, .g = 255, .b = 255, .a = 255};
color_t prompt_color = {.r = 255, .g = 255, .b = 255, .a = 255};
color_t background_color = {.r = 46, .g = 52, .b = 64, .a = 102};
color_t shadow_color = {.r = 0, .g = 0, .b = 0, .a = 30};
int background_color_set = 0;
color_t highlight_color = {.r = 255, .g = 255, .b = 255, .a = 50};
color_t scrollbar_color = {.r = 255, .g = 255, .b = 255, .a = 32};
color_t scrollindicator_color = {.r = 255, .g = 255, .b = 255, .a = 160};
int stdin_poll_timeout = 10;
int nostdin = 0;

int hoverset=MOUSE;
int desktop_mode=0;
int lock;

/* areas to update */
Imlib_Updates updates, current_update;
/* our background image, rendered only once */
Imlib_Image background = NULL;
/* highlighting image (placed under icon on hover) */
Imlib_Image highlight = NULL;
/* image variable */
Imlib_Image image = NULL;


const int option_count=sizeof(long_options)/sizeof(long_options[0]);

void restack()
{
    if (desktop_mode) XLowerWindow(disp,win);
    else if (!windowed) XRaiseWindow(disp,win);
}

void cleanup()
{
    cache_cancel();
    focus_cleanup();
    flock(lock, LOCK_UN | LOCK_NB);
    // destroy window, disconnect display, and exit
    XDestroyWindow(disp,win);
    XFlush(disp);
    XCloseDisplay(disp);

    if(input_source == stdin){
        int fd = fileno(stdin);
        int flags = fcntl(fd, F_GETFL, 0);
        flags &= ~O_NONBLOCK;
        fcntl(fd, F_SETFL, flags);
        fclose(input_source);
    }

    clear_entries();
    button_t * button = buttons;
    buttons = NULL;
    while (button != NULL) {
      button_t *last = button;
      button = button->next;
      free(last);
    }
    shortcut_t * shortcut = shortcuts;
    shortcuts = NULL;
    while (shortcut != NULL) {
      shortcut_t *last = shortcut;
      shortcut = shortcut->next;
      free(last->key);
      free(last);
    }
}

void init(int argc, char **argv)
{
    // If no configuration file was explicitly given, try to read a default one
    if (read_config == 0) {
        FILE *config_source;
        char * homeconf = NULL;

        char * home = getenv("HOME");
        if (home!=NULL)
        {
            homeconf = concat(home,"/.config/xlunch/xlunch.conf");
        }
        config_source = fopen(homeconf, "rb");
        if(config_source == NULL) {
            config_source = fopen("/etc/xlunch/default.conf", "rb");
        }
        free(homeconf);
        if(config_source != NULL){
            parse_config(config_source);
            fclose(config_source);
        }
    }

    // Handle options from the command line
    int c, option_index;
    while ((c = getopt_long(argc, argv, "vdr:nNg:L:b:B:s:i:p:f:mc:x:y:w:h:oatGHI:T:P:WF:SqROMuXeCl:V:U:A:", long_options, &option_index)) != -1) {
        handle_option(c, optarg);
    }

    if (least_v_margin == -1) least_v_margin = least_margin;
    if (icon_v_padding == -1) icon_v_padding = icon_padding;
    ui_validate();
    if (ui_check_file) { fprintf(stderr,"UI configuration is valid: %s\n",ui_check_file); exit(OKAY); }
    if (focus_mode || !strcmp(ui_value("entries.source"),"desktop"))
    {
        setlocale(LC_CTYPE,""); setlocale(LC_MESSAGES,""); setlocale(LC_COLLATE,"");
        // Desktop metadata and INI labels are UTF-8 even under the C locale.
        if (strcasecmp(nl_langinfo(CODESET),"UTF-8") && strcasecmp(nl_langinfo(CODESET),"UTF8")) setlocale(LC_CTYPE,"C.UTF-8");
    }

    if (rebuild_cache)
    {
        if (strcmp(ui_value("entries.source"),"desktop"))
        {
            fprintf(stderr,"--rebuild-cache requires entries.source=desktop\n"); exit(CONFIGERROR);
        }
        exit(cache_rebuild() ? OKAY : EXTERNALERROR);
    }

    /* connect to X */
    disp = XOpenDisplay(NULL);
    if (!disp)
    {
        fprintf(stderr,"Cannot connect to DISPLAY\n");
        exit(WINERROR);
    }

    if (!XMatchVisualInfo(disp, DefaultScreen(disp), 32, TrueColor, &vinfo)) {
        if (!XMatchVisualInfo(disp, DefaultScreen(disp), 24, TrueColor, &vinfo)) {
           if (!XMatchVisualInfo(disp, DefaultScreen(disp), 16, DirectColor, &vinfo)) {
              XMatchVisualInfo(disp, DefaultScreen(disp), 8, PseudoColor, &vinfo);
           }
        }
    }

    attr.colormap = XCreateColormap(disp, DefaultRootWindow(disp), vinfo.visual, AllocNone);
    attr.border_pixel = 0;
    attr.background_pixel = 0;

    /* get default visual , colormap etc. you could ask imlib2 for what it */
    /* thinks is the best, but this example is intended to be simple */
    screen = DefaultScreen(disp);
    /* get/set screen size */
    if (uwidth==0) screen_width=DisplayWidth(disp,screen);
    else screen_width=uwidth;

    if (uheight==0) screen_height=DisplayHeight(disp,screen);
    else screen_height=uheight;

    // calculate relative positions if they are negative
    if (uposx < 0) uposx = DisplayWidth(disp,screen) + uposx - uwidth;
    if (uposy < 0) uposy = DisplayHeight(disp,screen) + uposy - uheight;

    // Center the requested window within the X screen, overriding explicit coordinates.
    if (windowed && center_window)
    {
        uposx=(DisplayWidth(disp,screen)-screen_width)/2;
        uposy=(DisplayHeight(disp,screen)-screen_height)/2;
        force_reposition=1;
    }

    calculate_percentage(screen_height, &uborder);
    calculate_percentage(screen_width, &uside_border);
}

int main(int argc, char **argv){
    /* events we get from X */
    XEvent ev;
    /* our virtual framebuffer image we draw into */
    Imlib_Image buffer;
    Imlib_Image render_buffer;

    /* width and height values */
    int w, h, x, y;

    // set initial variables now
    init(argc, argv);

    if(program_name == NULL) {
        program_name = getenv("RESOURCE_NAME");
        if(program_name == NULL) {
            program_name = argv[0];
        }
    }
    joincmdlinetext();

    // If an instance is already running, quit
    if (!multiple_instances)
    {
        int oldmask = umask(0);
        lock = open("/tmp/xlunch.lock", O_CREAT | O_RDWR, 0666);
        umask(oldmask);
        int rc = flock(lock, LOCK_EX | LOCK_NB);
        if (rc) {
            if (errno == EWOULDBLOCK) fprintf(stderr,"xlunch already running. You may want to consider --multiple\nIf this is an error, you may remove /tmp/xlunch.lock\n");
            exit(LOCKERROR);
        }
    }

    // Leave the cursor unset so the window inherits the desktop's cursor.
    win = XCreateWindow(disp, DefaultRootWindow(disp), uposx, uposy, screen_width, screen_height, 0, vinfo.depth, InputOutput, vinfo.visual, CWColormap | CWBorderPixel | CWBackPixel, &attr);

    // absolute fullscreen mode by overide redirect
    if (!windowed && desktop_mode)
    {
        unsigned long valuemask = CWOverrideRedirect;
        XSetWindowAttributes attributes;
        attributes.override_redirect=True;
        XChangeWindowAttributes(disp,win,valuemask,&attributes);
    }

    /* add the ttf fonts dir to our font path */
    char* homedir;
    if((homedir = getenv("HOME")) != NULL) {
        imlib_add_path_to_font_path(concat(homedir,"/.local/share/fonts"));
        imlib_add_path_to_font_path(concat(homedir,"/.fonts"));
    }
    imlib_add_path_to_font_path("/usr/local/share/fonts");
    imlib_add_path_to_font_path("/usr/share/fonts/truetype");
    imlib_add_path_to_font_path("/usr/share/fonts/truetype/dejavu");
    imlib_add_path_to_font_path("/usr/share/fonts/TTF");
    /* set our cache to 2 Mb so it doesn't have to go hit the disk as long as */
    /* the images we use use less than 2Mb of RAM (that is uncompressed) */
    imlib_set_cache_size(2048 * screen_width);
    /* set the font cache to 512Kb - again to avoid re-loading */
    imlib_set_font_cache_size(512 * screen_width);
    /* Preload fonts so that recalc can know their sizes */
    load_font();
    load_prompt_font();
    recalc_cells();
    /* set the maximum number of colors to allocate for 8bpp and less to 128 */
    imlib_set_color_usage(128);
    /* dither for depths < 24bpp */
    imlib_context_set_dither(1);
    /* set the display , visual, colormap and drawable we are using */
    imlib_context_set_display(disp);
    imlib_context_set_visual(vinfo.visual);
    imlib_context_set_colormap(attr.colormap);
    imlib_context_set_drawable(win);

    update_background_images();

    /* tell X what events we are interested in */
    XSelectInput(disp, win, StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask | ExposureMask | KeyPressMask | KeyReleaseMask | KeymapStateMask | FocusChangeMask );
    GC gc = XCreateGC(disp, win, 0, 0);
    /* set our name */
    XClassHint* classHint;

    /* set the titlebar name */
    if (window_title == NULL) {
        window_title = "xlunch: Graphical app launcher";
    }
    XStoreName(disp, win, window_title);

    /* set the icon */
    if (window_icon != NULL) {
        Imlib_Image icon = imlib_load_image(window_icon);
        if (icon != NULL) {
            imlib_context_set_image(icon);
            DATA32* icon_data = imlib_image_get_data_for_reading_only();
            int width = imlib_image_get_width();
            int height = imlib_image_get_height();
            int elements = width*height + 2;
            size_t* icon_data_x11 = malloc(elements * sizeof(size_t));
            icon_data_x11[0] = width;
            icon_data_x11[1] = height;
            // Can't simply use memcpy here because the target buffer is
            // architecture dependent but the imlib buffer is always 32-bit
            for(int i = 0; i < width*height; i++) {
                icon_data_x11[2+i] = icon_data[i];
            }
            Atom net_wm_icon = XInternAtom(disp, "_NET_WM_ICON", False);
            Atom cardinal = XInternAtom(disp, "CARDINAL", False);
            XChangeProperty(disp, win, net_wm_icon, cardinal, 32, PropModeReplace, (const unsigned char*) icon_data_x11, elements);
            free(icon_data_x11);
            imlib_free_image();
        }
    }


    /* set the name and class hints for the window manager to use */
    classHint = XAllocClassHint();
    if (classHint) {
        classHint->res_name = basename(program_name);
        classHint->res_class = (windowed ? "xlunch-windowed" : (desktop_mode ? "xlunch-desktop" : "xlunch-fullscreen"));
        if (window_class) classHint->res_class=window_class;
    }
    XSetClassHint(disp, win, classHint);
    XFree(classHint);
    /* show the window */
    XMapRaised(disp, win);
    /* Force window reposition, can make effect only when windowed is enabled, depending on WM */
    if (force_reposition)
       XMoveWindow(disp,win,uposx,uposy);

    // prepare for keyboard UTF8 input
    if (XSetLocaleModifiers("@im=none") == NULL) {
        cleanup();
        exit(LOCALEERROR);
    }
    im = XOpenIM(disp, NULL, NULL, NULL);
    if (im == NULL) {
        fputs("Could not open input method\n", stdout);
        cleanup();
        exit(INPUTMERROR);
    }

    ic = XCreateIC(im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, win, NULL);
    if (ic == NULL) {
        fprintf(stderr,"Could not open input context, without it we can't properly handle UTF8\n");
        cleanup();
        exit(INPUTCERROR);
    }
    XSetICFocus(ic);

    // send to back or front, depending on settings
    restack();

    // parse entries file
    if (!strcmp(ui_value("entries.source"),"desktop"))
    {
        if (input_source && input_source!=stdin) fclose(input_source);
        input_source=NULL;
        if (!cache_load(0)) ui_load_entries();
    }
    else
    {
        input_source = determine_input_source();
        if(input_source != NULL) parse_entries();
    }
    ui_custom_entries(); filter_entries();
    if (focus_mode) focus_layout();
    arrange_positions();

    // accept delete window notifications from window managers
    Atom wm_delete_window = XInternAtom(disp, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(disp, win, &wm_delete_window, 1);

    // prepare message for window manager that we are requesting fullscreen
    XClientMessageEvent msg = {
        .type = ClientMessage,
        .display = disp,
        .window = win,
        .message_type = XInternAtom(disp, "_NET_WM_STATE", True),
        .format = 32,
        .data = {
            .l = {
                1 /* _NET_WM_STATE_ADD */,
                XInternAtom(disp, "_NET_WM_STATE_FULLSCREEN", True),
                None,
                0,
                1
            }
        }
    };

    // send the message
    if (!windowed && !desktop_mode)
        XSendEvent(disp, DefaultRootWindow(disp), False, SubstructureRedirectMask | SubstructureNotifyMask, (XEvent*)&msg);

    // Get the FD of the X11 display
    x11_fd = ConnectionNumber(disp);
    eventfds[0].fd = x11_fd;
    eventfds[0].events = POLLIN;
    if(input_source == stdin) {
        eventfds[1].fd = 0; /* this is STDIN */
        eventfds[1].events = POLLIN;
    }

    int cache_started=0;
    /* infinite event loop */
    for (;;)
    {
        /* init our updates to empty */
        updates = imlib_updates_init();

        // Poll for events, while blocking until one becomes available
        int poll_result;
        if(!XPending(disp)){
            poll_result = poll(eventfds, 3, scroll_target != scrolled_past ? 16 : -1);
        } else {
            poll_result = 1;
            eventfds[0].revents = 1;
        }

        if(poll_result < 0){
            // An error occured, abort
            cleanup();
            exit(POLLERROR);
        } else {
            if(input_source == stdin && eventfds[1].revents != 0){
                int changed = parse_entries();
                if(changed){
                    updates = imlib_update_append_rect(updates, 0, 0, screen_width, screen_height);
                }
            }
            if(eventfds[0].revents != 0) {
                /* while there are events form X - handle them */
                while (XPending(disp))
                {
                    XNextEvent(disp, &ev);

                    // allow through only UTF8 events
                    if (XFilterEvent(&ev, win)) continue;
                    if (focus_mode && focus_event(&ev)) continue;

                    switch (ev.type)
                    {

                    case Expose:
                        /* window rectangle was exposed - add it to the list of */
                        /* rectangles we need to re-render */
                        updates = imlib_update_append_rect(updates, ev.xexpose.x, ev.xexpose.y, ev.xexpose.width, ev.xexpose.height);
                        break;

                    case FocusIn:
                        restack();
                        break;

                    case FocusOut:
                        restack();
                        if (focus_lost_terminate) {
                            cleanup();
                            exit(FOCUSLOST);
                        }
                        break;


                    case ConfigureNotify:
                        if (screen_width!=ev.xconfigure.width || screen_height!=ev.xconfigure.height)
                        {
                            screen_width=ev.xconfigure.width;
                            screen_height=ev.xconfigure.height;
                            calculate_percentage(screen_height, &uborder);
                            calculate_percentage(screen_width, &uside_border);
                            if (!use_root_img) update_background_images();
                            recalc_cells();
                            scrollbar_drag_y=-1;
                            set_scroll_level(scrolled_past);
                            arrange_positions();
                            updates = imlib_update_append_rect(updates, 0, 0, screen_width, screen_height);
                        }
                        break;

                    case ButtonPress:
                        handleButtonPress(ev);
                        break;

                    case ButtonRelease:
                        handleButtonRelease(ev);
                        break;

                    // refresh keyboard layout if changed
                    case MappingNotify:
                        XRefreshKeyboardMapping(&ev.xmapping);
                        break;

                    case KeyPress:
                        handleKeyPress(ev);
                        break;

                    case KeyRelease:
                        break;

                    case LeaveNotify:
                    case EnterNotify:
                    case MotionNotify:
                        if (ev.type == MotionNotify && scrollbar_drag_y >= 0) drag_scrollbar(ev.xmotion.y);
                        mouse_moves++;
                        recheckHover(ev);
                        break;

                    case ClientMessage:
                        if (ev.xclient.data.l[0] == wm_delete_window) {
                          cleanup();
                          exit(ESCAPE);
                        }
                        break;

                    default:
                        /* any other events - do nothing */
                        break;
                    }
                }
            }
            cache_update();
            // Animate wheel scrolling using elapsed time, including frames without X events.
            if (scroll_target != scrolled_past)
            {
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                double elapsed=(now.tv_sec-scroll_time.tv_sec)+(now.tv_nsec-scroll_time.tv_nsec)/1000000000.0;
                if (focus_mode)
                {
                    double duration=ui_number("scroll.duration")/1000.0;
                    double progress=duration>0 ? fmin(1,elapsed/duration) : 1;
                    double easing=1-(1-progress)*(1-progress)*(1-progress);
                    scrolled_past=scroll_start+(scroll_target-scroll_start)*easing;
                }
                else
                {
                    double fraction=elapsed/(.06+elapsed);
                    scroll_time=now;
                    scrolled_past+=(scroll_target-scrolled_past)*fraction;
                }
                double remaining=(scroll_target-scrolled_past)*(cell_height+row_margin);
                if (remaining > -0.5 && remaining < 0.5) scrolled_past=scroll_target;
                arrange_positions();
                Window root, child;
                int root_x, root_y;
                unsigned int mask;
                XEvent pointer={0};
                if (XQueryPointer(disp, win, &root, &child, &root_x, &root_y, &pointer.xmotion.x, &pointer.xmotion.y, &mask)) recheckHover(pointer);
                updates=imlib_update_append_rect(updates, 0, 0, screen_width, screen_height);
            }
            /* no more events for now ? ok - idle time so lets draw stuff */

            /* take all the little rectangles to redraw and merge them into */
            /* something sane for rendering */
            updates = imlib_updates_merge_for_rendering(updates, screen_width, screen_height);

            for (current_update = updates;   current_update;    current_update = imlib_updates_get_next(current_update))
            {
                int up_x, up_y, up_w, up_h;

                /* find out where the first update is */
                imlib_updates_get_coordinates(current_update, &up_x, &up_y, &up_w, &up_h);

                /* create our buffer image for rendering this update */
                buffer = imlib_create_image(up_w, up_h);

                /* we can blend stuff now */
                imlib_context_set_blend(1);

                imlib_context_set_image(buffer);

                imlib_image_set_has_alpha(1);
                imlib_image_clear();
                /* blend background image onto the buffer */
                imlib_blend_image_onto_image(background, 1, 0, 0, screen_width, screen_height, - up_x, - up_y, screen_width, screen_height);

                node_t * current = entries;
                int top, bottom;
                entry_viewport(&top, &bottom);
                imlib_context_set_cliprect(0, top-up_y, up_w, bottom-top);

                if (focus_mode)
                {
                    imlib_context_set_cliprect(0,0,0,0);
                    focus_render(buffer,up_x,up_y);
                }
                else
                {

                while (current != NULL)
                {
                    if (!current->hidden && current->y+cell_height > top && current->y < bottom)
                    {
                        renderEntry(buffer, current, up_x, up_y);
                    }
                    current = current->next;
                }
                imlib_context_set_cliprect(0, 0, 0, 0);

                button_t *button = buttons;
                while (button != NULL) {
                    image = imlib_load_image(button->hovered ? (button->icon_highlight[0] == '\0' ? button->icon_normal : button->icon_highlight) : button->icon_normal);
                    if (image)
                    {
                        imlib_context_set_image(buffer);

                        int d;
                        if (button->clicked) d=2;
                        else d=0;
                        int x = (button->x < 0 ? screen_width + button->x + 1 - button->w : button->x);
                        int y = (button->y < 0 ? screen_height + button->y + 1 - button->h : button->y);

                        imlib_blend_image_onto_image(image, 1, 0, 0, button->w, button->h, x - up_x + d, y - up_y + d, button->w-d*2, button->h-d*2);

                        imlib_context_set_image(image);
                        imlib_free_image();
                    }

                    button = button->next;
                }

                /* set the buffer image as our current image */
                imlib_context_set_image(buffer);

                /* draw prompt */
                if (!no_prompt) {
                    Imlib_Font font = load_prompt_font();
                    if (font)
                    {
                        imlib_context_set_font(font);
                        if(upside_down) {
                            draw_text_with_shadow(prompt_x - up_x, (screen_height - prompt_font_height) - (prompt_y - up_y), commandlinetext, prompt_color);
                        } else {
                            draw_text_with_shadow(prompt_x - up_x, prompt_y - up_y, commandlinetext, prompt_color);
                        }
                        /* free the font */
                        imlib_free_font();
                    }
                }


                /* draw the scrollbar using the same geometry as its hit testing */
                scrollbar_t bar=scrollbar_geometry();
                if (bar.width)
                {
                    int x=bar.x+(bar.width-8)/2-up_x;
                    draw_scrollbar_pill(buffer, x, bar.y-up_y, bar.height, scrollbar_color);
                    draw_scrollbar_pill(buffer, x, bar.thumb_y-up_y, bar.thumb_height, scrollindicator_color);
                    imlib_context_set_blend(0);
                }
                }

                render_buffer = imlib_create_image(up_w, up_h);
                imlib_context_set_image(render_buffer);
                imlib_context_set_blend(1);
                imlib_image_clear();

                imlib_image_copy_alpha_to_image(buffer, 0, 0);
                imlib_blend_image_onto_image(buffer, 1, 0, 0, up_w, up_h, 0, 0, up_w, up_h);

                imlib_context_set_image(buffer);
                imlib_free_image();

                imlib_context_set_image(render_buffer);
                imlib_render_image_on_drawable(up_x, up_y);
                imlib_free_image();
            }
        }
        /* if we had updates - free them */
        if (updates)
            imlib_updates_free(updates);
        if (!cache_started && updates)
        {
            XFlush(disp); cache_start(); cache_started=1;
        }
        /* loop again waiting for events */
    }
    return 0;
}
