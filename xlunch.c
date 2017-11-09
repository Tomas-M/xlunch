// this code is not optimal in many ways, but works just nicely.
// Licence: GNU GPL v3
// Author: Tomas M <www.slax.org>

const int VERSION_MAJOR = 3; // Major version, changes when breaking backwards compatability
const int VERSION_MINOR = 1; // Minor version, changes when new functionality is added
const int VERSION_PATCH = 1; // Patch version, changes when something is changed without changing deliberate functionality (eg. a bugfix or an optimisation)

#define _GNU_SOURCE
/* open and O_RDWR,O_CREAT */
#include <fcntl.h>
/* include X11 stuff */
#include <X11/Xlib.h>
/* include Imlib2 stuff */
#include <Imlib2.h>
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


typedef struct keynode {
    char key[255];
    struct keynode * next;
} keynode_t;

typedef struct color {
    int r, g, b, a;
} color_t;

node_t * entries = NULL;
keynode_t * cmdline = NULL;

int icon_size = 48;
int ucolumns = 0;
int columns;
int urows = 0;
int rows;
int column_margin = 0;
int row_margin = 0;
int icon_padding = 10;
int text_padding = 10;
int border;
int side_border;
int cell_width;
int cell_height;
int font_height;
int prompt_font_height;
int use_root_img = 0;
char commandline[10024];
char commandlinetext[10024];
int prompt_x;
int prompt_y;
char * background_file = "";
char * highlight_file = "";
char * input_file = "";
FILE * input_source = NULL;
char * prompt = "";
char * font_name = "";
char * prompt_font_name = "";
char * program_name;
int no_prompt = 0;
int prompt_spacing = 48;
int windowed = 0;
int multiple_instances = 0;
int uposx = 0;
int uposy = 0;
int uwidth = 0;
int uheight = 0;
int uborder = 0;
int uside_border = 0;
int void_click_terminate = 0;
int dont_quit = 0;
int reverse = 0;
int output_only = 0;
int select_only = 0;
int text_after_margin = 0;
int text_other_side = 0;
int clear_memory = 0;
int upside_down = 0;
int padding_swap = 0;
int least_margin = 0;
int hide_missing = 0;
int center_icons = 0;
color_t text_color = {.r = 255, .g = 255, .b = 255, .a = 255};
color_t prompt_color = {.r = 255, .g = 255, .b = 255, .a = 255};
color_t background_color = {.r = 46, .g = 52, .b = 64, .a = 255};
int background_color_set = 0;
color_t highlight_color = {.r = 255, .g = 255, .b = 255, .a = 50};

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


void recalc_cells()
{
    int margined_cell_width, margined_cell_height;
    border=screen_width/10;
    if (uborder>0) border = uborder;
    side_border = border;
    if (uside_border>0) side_border = uside_border;

    if (text_after_margin){
        cell_width=icon_size+icon_padding*2;
        cell_height=icon_size+icon_padding*2;
        margined_cell_width=icon_size+icon_padding*2+least_margin;
        margined_cell_height=icon_size+icon_padding*2+least_margin;
        if(ucolumns == 0)
            ucolumns = 1;
    } else {
        cell_width=icon_size+icon_padding*2;
        cell_height=icon_size+icon_padding*2+font_height+text_padding;
        margined_cell_width=icon_size+icon_padding*2+least_margin;
        margined_cell_height=icon_size+icon_padding*2+font_height+text_padding+least_margin;
    }

    int usable_width;
    int usable_height;
    // These do while loops should run at most three times, it's just to avoid copying code
    do {
        usable_width = screen_width-side_border*2;
        if (no_prompt) {
            usable_height = screen_height-border*2;
        } else {
            usable_height = screen_height-border*2-prompt_spacing-prompt_font_height;
        }

        // If the usable_width is too small, take some space away from the border
        if (usable_width < margined_cell_width) {
            side_border = (screen_width - margined_cell_width - 1)/2;
        } else if (usable_height < margined_cell_height) {
            border = (screen_height - margined_cell_height - prompt_spacing - prompt_font_height - 1)/2;
        }
    } while ((usable_width < margined_cell_width && screen_width > margined_cell_width) || (usable_height < margined_cell_height && screen_height > margined_cell_height));
    // If columns were not manually overriden, calculate the most it can possibly contain
    if (ucolumns == 0){
        columns = usable_width/margined_cell_width;
    } else{
        columns = ucolumns;
        if (center_icons)
        {
           side_border = (screen_width - margined_cell_width*columns - least_margin)/2;
           usable_width = screen_width - side_border*2;
        }
    }
    if (urows == 0){
        rows = usable_height/margined_cell_height;
    } else{
        rows = urows;
        if (center_icons)
        {
           border = (screen_height - margined_cell_height*rows - least_margin)/2;
           usable_height = screen_height - border*2;
        }
    }

    // If we don't have space for a single column or row, force it.
    if (columns == 0) {
        columns = 1;
    }
    if (rows == 0) {
        rows = 1;
    }

    if (text_after_margin){
        cell_width = (usable_width - text_after_margin*(columns - 1))/columns;
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
    prompt_x = side_border;
    prompt_y = border;
    /*
    if(uside_border == 0){
        side_border = border;
    }*/
}


void restack()
{
    if (desktop_mode) XLowerWindow(disp,win);
    else XRaiseWindow(disp,win);
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
            current->x = side_border + i * (cell_width+column_margin);
            if (no_prompt) {
                current->y = border + j * (cell_height+row_margin);
            } else {
                current->y = border + prompt_font_height + prompt_spacing + j * (cell_height+row_margin);
            }
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
    entries = NULL;
    while (current != NULL) {
        node_t * last = current;
        current = current->next;
        if (clear_memory) {
            memset(last->title, 0, 256);
            memset(last->icon, 0, 256);
            memset(last->cmd, 0, 256);
            memset(last, 0, sizeof(node_t));
        }
        free(last);
    }
}


void cleanup()
{
    flock(lock, LOCK_UN | LOCK_NB);
    // destroy window, disconnect display, and exit
    XDestroyWindow(disp,win);
    XFlush(disp);
    XCloseDisplay(disp);
    if(input_source == stdin){
        fclose(input_source);
    }
    clear_entries();
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
}


void filter_entries()
{
    node_t * current = entries;

    while (current != NULL)
    {
        if (strlen(commandline)==0 || strcasestr(current->title,commandline)!=NULL || strcasestr(current->cmd,commandline)!=NULL) current->hidden=0;
        else {
            current->hidden=1;
            current->hovered=0;
            current->clicked=0;
        }
        current=current->next;
    }
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
    exit(1);
}


FILE * determine_input_source(){
    FILE * fp;
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
            fp = fopen(homeconf, "rb");
        }
    } else {
        fp = fopen(input_file, "rb");
    }
    if (fp == NULL)
    {
        fprintf(stderr, "Error opening config file from %s.\nReverting back to system conf.\n", input_file);
        input_file = "/etc/xlunch/entries.dsv";
        fp = fopen(input_file, "rb");

        if (fp == NULL)
        {
            fprintf(stderr, "Error opening config file %s\n", input_file);
            fprintf(stderr, "You may need to create it. Icon file format is following:\n");
            fprintf(stderr, "title;icon_path;command\n");
            fprintf(stderr, "title;icon_path;command\n");
            fprintf(stderr, "title;icon_path;command\n");
        }
    }
    return fp;
}


int parse_entries()
{
    int changed = 0; // if the list of elements have changed or not
    int parsing = 0; // section currently being read
    int position = 0; // position in the current entry
    int line = 1; // line count for error messages
    int readstatus;

    struct pollfd fds;
    fds.fd = fileno(input_source);
    fds.events = POLLIN;
    node_t * current_entry;
    while(poll(&fds, 1, 0)) {
        if(parsing == 0 && position == 0){
            current_entry = malloc(sizeof(node_t));
        }
        char b;
        readstatus = read(fds.fd, &b, 1);
        if(readstatus <= 0){
            break;
        }
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
        switch(parsing){
            case 0:
                current_entry->title[position] = b;
                break;
            case 1:
                current_entry->icon[position] = b;
                break;
            case 2:
                current_entry->cmd[position] = b;
                break;
        }
        position++;
        if(b == '\0') {
            position = 0;
            if(parsing == 2) {
                push_entry(current_entry);
                changed = 1;
                parsing = 0;
            } else {
                parsing++;
            }
        }
        int maxlen = (parsing == 2 ? 511 : 255);
        if(position == maxlen){
            fprintf(stderr, "Entry too long on line %d, maximum length is %d characters!\n", line, maxlen);
            break;
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


int mouse_over_cell(node_t * cell, int mouse_x, int mouse_y)
{
    if (cell->hidden) return 0;

    if (   mouse_x >= cell->x
        && mouse_x < cell->x+cell_width
        && mouse_y >= cell->y
        && mouse_y < cell->y+cell_height) return 1;
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


int starts_with(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? 0 : strncmp(pre, str, lenpre) == 0;
}


void run_command(char * cmd_orig)
{
    if(output_only){
        fprintf(stdout, "%s\n", cmd_orig);
        if(!dont_quit){
            cleanup();
            exit(0);
        } else {
            return;
        }
    }

    char cmd[512];
    char *array[100] = {0};
    strcpy(cmd,cmd_orig);

    int isrecur = starts_with("recur ", cmd_orig) || (strcmp("recur", cmd_orig) == 0);
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
        array[0] = "/bin/bash";
        array[1] = "-c";
        array[2] = cmd_orig;
        array[3] = NULL;
    }

    restack();

    if (dont_quit)
    {
        pid_t pid=fork();
        if (pid==0) // child process
        {
            if (!multiple_instances) close(lock);
            printf("Forking command: %s\n",cmd);
            int err;
            err = execvp(array[0],array);
            fprintf(stderr,"Error forking %s : %d\n",cmd,err);
            exit(0);
        }
        else if (pid<0) // error forking
        {
            fprintf(stderr,"Error running %s\n",cmd);
        }
        else // parent process
        {
            while (cmdline != NULL) pop_key();
            joincmdline();
            joincmdlinetext();
            filter_entries();
            arrange_positions();
        }
    }
    else
    {
        cleanup();
        printf("Running command: %s\n",cmd);
        int err;
        err = execvp(array[0],array);
        fprintf(stderr,"Error running %s : %d\n",cmd, err);
        exit(0);
    }
}


void set_hover(node_t * cell, int hover)
{
    if (cell==NULL) return;
    if (cell->hidden) return;
    if (cell->hovered!=hover) updates = imlib_update_append_rect(updates, cell->x, cell->y, cell_width, cell_height);
    cell->hovered=hover;
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
        exit(1);
    }
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
        exit(1);
    }
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
                    imlib_blend_image_onto_image(image, 1, 0, 0, w, h,  0,0, screen_width, screen_height);
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
    imlib_context_set_color(0, 0, 0, 30);
    imlib_text_draw(posx +1, posy +1, text);
    imlib_text_draw(posx +1, posy +2, text);
    imlib_text_draw(posx +2, posy +2, text);

    imlib_context_set_color(color.r, color.g, color.b, color.a);
    imlib_text_draw(posx, posy, text);
}

void init(int argc, char **argv)
{
    static struct option long_options[] =
        {
            {"version",               no_argument,       0, 'v'},
            {"help",                  no_argument,       0, 'H'},
            {"desktop",               no_argument,       0, 'd'},
            {"rootwindowbackground",  no_argument,       0, 'G'},
            {"noprompt",              no_argument,       0, 'n'},
            {"background",            required_argument, 0, 'g'},
            {"highlight",             required_argument, 0, 'L'},
            {"iconpadding",           required_argument, 0, 'I'},
            {"textpadding",           required_argument, 0, 'T'},
            {"columns",               required_argument, 0, 'c'},
            {"rows",                  required_argument, 0, 'r'},
            {"border",                required_argument, 0, 'b'},
            {"sideborder",            required_argument, 0, 'B'},
            {"promptspacing",         required_argument, 0, 'P'},
            {"iconsize",              required_argument, 0, 's'},
            {"input",                 required_argument, 0, 'i'},
            {"windowed",              no_argument,       0, 'W'},
            {"prompt",                required_argument, 0, 'p'},
            {"font",                  required_argument, 0, 'f'},
            {"promptfont",            required_argument, 0, 'F'},
            {"multiple",              no_argument,       0, 'm'},
            {"voidclickterminate",    no_argument,       0, 't'},
            {"xposition",             required_argument, 0, 'x'},
            {"yposition",             required_argument, 0, 'y'},
            {"width",                 required_argument, 0, 'w'},
            {"height",                required_argument, 0, 'h'},
            {"outputonly",            no_argument,       0, 'o'},
            {"selectonly",            no_argument,       0, 'S'},
            {"textcolor",             required_argument, 0, 1009},
            {"promptcolor",           required_argument, 0, 1010},
            {"backgroundcolor",       required_argument, 0, 1011},
            {"highlightcolor",        required_argument, 0, 1012},
            {"tc",                    required_argument, 0, 1009},
            {"pc",                    required_argument, 0, 1010},
            {"bc",                    required_argument, 0, 1011},
            {"hc",                    required_argument, 0, 1012},
            {"textafter",             required_argument, 0, 'a'},
            {"name",                  required_argument, 0, 1013},
            {"dontquit",              no_argument,       0, 'q'},
            {"reverse",               no_argument,       0, 'R'},
            {"textotherside",         no_argument,       0, 'O'},
            {"clearmemory",           no_argument,       0, 'M'},
            {"upsidedown",            no_argument,       0, 'u'},
            {"paddingswap",           no_argument,       0, 'X'},
            {"leastmargin",           required_argument, 0, 'l'},
            {"center",                no_argument,       0, 'C'},
            {"hidemissing",           no_argument,       0, 'e'},
            {0, 0, 0, 0}
        };

    int c, option_index;
    while ((c = getopt_long(argc, argv, "vdr:ng:L:b:B:s:i:p:f:mc:x:y:w:h:oa:tGHI:T:P:WF:SqROMuXeCl:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'v':
                fprintf(stderr, "xlunch graphical program launcher, version %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
                exit(0);

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
                uborder = atoi(optarg);
                break;

            case 'B':
                uside_border = atoi(optarg);
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
                break;

            case 'y':
                uposy = atoi(optarg);
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

            case 1012:
                sscanf(optarg, "%02x%02x%02x%02x", &highlight_color.r, &highlight_color.g, &highlight_color.b, &highlight_color.a);
                break;

            case 'a':
                text_after_margin = atoi(optarg);
                break;

            case 1013:
                program_name = optarg;
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

            case 'C':
                center_icons = 1;
                break;

            case 'e':
                hide_missing = 1;
                break;

            case '?':
            case 'H':
                fprintf (stderr,"usage: xlunch [options]\n");
                fprintf (stderr,"    xlunch is a program launcher/option selector similar to dmenu or rofi.\n");
                fprintf (stderr,"    By default it launches in full-screen mode and terminates after a selection is made,\n");
                fprintf (stderr,"    it is also possible to close xlunch by pressing Esc or the right mouse button.\n");
                fprintf (stderr,"    Some options changes this behaviour, the most notable being the desktop mode switch:\n");
                fprintf (stderr,"        -d, --desktop                     Desktop mode, always keep the launcher at background\n");
                fprintf (stderr,"                                          (behind other windows), and ignore ESC and right mouse click.\n");
                fprintf (stderr,"                                          Combined with --dontquit xlunch never exits (behaviour of --desktop\n");
                fprintf (stderr,"                                          from previous versions).\n");
                fprintf (stderr,"    Functinal options:\n");
                fprintf (stderr,"        -v, --version                     Returns the version of xlunch\n");
                fprintf (stderr,"        -H, --help                        Shows this help message\n");
                fprintf (stderr,"        --name                            POSIX-esque way to specify the first part of WM_CLASS\n");
                fprintf (stderr,"                                          (default: environment variable RESOURCE_NAME or argv[0])\n");
                fprintf (stderr,"        -n, --noprompt                    Hides the prompt, only allowing selection by icon\n");
                fprintf (stderr,"        -o, --outputonly                  Do not run the selected entry, only output it to stdout\n");
                fprintf (stderr,"        -S, --selectonly                  Only allow an actual entry and not free-typed commands\n");
                fprintf (stderr,"        -i, --input [file]                File to read entries from, defaults to stdin if data is available\n");
                fprintf (stderr,"                                          otherwise it reads from $HOME/.config/xlunch/entries.dsv or /etc/xlunch/entries.dsv\n");
                fprintf (stderr,"        -m, --multiple                    Allow multiple instances running\n");
                fprintf (stderr,"        -t, --voidclickterminate          Clicking anywhere that's not an entry terminates xlunch,\n");
                fprintf (stderr,"                                          practical for touch screens.\n");
                fprintf (stderr,"        -q, --dontquit                    When an option is selected, don't close xlunch. Combined with --desktop xlunch\n");
                fprintf (stderr,"                                          never exits (behaviour of --desktop from previous versions).\n");
                fprintf (stderr,"        -R, --reverse                     All entries in xlunch as reversly ordered.\n");
                fprintf (stderr,"        -W, --windowed                    Start in windowed mode\n");
                fprintf (stderr,"        -M, --clearmemory                 Set the memory of each entry to null before exiting. Used for passing sensitive\n");
                fprintf (stderr,"                                          information through xlunch.\n\n");
                fprintf (stderr,"    Multi monitor setup: xlunch cannot detect your output monitors, it sees your monitors\n");
                fprintf (stderr,"    as a big single screen. You can customize this manually by setting windowed mode and\n");
                fprintf (stderr,"    providing the top/left coordinates and width/height of your monitor screen which\n");
                fprintf (stderr,"    effectively positions xlunch on the desired monitor. Use the following options:\n");
                fprintf (stderr,"        -x, --xposition [i]               The x coordinate of the launcher window\n");
                fprintf (stderr,"        -y, --yposition [i]               The y coordinate of the launcher window\n");
                fprintf (stderr,"        -w, --width [i]                   The width of the launcher window\n");
                fprintf (stderr,"        -h, --height [i]                  The height of the launcher window\n\n");
                fprintf (stderr,"    Style options:\n");
                fprintf (stderr,"        -p, --prompt [text]               The prompt asking for input (default: \"Run: \")\n");
                fprintf (stderr,"        -f, --font [name]                 Font name including size after slash (default: \"OpenSans-Regular/10\" and  \"DejaVuSans/10\")\n");
                fprintf (stderr,"        -F, --promptfont [name]           Font to use for the prompt (default: same as --font)\n");
                fprintf (stderr,"        -G, --rootwindowbackground        Use root windows background image\n");
                fprintf (stderr,"        -g, --background [file]           Image to set as background (jpg/png)\n");
                fprintf (stderr,"        -L, --highlight [file]            Image set as highlighting under selected icon (jpg/png)\n");
                fprintf (stderr,"        -I, --iconpadding [i]             Padding around icons (default: 10)\n");
                fprintf (stderr,"        -T, --textpadding [i]             Padding around entry titles (default: 10)\n");
                fprintf (stderr,"        -c, --columns [i]                 Number of columns to show (without this the max amount possible is used)\n");
                fprintf (stderr,"        -r, --rows [i]                    Numbers of rows to show (without this the max amount possible is used)\n");
                fprintf (stderr,"        -b, --border [i]                  Size of the border around the icons and prompt (default: 1/10th of screen width)\n");
                fprintf (stderr,"        -B, --sideborder [i]              Size of the border on the sides, if this is used --border will be only top and bottom\n");
                fprintf (stderr,"        -C, --center                      Center icons. Has effect only in combination with --rows or --columns (or both).\n");
                fprintf (stderr,"                                          When used, border is ignored. You can adjust the space between icons with --leastmargin\n");
                fprintf (stderr,"        -P, --promptspacing [i]           Distance between the prompt and the icons (default: 48)\n");
                fprintf (stderr,"        -s, --iconsize [i]                Size of the icons (default: 48)\n");
                fprintf (stderr,"        -a, --textafter [i]               Draw the title to the right of the icon instead of below, this option\n");
                fprintf (stderr,"                                          automatically sets --columns to 1 but this can be overridden. The argument is\n");
                fprintf (stderr,"                                          the margin to apply between columns (default: n/a).\n");
                fprintf (stderr,"        -O, --textotherside               Draw the text on the other side of the icon from where it is normally drawn.\n");
                fprintf (stderr,"        -u, --upsidedown                  Draw the prompt on the bottom and have icons sort from bottom to top.\n");
                fprintf (stderr,"        -X, --paddingswap                 Icon padding and text padding swaps order around text.\n");
                fprintf (stderr,"        -l, --leastmargin [i]             Adds a margin to the calculation of application sizes, no effect when specified rows and columns.\n");
                fprintf (stderr,"        -e, --hidemissing                 Hide entries with missing or broken icon images\n");
                fprintf (stderr,"        --tc, --textcolor [color]         Color to use for the text on the format rrggbbaa (default: ffffffff)\n");
                fprintf (stderr,"        --pc, --promptcolor [color]       Color to use for the prompt text (default: ffffffff)    \n");
                fprintf (stderr,"        --bc, --backgroundcolor [color]   Color to use for the background (default: 2e3440ff)\n");
                fprintf (stderr,"                                          (NOTE: transparent background color requires a compositor)\n");
                fprintf (stderr,"        --hc, --highlightcolor [color]    Color to use for the highlight box (default: ffffff32)\n\n");
                // Check if we came from the error block above or if this was a call with --help
                if(c == '?'){
                    exit(1);
                } else {
                    exit(0);
                }
        }
    }
    /* connect to X */
    disp = XOpenDisplay(NULL);
    if (!disp)
    {
        fprintf(stderr,"Cannot connect to DISPLAY\n");
        exit(2);
    }

    XMatchVisualInfo(disp, DefaultScreen(disp), 32, TrueColor, &vinfo);

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
}

/* the program... */
int main(int argc, char **argv){
    /* events we get from X */
    XEvent ev;
    /* our virtual framebuffer image we draw into */
    Imlib_Image buffer;
    /* a font */
    Imlib_Font font;
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
        lock=open("/tmp/xlunch.lock",O_CREAT | O_RDWR,0666);
        int rc = flock(lock, LOCK_EX | LOCK_NB);
        if (rc) {
            if (errno == EWOULDBLOCK) fprintf(stderr,"xlunch already running. You may want to consider --multiple\nIf this is an error, you may remove /tmp/xlunch.lock\n");
            exit(3);
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
    XSelectInput(disp, win, StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask | ExposureMask | KeyPressMask | KeyReleaseMask | KeymapStateMask | FocusChangeMask );
    GC gc = XCreateGC(disp, win, 0, 0);
    /* set our name */
    XClassHint* classHint;

    /* set the titlebar name */
    XStoreName(disp, win, "xlunch: Graphical app launcher");

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

    // prepare for keyboard UTF8 input
    if (XSetLocaleModifiers("@im=none") == NULL) return 11;
    im = XOpenIM(disp, NULL, NULL, NULL);
    if (im == NULL) {
        fputs("Could not open input method\n", stdout);
        return 2;
    }

    ic = XCreateIC(im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, win, NULL);
    if (ic == NULL) {
        fprintf(stderr,"Could not open input context, without it we can't properly handle UTF8\n");
        return 4;
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
    eventfds[0].events = POLLIN || POLLPRI || POLLOUT || POLLRDHUP;
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
            exit(1);
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
                        break;


                    case ConfigureNotify:
                    {
                        if (screen_width!=ev.xconfigure.width || screen_height!=ev.xconfigure.height)
                        {
                            screen_width=ev.xconfigure.width;
                            screen_height=ev.xconfigure.height;
                            if (!use_root_img) update_background_images();
                            recalc_cells();
                            arrange_positions();
                            updates = imlib_update_append_rect(updates, 0, 0, screen_width, screen_height);
                        }
                        break;
                    }

                    case ButtonPress:
                    {
                        if (ev.xbutton.button==3 && !desktop_mode) {
                            cleanup();
                            exit(0);
                        }
                        if (ev.xbutton.button!=1) break;
                        node_t * current = entries;
                        int voidclicked = 1;
                        while (current != NULL)
                        {
                            if (mouse_over_cell(current, ev.xmotion.x, ev.xmotion.y)) {
                                set_clicked(current,1);
                                voidclicked = 0;
                            }
                            else set_clicked(current,0);
                            current = current->next;
                        }

                        if (voidclicked && void_click_terminate) {
                            cleanup();
                            exit(0);
                        }
                        break;
                    }

                    case ButtonRelease:
                    {
                        node_t * current = entries;

                        while (current != NULL)
                        {
                            if (mouse_over_cell(current, ev.xmotion.x, ev.xmotion.y)) if (current->clicked==1) run_command(current->cmd);
                            set_clicked(current, 0); // button release means all cells are not clicked
                            current = current->next;
                        }

                        break;
                    }

                    // refresh keyboard layout if changed
                    case KeymapNotify:
                        XRefreshKeyboardMapping(&ev.xmapping);
                        break;

                    case KeyPress:
                    {
                        // keyboard events
                        int count = 0;
                        KeySym keycode = 0;
                        Status status = 0;
                        char kbdbuf[20]= {0};
                        count = Xutf8LookupString(ic, (XKeyPressedEvent*)&ev, kbdbuf, 20, &keycode, &status);

                        if (keycode==XK_Escape && !desktop_mode)
                        {
                            cleanup();
                            exit(0);
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
                                }
                                if (!current->hidden && current->hovered) selected=current;
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
                            if (keycode==XK_Up || keycode==XK_KP_Up || keycode==XK_Page_Up) i=-columns;
                            if (keycode==XK_Down || keycode==XK_KP_Down || keycode==XK_Page_Down) i=columns;
                            if (keycode==XK_Tab || keycode==XK_Right || keycode==XK_KP_Right) i=1;

                            int j=0,n=0;
                            node_t * current = entries;
                            node_t * selected = NULL;
                            while (current != NULL)
                            {
                                if (!current->hidden)
                                {
                                    if (current->y+cell_height<=screen_height-border) n++;
                                    if (selected==NULL) j++;
                                    if (current->hovered) selected=current;
                                    set_hover(current,0);
                                }
                                current=current->next;
                            }

                            if (selected==NULL) {
                                selected=entries;
                                i=0;
                                j=0;
                            }
                            current=entries;

                            int k=i+j;
                            if (k>n || keycode==XK_End) k=n;
                            if (k<1 || keycode==XK_Home) k=1;
                            while (current != NULL)
                            {
                                if (!current->hidden)
                                {
                                    k--;
                                    if (k==0) {
                                        set_hover(current,1);
                                        hoverset=KEYBOARD;
                                    }
                                }
                                current=current->next;
                            }

                            continue; // do not conitnue
                        }

                        if (keycode==XK_Delete || keycode==XK_BackSpace)
                            pop_key();
                        else if (count>1 || (count==1 && kbdbuf[0]>=32)) // ignore unprintable characterrs
                            if (!no_prompt) push_key(kbdbuf);

                        joincmdline();
                        joincmdlinetext();
                        filter_entries();
                        arrange_positions();

                        // we used keyboard to type command. So unselect all icons.
                        node_t * current = entries;
                        while (current != NULL)
                        {
                            set_hover(current,0);
                            set_clicked(current,0);
                            current = current->next;
                        }

                        updates = imlib_update_append_rect(updates, 0, 0, screen_width, screen_height);
                        break;
                    }

                    case KeyRelease:
                        break;

                    case EnterNotify:
                    case MotionNotify:
                    {
                        node_t * current = entries;

                        while (current != NULL)
                        {
                            if (mouse_over_cell(current, ev.xmotion.x, ev.xmotion.y)) {
                                set_hover(current,1);
                                hoverset=MOUSE;
                            }
                            else {
                                set_hover(current,0);
                                set_clicked(current,0);
                            }
                            current = current->next;
                        }
                        break;
                    }

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
                Cursor c = XCreateFontCursor(disp,XC_top_left_arrow);

                while (current != NULL)
                {
                    if (!current->hidden)
                    {
                        if (current->hovered)
                        {
                            c = XCreateFontCursor(disp,XC_hand1);

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
                                int d;
                                if (current->clicked) d=2;
                                else d=0;
                                x = current->x - up_x +
                                            (text_other_side && text_after_margin ? cell_width - icon_padding - icon_size : icon_padding)+d;
                                y = current->y - up_y +(text_other_side && !text_after_margin ? cell_height - icon_padding - icon_size : icon_padding)+d;

                                imlib_blend_image_onto_image(image, 1, 0, 0, w, h, x, y, icon_size-d*2, icon_size-d*2);

                                imlib_context_set_image(image);
                                imlib_free_image();
                            }
                        }
                        /* draw text under icon */
                        font = load_font();
                        if (font)
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
                            } while(text_w > cell_width-(text_after_margin ? (icon_size != 0 ? icon_padding*2 : icon_padding) + icon_size + text_padding : 2*text_padding) && sz>0);

                            int d;
                            if (current->clicked==1) d=4;
                            else d=0;

                            if (text_after_margin) {
                                draw_text_with_shadow(current->x - up_x + (text_other_side ? text_padding : (icon_size != 0 ? (padding_swap ? icon_padding + text_padding : icon_padding*2) : icon_padding) + icon_size), current->y - up_y + cell_height/2 - font_height/2, title, text_color);
                            } else {
                                draw_text_with_shadow(current->x - up_x + cell_width/2 - text_w/2, current->y - up_y + (text_other_side ? text_padding : (padding_swap ? icon_padding + text_padding : icon_padding*2) + icon_size), title, text_color);
                            }

                            /* free the font */
                            imlib_free_font();
                        }
                        drawn++;
                    }
                    if (drawn == columns*rows)
                        break;
                    current = current->next;
                }

                XDefineCursor(disp,win,c);

                /* set the buffer image as our current image */
                imlib_context_set_image(buffer);

                /* draw text */
                if (!no_prompt) {
                    font = load_prompt_font();
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
