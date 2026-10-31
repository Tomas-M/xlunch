#include "ui.h"

Pixmap GetRootPixmap(Display* display, Window *root)
{
    Pixmap currentRootPixmap=None;
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
    Pixmap bg;
    XImage* img;

    display = XOpenDisplay(NULL);
    if (display == NULL) return 0;

    screen = DefaultScreen(display);
    root = RootWindow(display, screen);

    bg = GetRootPixmap(display, &root);
    if (bg==None) { XCloseDisplay(display); return 0; }
    // The wallpaper can still have its previous size while the desktop resizes.
    Window pixmap_root;
    int pixmap_x,pixmap_y;
    unsigned int pixmap_width,pixmap_height,border,depth;
    if (!XGetGeometry(display,bg,&pixmap_root,&pixmap_x,&pixmap_y,&pixmap_width,&pixmap_height,&border,&depth))
    { XCloseDisplay(display); return 0; }
    int width=screen_width<(int)pixmap_width ? screen_width : (int)pixmap_width;
    int height=screen_height<(int)pixmap_height ? screen_height : (int)pixmap_height;
    img = XGetImage(display, bg, 0, 0, width, height, ~0, ZPixmap);

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
            direct[y*screen_width+x] = 0xff000000|(pixel&0x00ffffff);
        }
    }

    XDestroyImage(img); XCloseDisplay(display);
    return 1;
}

void draw_scrollbar_pill(Imlib_Image buffer, int x, int y, int height, color_t color)
{
    Imlib_Image pill=imlib_create_image(24, height*3);
    imlib_context_set_image(pill);
    imlib_image_set_has_alpha(1);
    imlib_image_clear();
    imlib_context_set_blend(0);
    imlib_context_set_color(color.r, color.g, color.b, color.a);
    imlib_image_fill_ellipse(12, 12, 12, 12);
    imlib_image_fill_ellipse(12, height*3-13, 12, 12);
    imlib_image_fill_rectangle(0, 12, 24, height*3-24);
    imlib_context_set_image(buffer);
    imlib_context_set_blend(1);
    imlib_blend_image_onto_image(pill, 1, 0, 0, 24, height*3, x, y, 8, height);
    imlib_context_set_image(pill);
    imlib_free_image();
    imlib_context_set_image(buffer);
}

void update_background_images()
{
    if (focus_mode) { focus_background(); return; }
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
                        if ( (double)w/h < (double)screen_width/screen_height )
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

void renderEntry(Imlib_Image buffer, node_t * current, int up_x, int up_y) {
    int h = 0;
    int w = 0;
    if (current->hovered)
    {
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
        imlib_context_set_font(font);

        // Keep label geometry stable across hover redraws; recompute it after resizing.
        if (current->text_cell_width != cell_width)
        {
            const size_t osz = strlen(current->title);
            size_t sz = osz;
            do
            {
                strncpyutf8(current->display_title,current->title,sz);
                if(sz != osz)
                    strcat(current->display_title,"..");
                imlib_get_text_size(current->display_title, &current->text_width, NULL);
                sz--;
            } while(current->text_width > cell_width-(text_after ? (icon_size != 0 ? icon_padding*2 : icon_padding) + icon_size + text_padding : 2*text_padding) && sz>0);
            current->text_cell_width=cell_width;
        }

        int d;
        if (current->clicked==1) d=4;
        else d=0;

        if (text_after) {
            draw_text_with_shadow(current->x - up_x + (text_other_side ? text_padding : (icon_size != 0 ? (padding_swap ? icon_padding*2 : icon_padding + text_padding) : icon_padding) + icon_size), current->y - up_y + cell_height/2 - font_height/2, current->display_title, text_color);
        } else {
            draw_text_with_shadow(current->x - up_x + cell_width/2 - current->text_width/2, current->y - up_y + (text_other_side ? text_padding : (padding_swap ? icon_v_padding*2 : icon_v_padding + text_padding) + icon_size), current->display_title, text_color);
        }

        /* free the font */
        imlib_free_font();
    }
}
