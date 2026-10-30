#ifndef XLUNCH_UI_H
#define XLUNCH_UI_H
#include "xlunch.h"
#include "cache.h"
#include <dirent.h>
#include <locale.h>
#include <langinfo.h>
#include <math.h>
#include <wchar.h>
#include <wctype.h>

typedef struct ui_rect { int x,y,w,h; } ui_rect_t;
extern int focus_mode;
extern const char *ui_check_file;
const char *ui_value(const char *key);
int ui_number(const char *key);
int ui_bool(const char *key);
double ui_scale(void);
color_t ui_color(const char *key, color_t fallback);
void ui_set(const char *key, const char *value, const char *file, int line);
void ui_read(const char *file);
void ui_validate(void);
char **ui_list(const char *value, int *count);
void ui_list_free(char **items, int count);
char **ui_sections(const char *prefix, int *count);
const char *ui_field(const char *kind, const char *id, const char *field);
const char *ui_system_icon_theme(void);
char *ui_icon_path(const char *name, int size);
Imlib_Image ui_icon_image(const char *name, int width, int height);
Imlib_Image ui_svg_image(const char *path, int width, int height);
void ui_load_entries(void);
void ui_custom_entries(void);
int ui_entry_matches(node_t *entry);
int ui_entry_search(node_t *entry);
int ui_group_contains(node_t *entry, const char *id);
node_t **ui_visible_entries(node_t *catalog, int *count);
extern char focus_group[256];
void ui_activate(node_t *entry);
void ui_action(const char *command, const char *output);
char *ui_executable(const char *name);
int ui_entry_needs_install(node_t *entry);
int ui_command(const char *command);
void focus_layout(void);
void focus_positions(void);
void focus_viewport(int *top, int *bottom);
double focus_max_scroll(void);
scrollbar_t focus_scrollbar(void);
void focus_render(Imlib_Image buffer, int x, int y);
int focus_event(XEvent *event);
void focus_background(void);
void focus_refresh(void);
int focus_input_timeout(void);
void focus_input_refresh(void);
void focus_cleanup(void);
void focus_select(int index);
#endif
