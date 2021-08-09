// this code is not optimal in many ways, but works just nicely.
// Licence: GNU GPL v3
// Authors: Tomas Matejicek <www.slax.org>
//          Peter Munch-Ellingsen <www.peterme.net>
const int VERSION_MAJOR = 4; // Major version, changes when breaking backwards compatability
const int VERSION_MINOR = 7; // Minor version, changes when new functionality is added
const int VERSION_PATCH = 0; // Patch version, changes when something is changed without changing deliberate functionality (eg. a bugfix or an optimisation)

#define _GNU_SOURCE
/* open and O_RDWR,O_CREAT */
#include <fcntl.h>
/* include X11 stuff */
#include <X11/Xlib.h>
/* include Imlib2 stuff */
#include <Imlib2.h>
/* basename include */
#include <libgen.h>
/* sprintf include */
#include <stdio.h>
/* strcpy include */
#include <string.h>
/* exit include */
#include <stdlib.h>
/* exec include */
#include <unistd.h>
/* long options include*/
#include <getopt.h>
/* cursor */
#include <X11/cursorfont.h>
/* X utils */
#include <X11/Xutil.h>
#include <X11/Xatom.h>
/* parse commandline arguments */
#include <ctype.h>
/* one instance */
#include <sys/file.h>
#include <sys/stat.h>
/* check stdin */
#include <sys/poll.h>
#include <errno.h>

/* some globals for our window & X display */
Display *disp;
Window   win;
XVisualInfo vinfo;
XSetWindowAttributes attr;
int x11_fd;
struct pollfd eventfds[2];

XIM im;
XIC ic;

int screen;
int screen_width;
int screen_height;

// Let's define a linked list node:

typedef struct node
{
    char title[256];
    char icon[256];
    char cmd[512];
    int hovered;
    int clicked;
    int hidden;
    int x;
    int y;
    struct node * next;
} node_t;

typedef struct button
{
    char icon_normal[256];
    char icon_highlight[256];
    char cmd[512];
    int hovered;
    int clicked;
    int x;
    int y;
    int w;
    int h;
    struct button * next;
} button_t;
typedef struct shortcut
{
  char *key;
  node_t *entry;
  struct shortcut *next;
} shortcut_t;

typedef struct keynode {
    char key[255];
    struct keynode * next;
} keynode_t;

typedef struct color {
    int r, g, b, a;
} color_t;

typedef struct percentable {
    int percent;
    int value;
} percentable_t;

int entries_count = 0;
node_t * entries = NULL;
button_t * buttons = NULL;
shortcut_t * shortcuts = NULL;
keynode_t * cmdline = NULL;

enum exit_code {
    OKAY,
    ESCAPE = 0x20,
    RIGHTCLICK,
    VOIDCLICK,
    FOCUSLOST,
    INTERNALCMD,
    LOCKERROR = 0x40,
    ALLOCERROR,
    FONTERROR,
    CONFIGERROR,
    WINERROR,
    LOCALEERROR,
    INPUTMERROR,
    INPUTCERROR,
    POLLERROR,
    EXTERNALERROR
};

static struct option long_options[] =
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
char * window_icon;
int bg_fill = 0;
int no_prompt = 0;
int no_title = 0;
int prompt_spacing = 48;
int windowed = 0;
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
int scrolled_past= 0;
int hovered_entry = 0;
color_t text_color = {.r = 255, .g = 255, .b = 255, .a = 255};
color_t prompt_color = {.r = 255, .g = 255, .b = 255, .a = 255};
color_t background_color = {.r = 46, .g = 52, .b = 64, .a = 255};
color_t shadow_color = {.r = 0, .g = 0, .b = 0, .a = 30};
int background_color_set = 0;
color_t highlight_color = {.r = 255, .g = 255, .b = 255, .a = 50};
color_t scrollbar_color = {.r = 255, .g = 255, .b = 255, .a = 60};
color_t scrollindicator_color = {.r = 255, .g = 255, .b = 255, .a = 112};

#define MOUSE 1
#define KEYBOARD 2
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

void calculate_percentage(int maxvalue, percentable_t* percentable)
{
    if(percentable->percent != -1) {
        percentable->value = (maxvalue * percentable->percent) / 100;
    }
}

void recalc_cells()
{
    int margined_cell_width, margined_cell_height;

    if (text_after){
        cell_width=icon_size+icon_padding*2;
        cell_height=icon_size+icon_v_padding*2;
        margined_cell_width=icon_size+icon_padding*2+least_margin;
        margined_cell_height=icon_size+icon_v_padding*2+least_v_margin;
        if(ucolumns == 0)
            ucolumns = 1;
    } else {
        cell_width=icon_size+icon_padding*2;
        cell_height=icon_size+icon_v_padding*2+font_height+text_padding;
        margined_cell_width=icon_size+icon_padding*2+least_margin;
        margined_cell_height=icon_size+icon_v_padding*2+font_height+text_padding+least_v_margin;
    }

    border = screen_width/10;
    if (uborder.value > 0) border = uborder.value;
    if (uborder.value == -1 && uborder.percent == -1){
        if (ucolumns == 0){
            border = 0;
        } else {
            side_border = (screen_width - (ucolumns * cell_width + (ucolumns - 1) * least_margin))/2;
            border = (screen_height - prompt_font_height - prompt_spacing - (urows * cell_height + (urows - 1) * least_v_margin))/2;
        }
    }

    if (uside_border.value == 0 && uside_border.percent == -1) side_border = border;
    if (uside_border.value > 0) side_border = uside_border.value;
    if (uside_border.value == -1 && uside_border.percent == -1){
        if (ucolumns == 0){
            side_border = 0;
        } else {
            side_border = (screen_width - (ucolumns * cell_width + (ucolumns - 1) * least_margin))/2;
        }
    }

    int usable_width;
    int usable_height;
    // These do while loops should run at most three times, it's just to avoid copying code
    do {
        usable_width = screen_width-side_border*2;
        usable_height = screen_height-border*2;
        usable_height = screen_height-border*2-prompt_spacing-prompt_font_height;

        // If the usable_width is too small, take some space away from the border
        if (usable_width < cell_width) {
            side_border = (screen_width - cell_width - 1)/2;
        } else if (usable_height < cell_height) {
            border = (screen_height - cell_height - prompt_spacing - prompt_font_height - 1)/2;
        }

    } while ((usable_width < cell_width && screen_width > cell_width) || (usable_height < cell_height && screen_height > cell_height));

    // just in case, make sure border is never negative
    if (border < 0) border = 0;
    if (side_border < 0) side_border = 0;

    // If columns were not manually overriden, calculate the most it can possibly contain
    if (ucolumns == 0){
        columns = usable_width/margined_cell_width;
    } else{
        columns = ucolumns;
    }
    if (urows == 0){
        rows = usable_height/margined_cell_height;
    } else{
        rows = urows;
    }

    // If we don't have space for a single column or row, force it.
    if (columns <= 0) {
        columns = 1;
    }
    if (rows <= 0 ) {
        rows = 1;
    }

    if (text_after){
        cell_width = (usable_width - least_margin*(columns - 1))/columns;
    }

    // The space between the icon tiles to fill all the space
    if (columns == 1){
        column_margin = (usable_width - cell_width*columns);
    } else {
        column_margin = (usable_width - cell_width*columns)/(columns - 1);
    }
    if (rows == 1){
        row_margin = (usable_height - cell_height*rows);
    } else {
        row_margin = (usable_height - cell_height*rows)/(rows - 1);
    }

    // These are kept in case manual positioning is reintroduced
    prompt_x = (side_border * side_border_ratio) / 50;
    prompt_y = (border * border_ratio) / 50;
    /*
    if(uside_border == 0){
        side_border = border;
    }*/
}


void restack()
{
    if (desktop_mode) XLowerWindow(disp,win);
    else if (!windowed) XRaiseWindow(disp,win);
}


char* strncpyutf8(char* dst, const char* src, size_t num)
{
    if(num)
    {
        size_t sizeSrc = strlen(src); // number of bytes not including null
        while(sizeSrc>num)
        {
            const char* lastByte = src + sizeSrc; // initially \0 at end

            while(lastByte-- > src) // test previous chars
                if((*lastByte & 0xC0) != 0x80) // utf8 start byte found
                    break;

            sizeSrc = lastByte-src;
        }
        memcpy(dst, src, sizeSrc);
        dst[sizeSrc] = '\0';
    }
    return dst;
}


void arrange_positions()
{
    node_t * current = entries;
    int i = 0;
    int j = 0;

    while (current != NULL)
    {
        if (current->hidden)
        {
            current->x = 0;
            current->y = 0;
        }
        else
        {
            int entries_last_line = entries_count - j * columns;
            if (center_icons && entries_last_line < columns) {
                int width = entries_last_line * cell_width + (entries_last_line - 1) * column_margin;
                int usable_width = screen_width - side_border * 2;
                int margin = usable_width - width;
                current->x = (side_border * side_border_ratio) / 50 + margin / 2 + i * (cell_width + column_margin);
            } else {
                current->x = (side_border * side_border_ratio) / 50 + i * (cell_width+column_margin);
            }
            current->y = (border * border_ratio) / 50 + prompt_font_height + prompt_spacing + (j - scrolled_past) * (cell_height+row_margin);
            if (upside_down) {
                current->y=screen_height - cell_height - current->y;
            }
            if (i == columns-1) {
                i = 0;
                j++;
            } else {
                i++;
            }
        }
        current = current->next;
    }
}


void push_key(char * key)
{
    keynode_t * current = cmdline;

    if (current==NULL)
    {
        cmdline = malloc(sizeof(keynode_t));
        strcpy(cmdline->key,key);
        cmdline->next = NULL;
        return;
    }

    while (current->next != NULL) {
        current = current->next;
    }

    /* now we can add a new item */
    current->next = malloc(sizeof(keynode_t));
    strcpy(current->next->key,key);
    current->next->next = NULL;
}


void pop_key()
{
    if (cmdline==NULL) return;

    // if there is only one item, remove it
    if (cmdline->next==NULL)
    {
        free(cmdline);
        cmdline=NULL;
        return;
    }

    keynode_t * current = cmdline;

    while (current->next->next != NULL) {
        current = current->next;
    }

    /* now we can remove last item */
    free(current->next);
    current->next=NULL;
}


void clear_entries(){
    node_t * current = entries;
    while (current != NULL) {
        node_t * last = current;
        if (clear_memory) {
            memset(last->title, 0, 256);
            memset(last->icon, 0, 256);
            memset(last->cmd, 0, 512);
            // Free the entire struct, except for it's next pointer
            memset(last, 0, sizeof(node_t) - sizeof(node_t *));
        }
        free(last);
        current = current->next;
    }
    entries = NULL;
}


void cleanup()
{
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


Imlib_Image load_image(char * icon) {
    Imlib_Load_Error load_error;
    Imlib_Image image = imlib_load_image_with_error_return(icon, &load_error);
    if(image) {
        imlib_context_set_image(image);
        imlib_free_image();
    } else {
        fprintf(stderr, "Could not load icon %s, Imlib failed with: ", icon);
        switch(load_error) {
            case IMLIB_LOAD_ERROR_NONE:
                fprintf(stderr, "IMLIB_LOAD_ERROR_NONE");
                break;
            case IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST:
                fprintf(stderr, "IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST");
                break;
            case IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY:
                fprintf(stderr, "IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY");
                break;
            case IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_READ:
                fprintf(stderr, "IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_READ");
                break;
            case IMLIB_LOAD_ERROR_NO_LOADER_FOR_FILE_FORMAT:
                fprintf(stderr, "IMLIB_LOAD_ERROR_NO_LOADER_FOR_FILE_FORMAT");
                break;
            case IMLIB_LOAD_ERROR_PATH_TOO_LONG:
                fprintf(stderr, "IMLIB_LOAD_ERROR_PATH_TOO_LONG");
                break;
            case IMLIB_LOAD_ERROR_PATH_COMPONENT_NON_EXISTANT:
                fprintf(stderr, "IMLIB_LOAD_ERROR_PATH_COMPONENT_NON_EXISTANT");
                break;
            case IMLIB_LOAD_ERROR_PATH_COMPONENT_NOT_DIRECTORY:
                fprintf(stderr, "IMLIB_LOAD_ERROR_PATH_COMPONENT_NOT_DIRECTORY");
                break;
            case IMLIB_LOAD_ERROR_PATH_POINTS_OUTSIDE_ADDRESS_SPACE:
                fprintf(stderr, "IMLIB_LOAD_ERROR_PATH_POINTS_OUTSIDE_ADDRESS_SPACE");
                break;
            case IMLIB_LOAD_ERROR_TOO_MANY_SYMBOLIC_LINKS:
                fprintf(stderr, "IMLIB_LOAD_ERROR_TOO_MANY_SYMBOLIC_LINKS");
                break;
            case IMLIB_LOAD_ERROR_OUT_OF_MEMORY:
                fprintf(stderr, "IMLIB_LOAD_ERROR_OUT_OF_MEMORY");
                break;
            case IMLIB_LOAD_ERROR_OUT_OF_FILE_DESCRIPTORS:
                fprintf(stderr, "IMLIB_LOAD_ERROR_OUT_OF_FILE_DESCRIPTORS");
                break;
            case IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_WRITE:
                fprintf(stderr, "IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_WRITE");
                break;
            case IMLIB_LOAD_ERROR_OUT_OF_DISK_SPACE:
                fprintf(stderr, "IMLIB_LOAD_ERROR_OUT_OF_DISK_SPACE");
                break;
            case IMLIB_LOAD_ERROR_UNKNOWN:
                fprintf(stderr, "IMLIB_LOAD_ERROR_UNKNOWN");
                break;
        }
        fprintf(stderr, "\n");
        /*
        cleanup();
        exit(1);*/
    }
    return image;
}


void push_entry(node_t * new_entry)//(char * title, char * icon, char * cmd, int x, int y)
{
    node_t * current = entries;
    int hasicon = (strlen(new_entry->icon) != 0);
    /* Pre-load the image into the cache, this is done to check for error messages
     * If a user has more images then can be shown this might incur a performance hit */
    if (hasicon) {
        Imlib_Image image = load_image(new_entry->icon);
        if (image == NULL) {
            if (hide_missing) return;
            strcpy(new_entry->icon, "/usr/share/icons/hicolor/48x48/apps/xlunch_ghost.png");
        }
    }

    // Set default values for internal state
    new_entry->x=0;
    new_entry->y=0;
    new_entry->hidden=0;
    new_entry->hovered=0;
    new_entry->clicked=0;
    new_entry->next = NULL;

    shortcut_t *shortcut = shortcuts;
    while (shortcut != NULL) {
        if (shortcut->entry == NULL) {
            shortcut->entry = new_entry;
            break;
        }
        shortcut = shortcut->next;
    }

    // empty list, add first directly
    if (current==NULL)
    {
        entries = new_entry;
        entries->next = NULL;
        return;
    }

    // Otherwise determine position
    if(!reverse){
        // Go to end of list and add there
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_entry;
        current->next->next = NULL;
    } else {
        // Add at head of list
        new_entry->next = entries;
        entries = new_entry;
    }
    entries_count++;
}


char *strtok_new(char * string, char const * delimiter){
   static char *source = NULL;
   char *p, *riturn = 0;
   if(string != NULL)         source = string;
   if(source == NULL)         return NULL;

   if((p = strpbrk (source, delimiter)) != NULL) {
      *p  = 0;
      riturn = source;
      source = ++p;
   }
    return riturn;
}


char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the zero-terminator
    if(result != 0) {
        strcpy(result, s1);
        strcat(result, s2);
        return result;
    }
    exit(ALLOCERROR);
}


FILE * determine_input_source(){
    FILE * fp;
    if(input_source == NULL) {
        char * homeconf = NULL;

        char * home = getenv("HOME");
        if (home!=NULL)
        {
            homeconf = concat(home,"/.config/xlunch/entries.dsv");
        }

        if (strlen(input_file)==0){
            fp = stdin;
            int flags;
            int fd = fileno(fp);
            flags = fcntl(fd, F_GETFL, 0);
            flags |= O_NONBLOCK;
            fcntl(fd, F_SETFL, flags);
            struct pollfd fds;
            fds.fd = 0; /* this is STDIN */
            fds.events = POLLIN;
            // Give poll a little timeout to make give the piping program some time
            if (poll(&fds, 1, 10) == 0){
                int flags = fcntl(fd, F_GETFL, 0);
                flags &= ~O_NONBLOCK;
                fcntl(fd, F_SETFL, flags);
                fclose(stdin);
                fp = fopen(homeconf, "rb");
            }
        } else {
            fp = fopen(input_file, "rb");
        }
        if (fp == NULL)
        {
            fprintf(stderr, "Error getting entries from %s.\nReverting back to system entries list.\n", strlen(input_file) == 0 ? "stdin" : input_file);
            input_file = "/etc/xlunch/entries.dsv";
            fp = fopen(input_file, "rb");

            if (fp == NULL)
            {
                fprintf(stderr, "Error opening entries file %s\n", input_file);
                fprintf(stderr, "You may need to create it. Icon file format is following:\n");
                fprintf(stderr, "title;icon_path;command\n");
                fprintf(stderr, "title;icon_path;command\n");
                fprintf(stderr, "title;icon_path;command\n");
            }
        }
        free(homeconf);
    } else {
        fp = input_source;
    }
    return fp;
}

int mouse_over_cell(node_t * cell, int index, int mouse_x, int mouse_y)
{
    if (cell->hidden) return 0;

    if (index > scrolled_past*columns && index < scrolled_past*columns + rows*columns +1
        && mouse_x >= cell->x
        && mouse_x < cell->x+cell_width
        && mouse_y >= cell->y
        && mouse_y < cell->y+cell_height) return 1;
    else return 0;
}

int mouse_over_button(button_t * button, int mouse_x, int mouse_y)
{
    int x = (button->x < 0 ? screen_width + button->x + 1 - button->w : button->x);
    int y = (button->y < 0 ? screen_height + button->y + 1 - button->h : button->y);
    if (   mouse_x >= x
        && mouse_x < x+button->w
        && mouse_y >= y
        && mouse_y < y+button->h) return 1;
    else return 0;
}

Pixmap GetRootPixmap(Display* display, Window *root)
{
    Pixmap currentRootPixmap;
    Atom act_type;
    int act_format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;
    Atom _XROOTPMAP_ID;

    _XROOTPMAP_ID = XInternAtom(display, "_XROOTPMAP_ID", False);

    if (XGetWindowProperty(display, *root, _XROOTPMAP_ID, 0, 1, False,
                           XA_PIXMAP, &act_type, &act_format, &nitems, &bytes_after,
                           &data) == Success) {

        if (data) {
            currentRootPixmap = *((Pixmap *) data);
            XFree(data);
        }
    }

    return currentRootPixmap;
}

int get_root_image_to_imlib_data(DATA32 * direct)
{
    int screen;
    Window root;
    Display* display;
    XWindowAttributes attrs;
    Pixmap bg;
    XImage* img;

    display = XOpenDisplay(NULL);
    if (display == NULL) return 0;

    screen = DefaultScreen(display);
    root = RootWindow(display, screen);
    XGetWindowAttributes(display, root, &attrs);

    bg = GetRootPixmap(display, &root);
    img = XGetImage(display, bg, 0, 0, attrs.width, attrs.height, ~0, ZPixmap);
    XFreePixmap(display, bg);

    if (!img) {
        XCloseDisplay(display);
        return 0;
    }

    unsigned long pixel;
    int x, y;

    for (y = 0; y < img->height; y++)
    {
        for (x = 0; x < img->width; x++)
        {
            pixel = XGetPixel(img,x,y);
            direct[y*img->width+x+0] = 0xffffffff&pixel;
        }
    }

    XDestroyImage(img);
    return 1;
}


void joincmdline()
{
    strcpy(commandline,"");

    keynode_t * current = cmdline;

    while (current != NULL) {
        strcat(commandline,current->key);
        current = current->next;
    }
}

void joincmdlinetext()
{
    if (no_prompt) return;
    if (strlen(prompt)==0) prompt="Run:  ";
    strcpy(commandlinetext,prompt);

    keynode_t * current = cmdline;

    while (current != NULL) {
        strcat(commandlinetext,current->key);
        current = current->next;
    }

    strcat(commandlinetext,"_");
}

void set_scroll_level(int new_scroll) {
    if (!noscroll){
        if (new_scroll != scrolled_past) {
            scrolled_past = new_scroll;
            if (scrolled_past > (entries_count - 1)/columns - rows + 1) {
                scrolled_past = (entries_count - 1)/columns - rows + 1;
            }
            if (scrolled_past < 0) {
                scrolled_past = 0;
            }
            arrange_positions();
            updates = imlib_update_append_rect(updates, 0, 0, screen_width, screen_height);
        }
    }
}

void set_hover(int hovered, node_t * cell, int hover)
{
    if (hover) hovered_entry = hovered;
    if (cell==NULL) return;
    if (cell->hidden) return;
    if (cell->hovered!=hover) updates = imlib_update_append_rect(updates, cell->x, cell->y, cell_width, cell_height);
    cell->hovered=hover;
}

void hover_entry(int entry){
    hoverset = KEYBOARD;
    int to_row = (entry+columns-1)/columns;
    int display_row = (hovered_entry-(scrolled_past*columns)+columns-1)/columns;
    if (entry>(columns*rows)+scrolled_past*columns || entry<=scrolled_past*columns) {
        set_scroll_level(to_row - display_row);
    }
    int j = 1;
    int max = scrolled_past*columns + columns*rows;
    max = (max > entries_count ? entries_count : max);
    int i = (entry < scrolled_past*columns + 1
            ? scrolled_past*columns + 1
            : (entry > max
                ? max
                : entry));
    node_t * current = entries;
    while (current != NULL) {
        if (j == i) set_hover(i, current, 1);
        else        set_hover(i, current, 0);
        if (!current->hidden) j++;
        current = current->next;
    }
}

void filter_entries()
{
    node_t * current = entries;
    entries_count = 0;

    while (current != NULL)
    {
        if (strlen(commandline)==0 || strcasestr(current->title,commandline)!=NULL || strcasestr(current->cmd,commandline)!=NULL){
            current->hidden=0;
            entries_count ++;
        } else {
            current->hidden=1;
            current->clicked=0;
            set_hover(0, current, 1);
        }
        current=current->next;
    }
    set_scroll_level(0);
}


int starts_with(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? 0 : strncmp(pre, str, lenpre) == 0;
}

void run_command(char * cmd_orig);
void run_internal_command(char * cmd_orig) {
    int will_quit = 0;
    char * cmd = malloc(strlen(cmd_orig)+1);
    memcpy(cmd, cmd_orig, strlen(cmd_orig)+1);
    char *p = strtok( cmd, " ");
    if (strcmp(p, "scroll") == 0) {
        p = strtok( NULL, " ");
        if (strcmp(p, "top") == 0){
            set_scroll_level(0);
        } else if (strcmp(p, "bottom") == 0){
            set_scroll_level(entries_count);
        } else {
            if (p[0] == '+' || p[0] == '-')
                set_scroll_level(scrolled_past + atoi(p));
            else
                set_scroll_level(atoi(p));
        }
    } else if (strcmp(p, "hover") == 0) {
        if (hoverset != KEYBOARD) hovered_entry = 0;
        p = strtok( NULL, " ");
        int new_hover;
        if (strcmp(p, "start") == 0){
            new_hover = 1;
        } else if (strcmp(p, "end") == 0){
            new_hover = entries_count;
        } else {
            if (p[0] == '+' || p[0] == '-')
                new_hover = hovered_entry + atoi(p);
            else
                new_hover = atoi(p);
        }
        hover_entry(new_hover);
    } else if (strcmp(p, "exec") == 0) {
        int old_oo = output_only;
        output_only = 0;
        run_command(strtok(NULL, "\""));
        output_only = old_oo;
    } else if (strcmp(p, "print") == 0){
        printf("%s\n", strtok(NULL, "\""));
    } else if (strcmp(p, "quit") == 0) {
        will_quit = 1;
    }
    if (p != NULL) {
        p = strtok(NULL, "");
        if (p != NULL) {
            if (p[0] == ':') {
                run_internal_command(&p[1]);
            } else {
                fprintf(stderr, "Internal command \"%s\" supplied with extraneous parameters \"%s\"\n", cmd_orig, p);
            }
        }
    }
    free(cmd);
    if (will_quit) {
        cleanup();
        exit(INTERNALCMD);
    }
}

void reset_prompt()
{
    while (cmdline != NULL) pop_key();
    joincmdline();
    joincmdlinetext();
    filter_entries();
    arrange_positions();
    node_t * current = entries;
    while (current != NULL) {
        set_hover(0, current, 0);
        current = current->next;
    }
    updates = imlib_update_append_rect(updates, 0, 0, screen_width, screen_height);
}

void run_command(char * cmd_orig)
{
    char *cmd;
    char *array[100] = {0};

    cmd = strdup(cmd_orig);
    if (!cmd) {
        fprintf(stderr, "Out of memory!\n");
        exit(ALLOCERROR);
    }

    int isrecur = starts_with(":recur ", cmd_orig) || (strcmp(":recur", cmd_orig) == 0);
    if(isrecur) {
        // split arguments into pieces
        int i = 0;
        // If we recur (ie. start xlunch again) run the command that was used to run xlunch
        array[0] = program_name;

        // Blindly consume the first token which should be "recur"
        char *p = strtok (cmd, " ");
        p = strtok (NULL, " ");
        while (p != NULL)
        {
            array[++i]=p;
            p = strtok (NULL, " ");
            if (i>=99) break;
        }
    } else {
        if (cmd_orig[0] == ':') {
            run_internal_command((char *)(cmd_orig + sizeof(char)));
            return;
        }
        if(output_only){
            fprintf(stdout, "%s\n", cmd_orig);
            if(!dont_quit){
                cleanup();
                exit(OKAY);
            } else {
                reset_prompt();
                return;
            }
        }
        array[0] = "/bin/sh";
        array[1] = "-c";
        array[2] = cmd_orig;
        array[3] = NULL;
    }

    restack();

    if (dont_quit)
    {
        pid_t pid=fork();

        switch (pid) {
        case 0:   /* Child process */
            if (!multiple_instances) close(lock);
            printf("Forking command: %s\n",cmd);
            break;

        case -1:  /* Error */
            perror("fork");
            /*FALLTHROUGH*/

        default:  /* Parent */
            reset_prompt();
            if (cmd != cmd_orig)
                free(cmd);
            return;
        }
    }
    else
    {
        cleanup();
        printf("Running command: %s\n",cmd);
    }

    execvp(array[0],array);
    fprintf(stderr,"Error running '%s -c \"%s\"': %s\n", array[0], array[2],
	strerror(errno));
    exit(EXTERNALERROR);
}

int parse_entries()
{
    int changed = 0; // if the list of elements have changed or not
    int parsing = 0; // section currently being read
    int position = 0; // position in the current entry
    int leading_space = 0;
    int line = 1; // line count for error messages
    int readstatus;

    struct pollfd fds;
    fds.fd = fileno(input_source);
    fds.events = POLLIN;
    node_t * current_entry = NULL;
    char internal_command[128];
    while(poll(&fds, 1, 0)) {
        char b;
        readstatus = read(fds.fd, &b, 1);
        if(readstatus <= 0){
            break;
        }
        if(parsing == 0 && position == leading_space){
            if(b != ':' && b != '#') {
                current_entry = malloc(sizeof(node_t));
            } else {
                current_entry = NULL;
                if(b == '#'){
                    parsing = -1;
                }
            }
        }
        if (current_entry == NULL) {
            if (b == '\n') b = '\0';
            if (position > leading_space && parsing != -1)
                internal_command[position-leading_space-1] = b;
        } else {
            if(b == '\0') {
                fprintf(stderr, "Null-byte encountered while reading entries at line %d. Aborting.\n", line);
            }
            // Check for semi-colons only for the first two elements to support bash commands with semi-colons in them
            if(b == ';' && parsing != 2) {
                b = '\0';
            } else if (b == '\n') {
                line++;
                b = '\0';
                if(parsing == 0){
                    clear_entries();
                    changed = 1;
                    continue;
                }
            }
            if(b == ' ' && position <= leading_space) leading_space++;
            if(b != ' ' && leading_space > 0) leading_space = -leading_space;
            switch(parsing){
                case 0:
                    if (leading_space <= 0)
                        current_entry->title[position+leading_space] = b;
                    break;
                case 1:
                    current_entry->icon[position] = b;
                    break;
                case 2:
                    current_entry->cmd[position] = b;
                    break;
            }
        }
        position++;
        if(b == '\0') {
            position = 0;
            leading_space = 0;
            if(parsing == 2 || current_entry == NULL) {
                if(current_entry != NULL) {
                    push_entry(current_entry);
                    changed = 1;
                } else {
                    if (parsing != -1)
                        run_internal_command(internal_command);
                }
                parsing = 0;
            } else {
                parsing++;
            }
        }
        if(current_entry != NULL) {
            int maxlen = (parsing == 2 ? 511 : 255);
            if(position == maxlen){
                fprintf(stderr, "Entry too long on line %d, maximum length is %d characters!\n", line, maxlen);
                break;
            }
        }
    }
    if(readstatus == 0){
        if(parsing == 2){
            current_entry->cmd[position]='\0';
            push_entry(current_entry);
            changed = 1;
        }
        close(fds.fd);
        input_source = NULL;
    }
    if(changed) {
        filter_entries();
        arrange_positions();
    }
    return changed;
}

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

void set_clicked(node_t * cell, int clicked)
{
    if (cell==NULL) return;
    if (cell->hidden) return;
    if (cell->clicked!=clicked) updates = imlib_update_append_rect(updates, cell->x, cell->y, cell_width, cell_height);
    cell->clicked=clicked;
}

Imlib_Font load_default_font(){
    // No font was specified, try to load some default options as a fallback
    Imlib_Font font;
    font = imlib_load_font("OpenSans-Regular/10");
    if (!font) font = imlib_load_font("DejaVuSans/10");
    return font;
}

int get_font_height(Imlib_Font font){
    imlib_context_set_font(font);
    // maximum font descent is relative to baseline (ie. negative)
    int height = imlib_get_maximum_font_ascent() - imlib_get_maximum_font_descent();
    imlib_free_font();
    return height;
}

Imlib_Font load_font()
{
    Imlib_Font font;
    if (strlen(font_name) == 0){
        font = load_default_font();
    } else {
        font = imlib_load_font(font_name);
    }
    if (font == NULL) {
        fprintf(stderr, "Font %s could not be loaded! Please specify one with -f parameter\n", font_name);
        exit(FONTERROR);
    }

    if (!no_title)
       font_height = get_font_height(font);

    return font;
}

Imlib_Font load_prompt_font()
{
    Imlib_Font font;
    if (strlen(prompt_font_name) == 0){
        if (strlen(font_name) == 0){
            font = load_default_font();
        } else {
            font = imlib_load_font(font_name);
        }
    } else {
        font = imlib_load_font(prompt_font_name);
    }
    if (font == NULL) {
        fprintf(stderr, "Prompt font %s could not be loaded! Please specify one with -F parameter\n", prompt_font_name);
        exit(FONTERROR);
    }

    if (!no_prompt)
       prompt_font_height = get_font_height(font);

    return font;
}


// set background image for desktop, optionally copy it from root window,
// and set background for highlighting items
//
void update_background_images()
{
    /* reset images if exist */
    if (background)
    {
        imlib_context_set_image(background);
        imlib_free_image();
    }

    if (highlight)
    {
        imlib_context_set_image(highlight);
        imlib_free_image();
    }

    // load highlighting image from a file
    if (strlen(highlight_file)>0)
       highlight=imlib_load_image(highlight_file);

    /* fill the window background */
    background = imlib_create_image(screen_width, screen_height);
    // When xlunch is launched and there is another full screen window 'background' was NULL
    if(background) {
        imlib_context_set_image(background);
        imlib_image_set_has_alpha(1);
        imlib_image_clear();

        if (use_root_img) {
            DATA32 * direct = imlib_image_get_data();
            int ok = get_root_image_to_imlib_data(direct);
            if (ok) {
                imlib_image_put_back_data(direct);
                imlib_context_set_color(background_color.r, background_color.g, background_color.b, background_color.a);
                imlib_context_set_blend(1);
                imlib_image_fill_rectangle(0,0, screen_width, screen_height);
                imlib_context_set_blend(0);
            }
        } else{ // load background from file
            if (strlen(background_file)>0) {
                image = imlib_load_image(background_file);
                if (image) {
                    imlib_context_set_image(image);
                    int w = imlib_image_get_width();
                    int h = imlib_image_get_height();
                    imlib_context_set_image(background);
                    imlib_context_set_color(background_color.r, background_color.g, background_color.b, background_color.a);
                    imlib_context_set_blend(1);

                    // Those are our source coordinates if we use just scale
                    // this would give the same result as feh --bg-scale
                    int imx=0, imy=0, imw=w, imh=h;

                    // But we do not want to use scale, rather we use fill to keep aspect ratio
                    // It gives the same result as feh --bg-fill
                    if (bg_fill) {
                        if ( (double) (w/h) < (double) (screen_width/screen_height) )
                        {
                           imw = (int) w;
                           imh = (int) (screen_height * w / screen_width);
                           imx = 0;
                           imy = (int) ((h - imh) / 2);
                        }
                        else
                        {
                           imw = (int) (screen_width * h / screen_height);
                           imh = (int) h;
                           imx = (int) ((w - imw) / 2);
                           imy = 0;
                        }
                    }

                    imlib_blend_image_onto_image(image, 1, imx, imy, imw, imh,  0, 0, screen_width, screen_height);
                    imlib_image_fill_rectangle(0,0, screen_width, screen_height);
                    imlib_context_set_blend(0);
                    imlib_context_set_image(image);
                    imlib_free_image();
                }
            } else {
                imlib_context_set_color(background_color.r, background_color.g, background_color.b, background_color.a);
                imlib_image_fill_rectangle(0,0, screen_width, screen_height);
            }
        }
    }
}

void draw_text_with_shadow(int posx, int posy, char * text, color_t color) {
    imlib_context_set_color(shadow_color.r, shadow_color.g, shadow_color.b, shadow_color.a);
    imlib_text_draw(posx +1, posy +1, text);
    imlib_text_draw(posx +1, posy +2, text);
    imlib_text_draw(posx +2, posy +2, text);

    imlib_context_set_color(color.r, color.g, color.b, color.a);
    imlib_text_draw(posx, posy, text);
}

void handle_option(int c, char *optarg);

void parse_config(FILE *input) {
    int readstatus;
    int position = 0;
    int size = 0;
    int eol = 0;
    int fileline = 1;
    char *optarg = NULL;
    char matching[(sizeof(long_options)/sizeof(struct option)) - 1];
    char *entries_word = "entries";
    int matching_entries = 1;
    int matched = '?';
    int comment = 0;
    memset(matching, 1, sizeof(long_options)/sizeof(struct option) - 1);

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
            for(int i = 0; i < sizeof(long_options)/sizeof(struct option) - 1; i++) {
                if (long_options[i].name[position] != b){
                    matching[i] = 0;
                }
            }
            if (entries_word[position] != b && eol != 1) {
                matching_entries = 0;
            }
            if(b == '\0') {
                for(int i = 0; i < sizeof(long_options)/sizeof(struct option) - 1; i++) {
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
            memset(matching, 1, sizeof(long_options)/sizeof(struct option) - 1);
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
                for(int i = 0; i < sizeof(long_options)/sizeof(struct option) - 1; i++) {
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
                            "        -H, --help                         Shows this help message\n"
                            "        --name                             POSIX-esque way to specify the first part of\n"
                            "                                           WM_CLASS (default: environment variable \n"
                            "                                           RESOURCE_NAME or argv[0])\n"
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
                            "                                           that can't fit the screen.\n\n"
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
                            "                                           (default: ffffff3c)\n"
                            "        --scrollindicatorcolor [color]     Color to use for the scrollbar indicator\n"
                            "                                           (default: ffffff70)\n\n");
            // Check if we came from the error block above or if this was a call with --help
            if(c == '?'){
                exit(CONFIGERROR);
            } else {
                exit(OKAY);
            }
            break;
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

    calculate_percentage(screen_height, &uborder);
    calculate_percentage(screen_width, &uside_border);
}

void recheckHover(XEvent ev) {
    node_t * current = entries;
    int i = 1;
    int j = 1;
    int any_hovered = 0;

    while (current != NULL)
    {
        if (mouse_moves>3 && mouse_over_cell(current, j, ev.xmotion.x, ev.xmotion.y)) {
            set_hover(i, current, 1);
            any_hovered = 1;
            hoverset=MOUSE;
        }
        else {
            set_hover(i, current,0);
            set_clicked(current,0);
        }
        if(!current->hidden)
            j++;
        i++;
        current = current->next;
    }
    if (any_hovered == 0) hovered_entry = 0;

    button_t * button = buttons;
    while (button != NULL) {
        int x = (button->x < 0 ? screen_width + button->x + 1 - button->w : button->x);
        int y = (button->y < 0 ? screen_height + button->y + 1 - button->h : button->y);
        if (mouse_over_button(button, ev.xmotion.x, ev.xmotion.y)) {
            if (button->hovered != 1) updates = imlib_update_append_rect(updates, x, y, button->w, button->h);
            button->hovered = 1;
        } else {
            if (button->hovered != 0) updates = imlib_update_append_rect(updates, x, y, button->w, button->h);
            button->hovered = 0;
            button->clicked = 0;
        }
        button = button->next;
    }
}

void handleButtonPress(XEvent ev) {
    switch (ev.xbutton.button) {
        case 3: // right click
            if (!desktop_mode) {
                cleanup();
                exit(RIGHTCLICK);
            }
            break;
        case 4: // scroll up
            set_scroll_level(scrolled_past - 1);
            recheckHover(ev);
            break;
        case 5: // scroll down
            set_scroll_level(scrolled_past + 1);
            recheckHover(ev);
            break;
        case 1:; // left click
            node_t * current = entries;
            int voidclicked = 1;
            int index = 1;
            mouse_moves += 4;
            recheckHover(ev);
            while (current != NULL)
            {
                if (mouse_over_cell(current, index, ev.xmotion.x, ev.xmotion.y)) {
                    set_clicked(current,1);
                    voidclicked = 0;
                }
                else set_clicked(current,0);
                if(!current->hidden)
                    index++;
                current = current->next;
            }

            button_t * button = buttons;
            while (button != NULL) {
                int x = (button->x < 0 ? screen_width + button->x + 1 - button->w : button->x);
                int y = (button->y < 0 ? screen_height + button->y + 1 - button->h : button->y);
                if (mouse_over_button(button, ev.xmotion.x, ev.xmotion.y)) {
                    if (button->clicked != 1) updates = imlib_update_append_rect(updates, x, y, button->w, button->h);
                    button->clicked = 1;
                    voidclicked = 0;
                } else {
                    if (button->clicked != 0) updates = imlib_update_append_rect(updates, x, y, button->w, button->h);
                    button->clicked = 0;
                }
                button = button->next;
            }

            if (voidclicked && void_click_terminate) {
                cleanup();
                exit(VOIDCLICK);
            }
            break;
    }
}

void handleButtonRelease(XEvent ev) {
    node_t * current = entries;
    int index = 1;

    while (current != NULL)
    {
        if (mouse_over_cell(current, index, ev.xmotion.x, ev.xmotion.y)) if (current->clicked==1) run_command(current->cmd);
        set_clicked(current, 0); // button release means all cells are not clicked
        updates = imlib_update_append_rect(updates, current->x, current->y, icon_size, icon_size);
        if(!current->hidden)
            index++;
        current = current->next;
    }

    button_t * button = buttons;

    while (button != NULL)
    {
        if (mouse_over_button(button, ev.xmotion.x, ev.xmotion.y) && button->clicked == 1) run_command(button->cmd);
        button->clicked = 0;
        int x = (button->x < 0 ? screen_width + button->x + 1 - button->w : button->x);
        int y = (button->y < 0 ? screen_height + button->y + 1 - button->h : button->y);
        updates = imlib_update_append_rect(updates, x, y, button->w, button->h);
        button = button->next;
    }
}

void handleKeyPress(XEvent ev) {
    // keyboard events
    int count = 0;
    KeySym keycode = 0;
    Status status = 0;
    char kbdbuf[20]= {0};
    count = Xutf8LookupString(ic, (XKeyPressedEvent*)&ev, kbdbuf, 20, &keycode, &status);

    if (keycode==XK_Escape && !desktop_mode)
    {
        cleanup();
        exit(ESCAPE);
    }

    if (keycode==XK_Return || keycode==XK_KP_Enter)
    {
        // if we have an icon hovered, and the hover was caused by keyboard arrows, run the hovered icon
        node_t * current = entries;
        node_t * selected = NULL;
        node_t * selected_one = NULL;
        node_t * first = NULL;
        int nb=0;
        while (current != NULL)
        {
            if (!current->hidden)
            {
                nb++;
                selected_one=current;
                if (first == NULL) first = current;
                if (current->hovered) selected = current;
            }
            current=current->next;
        }
        /* if only 1 app was filtered, consider it selected */
        if (nb==1 && selected_one!=NULL) run_command(selected_one->cmd);
        else if (hoverset==KEYBOARD && selected!=NULL) run_command(selected->cmd);
        // else run the command entered by commandline, if the command prompt is used
        else if (!no_prompt && !select_only) run_command(commandline);
        // or if --selectonly is specified, run first program regardless of selected state
        else if (select_only && first!=NULL) run_command(first->cmd);
    }

    if (keycode==XK_Tab || keycode==XK_Up || keycode==XK_Down || keycode==XK_Left || keycode==XK_Right
            || keycode==XK_KP_Up || keycode==XK_KP_Down || keycode==XK_KP_Left || keycode==XK_KP_Right
            || keycode==XK_Page_Down || keycode==XK_Page_Up || keycode==XK_Home || keycode==XK_End)
    {
        int i=0;
        if (keycode==XK_KP_Left || keycode==XK_Left) i=-1;
        if (keycode==XK_Up || keycode==XK_KP_Up) i=-columns;
        if (keycode==XK_Down || keycode==XK_KP_Down) i=columns;
        if (keycode==XK_Tab || keycode==XK_Right || keycode==XK_KP_Right) i=1;
        if (keycode==XK_Page_Up) i=-columns*rows;
        if (keycode==XK_Page_Down) i=columns*rows;
        if (keycode==XK_End) i = entries_count;//(scroll ? scrolled_past*columns+n : n);
        if (keycode==XK_Home) i = -entries_count;//(scroll ? scrolled_past*columns+1 : 1);
        if (hovered_entry == 0) {
            if (keycode != XK_End && keycode != XK_Page_Down) {
                hovered_entry = 1-i;
            } else {
                hovered_entry = 1;
            }
        }
        i = hovered_entry + i;
        hover_entry(i);
        return;
    }

    if (keycode==XK_Delete || keycode==XK_BackSpace)
        pop_key();
    else if (count>1 || (count==1 && kbdbuf[0]>=32)){ // ignore unprintable characterrs
        if (!no_prompt) push_key(kbdbuf);
        shortcut_t *shortcut = shortcuts;
        while(shortcut != NULL) {
            for(int i = 0; i < count; i++){
                if (kbdbuf[i] != shortcut->key[i]) {
                    break;
                } else if (i == count-1) {
                    run_command(shortcut->entry->cmd);
                }
            }
            shortcut = shortcut->next;
        }
    }

    joincmdline();
    joincmdlinetext();
    filter_entries();
    arrange_positions();

    // we used keyboard to type command. So unselect all icons.
    node_t * current = entries;
    int i = 1;
    while (current != NULL)
    {
        set_hover(i, current,0);
        set_clicked(current,0);
        current = current->next;
        i++;
    }

    updates = imlib_update_append_rect(updates, 0, 0, screen_width, screen_height);
}

void renderEntry(Imlib_Image buffer, char title[256], node_t * current, Cursor * c, int up_x, int up_y) {
    int h = 0;
    int w = 0;
    if (current->hovered)
    {
        if (hoverset == MOUSE) *c = XCreateFontCursor(disp,XC_hand1);

        if (highlight)
        {
            imlib_context_set_image(highlight);
            w = imlib_image_get_width();
            h = imlib_image_get_height();
            imlib_context_set_image(buffer);
            imlib_blend_image_onto_image(highlight, 1, 0, 0, w, h, current->x-up_x, current->y-up_y, cell_width, cell_height);
        }
        else
        {
           imlib_context_set_image(buffer);
           imlib_context_set_color(highlight_color.r, highlight_color.g, highlight_color.b, highlight_color.a);
           imlib_image_fill_rectangle(current->x-up_x, current->y-up_y, cell_width, cell_height);
        }
    }
    if (strlen(current->icon) != 0) {
        image=imlib_load_image(current->icon);
        if (image)
        {
            imlib_context_set_image(image);
            w = imlib_image_get_width();
            h = imlib_image_get_height();
            imlib_context_set_image(buffer);
            imlib_image_set_has_alpha(1);

            int d;
            if (current->clicked) d=2;
            else d=0;
            int x = current->x - up_x +
                        (text_other_side && text_after ? cell_width - icon_padding - icon_size : icon_padding)+d;
            int y = current->y - up_y +(text_other_side && !text_after ? cell_height - icon_v_padding - icon_size : icon_v_padding)+d;

            imlib_blend_image_onto_image(image, 1, 0, 0, w, h, x, y, icon_size-d*2, icon_size-d*2);

            imlib_context_set_image(image);
            imlib_free_image();
        }
    }
    /* draw text under icon */
    Imlib_Font font = load_font();
    if (font && !no_title)
    {
        imlib_context_set_image(buffer);
        int text_w;
        int text_h;

        const size_t osz = strlen(current->title);
        size_t sz = osz;
        imlib_context_set_font(font);
        do
        {
            strncpyutf8(title,current->title,sz);
            if(sz != osz)
                strcat(title,"..");
            imlib_get_text_size(title, &text_w, &text_h);
            sz--;
        } while(text_w > cell_width-(text_after ? (icon_size != 0 ? icon_padding*2 : icon_padding) + icon_size + text_padding : 2*text_padding) && sz>0);

        int d;
        if (current->clicked==1) d=4;
        else d=0;

        if (text_after) {
            draw_text_with_shadow(current->x - up_x + (text_other_side ? text_padding : (icon_size != 0 ? (padding_swap ? icon_padding*2 : icon_padding + text_padding) : icon_padding) + icon_size), current->y - up_y + cell_height/2 - font_height/2, title, text_color);
        } else {
            draw_text_with_shadow(current->x - up_x + cell_width/2 - text_w/2, current->y - up_y + (text_other_side ? text_padding : (padding_swap ? icon_v_padding*2 : icon_v_padding + text_padding) + icon_size), title, text_color);
        }

        /* free the font */
        imlib_free_font();
    }
}

/* the program... */
int main(int argc, char **argv){
    /* events we get from X */
    XEvent ev;
    /* our virtual framebuffer image we draw into */
    Imlib_Image buffer;
    char title[255];

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

    //win = XCreateSimpleWindow(disp, DefaultRootWindow(disp), uposx, uposy, screen_width, screen_height, 0, 0, 0);
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
    input_source = determine_input_source();
    if(input_source != NULL){
        parse_entries();
    }


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
    eventfds[0].events = POLLIN || POLLPRI || POLLOUT;
    if(input_source == stdin) {
        eventfds[1].fd = 0; /* this is STDIN */
        eventfds[1].events = POLLIN;
    }

    /* infinite event loop */
    for (;;)
    {
        /* init our updates to empty */
        updates = imlib_updates_init();

        // Poll for events, while blocking until one becomes available
        int poll_result;
        if(!XPending(disp)){
            poll_result = poll(eventfds, (input_source == stdin ? 2 : 1), -1);
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
                int changed = parse_entries(input_source);
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
                    case KeymapNotify:
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
                        mouse_moves++;
                        recheckHover(ev);
                        break;

                    default:
                        /* any other events - do nothing */
                        break;
                    }
                }
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
                int drawn = 0;
                int seen = 0;
                Cursor c = XCreateFontCursor(disp,XC_top_left_arrow);

                while (current != NULL)
                {
                    if (!current->hidden)
                    {
                        if (seen++ < scrolled_past * columns) {
                            current = current->next;
                            continue;
                        }
                        renderEntry(buffer, title, current, &c, up_x, up_y);
                        if (++drawn == columns*rows)
                            break;
                    }
                    current = current->next;
                }

                button_t *button = buttons;
                while (button != NULL) {
                    if(button->hovered) c = XCreateFontCursor(disp,XC_hand1);
                    image = imlib_load_image(button->hovered ? (button->icon_highlight[0] == '\0' ? button->icon_normal : button->icon_highlight) : button->icon_normal);
                    if (image)
                    {
                        imlib_context_set_image(buffer);
                        imlib_image_set_has_alpha(0);

                        int d;
                        if (button->clicked) d=2;
                        else d=0;
                        int x = (button->x < 0 ? screen_width + button->x + 1 - button->w : button->x);
                        int y = (button->y < 0 ? screen_height + button->y + 1 - button->h : button->y);

                        imlib_image_copy_alpha_to_image(image,x - up_x + d, y - up_y + d);
                        imlib_blend_image_onto_image(image, 1, 0, 0, button->w, button->h, x - up_x + d, y - up_y + d, button->w-d*2, button->h-d*2);

                        imlib_context_set_image(image);
                        imlib_free_image();
                    }

                    button = button->next;
                }

                XDefineCursor(disp,win,c);

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


                /* draw scrollbar, currently not draggable, only indication of scroll */
                if (!noscroll)
                {
                   int scrollbar_width=15; // width of entire scrollbar
                   int scrollbar_height=screen_height-2*border; // height of entire scrollbar
                   int scrollbar_screen_margin=50; // distance from screen edge
                   int pages = (entries_count - 1) / rows / columns + 1; // total pages to scroll, round up, min 1

                   // show scrollbar only if there are more pages
                   if (pages > 1)
                   {
                      int scrollbar_draggable_height = scrollbar_height * rows * columns / (entries_count+columns - ((entries_count+columns-1)%columns)); // height of scrollbar page (draggable), round to whole rows
                      float p = (entries_count - 1) / columns + 1 - rows; // current scrolled percentage
                      int scrollbar_draggable_shift = p ? (scrollbar_height - scrollbar_draggable_height) * scrolled_past / p : 0;

                      imlib_context_set_blend(1);

                      // draw scrollbar background on full height
                      imlib_context_set_color(scrollbar_color.r, scrollbar_color.g, scrollbar_color.b, scrollbar_color.a);
                      imlib_image_fill_rectangle(screen_width - scrollbar_screen_margin, border, scrollbar_width, scrollbar_height);

                      // draw current scroll position
                      imlib_context_set_color(scrollindicator_color.r, scrollindicator_color.g, scrollindicator_color.b, scrollindicator_color.a);
                      imlib_image_fill_rectangle(screen_width - scrollbar_screen_margin, border + scrollbar_draggable_shift, scrollbar_width, scrollbar_draggable_height);
                      imlib_context_set_blend(0);
                   }
                }

                /* don't blend the image onto the drawable - slower */
                imlib_context_set_blend(0);
                /* render the image at 0, 0 */
                imlib_render_image_on_drawable(up_x, up_y);
                /* don't need that temporary buffer image anymore */
                imlib_free_image();
            }
        }
        /* if we had updates - free them */
        if (updates)
            imlib_updates_free(updates);
        /* loop again waiting for events */
    }
    return 0;
}

