#include "ui.h"
#include <dlfcn.h>

typedef struct svg_viewport { double x,y,width,height; } svg_viewport_t;
static struct {
    void *(*handle)(const char *,void **);
    int (*render)(void *,void *,const svg_viewport_t *,void **);
    void (*unref)(void *),(*error_free)(void *);
    void *(*surface)(unsigned char *,int,int,int,int),*(*context)(void *);
    void (*surface_free)(void *),(*context_free)(void *);
} svg;
static int initialized;

// Use the SVG loader's existing runtime libraries without requiring new build headers.
static int svg_initialize(void)
{
    if (!initialized)
    {
        initialized=1;
        void *rsvg=dlopen("librsvg-2.so.2",RTLD_LAZY|RTLD_LOCAL),*cairo=dlopen("libcairo.so.2",RTLD_LAZY|RTLD_LOCAL);
        if (!rsvg || !cairo) return 0;
        svg.handle=dlsym(rsvg,"rsvg_handle_new_from_file"); svg.render=dlsym(rsvg,"rsvg_handle_render_document");
        svg.unref=dlsym(rsvg,"g_object_unref"); svg.error_free=dlsym(rsvg,"g_error_free");
        svg.surface=dlsym(cairo,"cairo_image_surface_create_for_data"); svg.context=dlsym(cairo,"cairo_create");
        svg.surface_free=dlsym(cairo,"cairo_surface_destroy"); svg.context_free=dlsym(cairo,"cairo_destroy");
    }
    return svg.render!=NULL;
}

// Rasterize vectors at their final device size, then convert Cairo's premultiplied pixels.
Imlib_Image ui_svg_image(const char *path, int width, int height)
{
    if (!svg_initialize()) return NULL;
    void *error=NULL,*handle=svg.handle(path,&error);
    if (!handle) { if (error) svg.error_free(error); return NULL; }
    Imlib_Image image=imlib_create_image(width,height); imlib_context_set_image(image);
    imlib_image_set_has_alpha(1); imlib_image_clear(); DATA32 *data=imlib_image_get_data();
    void *surface=svg.surface((unsigned char *)data,0,width,height,width*4),*context=svg.context(surface);
    svg_viewport_t viewport={0,0,width,height};
    int rendered=svg.render(handle,context,&viewport,&error);
    svg.context_free(context); svg.surface_free(surface); svg.unref(handle);
    if (error) svg.error_free(error);
    for (int i=0;i<width*height;i++)
    {
        unsigned int alpha=data[i]>>24;
        if (alpha) data[i]=(alpha<<24)|((((data[i]>>16)&255)*255/alpha)<<16)
            |((((data[i]>>8)&255)*255/alpha)<<8)|((data[i]&255)*255/alpha);
    }
    imlib_image_put_back_data(data);
    if (!rendered) { imlib_free_image(); return NULL; }
    return image;
}
