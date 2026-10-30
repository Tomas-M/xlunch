#include "xlunch.h"
#include <dlfcn.h>

// Load themed cursors for controls and text; inherit the desktop cursor elsewhere.
void pointer_cursor(pointer_kind_t kind)
{
    static Cursor cursors[3];
    static int initialized;
    static pointer_kind_t active;
    if (!initialized)
    {
        initialized=1;
        void *library=dlopen("libXcursor.so.1",RTLD_LAZY|RTLD_LOCAL);
        Cursor (*load)(Display *,const char *)=library ? dlsym(library,"XcursorLibraryLoadCursor") : NULL;
        if (load) { cursors[POINTER_HAND]=load(disp,"hand2"); cursors[POINTER_TEXT]=load(disp,"text"); }
    }
    if (active==kind) return;
    XDefineCursor(disp,win,cursors[kind]); active=kind;
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
