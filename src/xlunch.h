#ifndef XLUNCH_H
#define XLUNCH_H
#define _GNU_SOURCE
#include <fcntl.h>
#include <X11/Xlib.h>
#include <Imlib2.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <X11/cursorfont.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <ctype.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <errno.h>
#include <time.h>

typedef struct node
{
    char title[256];
    char display_title[258];
    int text_width;
    int text_cell_width;
    char icon[256];
    char cmd[512];
    char *id,*name,*categories,*description,*keywords,*desktop_file,*desktop_exec,*executable;
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

typedef struct scrollbar {
    int x, y, width, height, thumb_y, thumb_height;
} scrollbar_t;

#define MOUSE 1
#define KEYBOARD 2

extern const int VERSION_MAJOR;
extern const int VERSION_MINOR;
extern const int VERSION_PATCH;
extern Display *disp;
extern Window   win;
extern XVisualInfo vinfo;
extern XSetWindowAttributes attr;
extern int x11_fd;
extern struct pollfd eventfds[3];
extern XIM im;
extern XIC ic;
extern int screen;
extern int screen_width;
extern int screen_height;
extern int entries_count;
extern node_t * entries;
extern button_t * buttons;
extern shortcut_t * shortcuts;
extern keynode_t * cmdline;
extern int icon_size;
extern int ucolumns;
extern int columns;
extern int urows;
extern int rows;
extern int column_margin;
extern int row_margin;
extern int no_row_stretch;
extern int icon_padding;
extern int icon_v_padding;
extern int text_padding;
extern int border;
extern int side_border;
extern int border_ratio;
extern int side_border_ratio;
extern int cell_width;
extern int cell_height;
extern int font_height;
extern int prompt_font_height;
extern int use_root_img;
extern char commandline[10024];
extern char commandlinetext[10024];
extern int prompt_x;
extern int prompt_y;
extern int mouse_moves;
extern char * background_file;
extern char * highlight_file;
extern char * input_file;
extern int read_config;
extern FILE * input_source;
extern char * prompt;
extern char * font_name;
extern char * prompt_font_name;
extern char * program_name;
extern char * window_title;
extern char * window_icon;
extern int bg_fill;
extern int no_prompt;
extern int no_title;
extern int prompt_spacing;
extern int windowed;
extern int multiple_instances;
extern int uposx;
extern int uposy;
extern int force_reposition;
extern int uwidth;
extern int uheight;
extern percentable_t uborder;
extern percentable_t uside_border;
extern int void_click_terminate;
extern int focus_lost_terminate;
extern int dont_quit;
extern int reverse;
extern int output_only;
extern int select_only;
extern int text_after;
extern int text_other_side;
extern int clear_memory;
extern int upside_down;
extern int padding_swap;
extern int least_margin;
extern int least_v_margin;
extern int hide_missing;
extern int center_icons;
extern int noscroll;
extern double scrolled_past;
extern double scroll_target;
extern double scroll_start;
extern struct timespec scroll_time;
extern int scrollbar_drag_y;
extern double scrollbar_drag_scroll;
extern int hovered_entry;
extern color_t text_color;
extern color_t prompt_color;
extern color_t background_color;
extern color_t shadow_color;
extern int background_color_set;
extern color_t highlight_color;
extern color_t scrollbar_color;
extern color_t scrollindicator_color;
extern int stdin_poll_timeout;
extern int nostdin;
extern int hoverset;
extern int desktop_mode;
extern int lock;
extern Imlib_Updates updates, current_update;
extern Imlib_Image background;
extern Imlib_Image highlight;
extern Imlib_Image image;
extern struct option long_options[];
extern const int option_count;

void calculate_percentage(int maxvalue, percentable_t* percentable);
void recalc_cells();
void restack();
char* strncpyutf8(char* dst, const char* src, size_t num);
void arrange_positions();
void push_key(char * key);
void pop_key();
void clear_entries();
void free_entry(node_t *entry);
void cleanup();
Imlib_Image load_image(char * icon);
void push_entry(node_t * new_entry);
char *strtok_new(char * string, char const * delimiter);
char* concat(const char *s1, const char *s2);
FILE * determine_input_source();
void entry_viewport(int *top, int *bottom);
int mouse_over_cell(node_t * cell, int mouse_x, int mouse_y);
int mouse_over_button(button_t * button, int mouse_x, int mouse_y);
Pixmap GetRootPixmap(Display* display, Window *root);
int get_root_image_to_imlib_data(DATA32 * direct);
void joincmdline();
void joincmdlinetext();
double max_scroll_level();
void set_scroll_level(double new_scroll);
void scroll_wheel(int direction);
scrollbar_t scrollbar_geometry();
void draw_scrollbar_pill(Imlib_Image buffer, int x, int y, int height, color_t color);
void drag_scrollbar(int mouse_y);
void set_hover(int hovered, node_t * cell, int hover);
void hover_entry(int entry);
void filter_entries();
int starts_with(const char *pre, const char *str);
void run_internal_command(char * cmd_orig);
void reset_prompt();
void run_command(char * cmd_orig);
int parse_entries(void);
void parse_button(char *button_spec);
void set_clicked(node_t * cell, int clicked);
Imlib_Font load_default_font();
int get_font_height(Imlib_Font font);
Imlib_Font load_font();
Imlib_Font load_prompt_font();
void update_background_images();
void draw_text_with_shadow(int posx, int posy, char * text, color_t color);
void parse_config(FILE *input);
void handle_option(int c, char *optarg);
void init(int argc, char **argv);
void recheckHover(XEvent ev);
void handleButtonPress(XEvent ev);
void handleButtonRelease(XEvent ev);
void handleKeyPress(XEvent ev);
void renderEntry(Imlib_Image buffer, node_t * current, unsigned int * cursor_shape, int up_x, int up_y);
#endif
