#include "ui.h"

typedef struct icon_cache { char *name,*path; int size; struct icon_cache *next; } icon_cache_t;
static icon_cache_t *icon_cache;
typedef struct icon_image { char *path; int w,h; Imlib_Image image; struct icon_image *next; } icon_image_t;
static icon_image_t *icon_images;

// Compare available icon sizes using fixed, threshold and scalable theme metadata.
static void icon_candidate(const char *root, const char *theme, const char *directory, const char *name,
                           int requested, int size, int minimum, int maximum, int threshold, int scale,
                           const char *type, int *best, char **result)
{
    int low=size,high=size;
    if (!strcmp(type,"Scalable")) { low=minimum; high=maximum; }
    if (!strcmp(type,"Threshold")) { low=size-threshold; high=size+threshold; }
    low*=scale; high*=scale;
    int distance=requested<low ? low-requested : requested>high ? requested-high : 0;
    if (distance>=*best) return;
    const char *extensions[]={"png","svg","xpm"};
    for (int i=0;i<3;i++)
    {
        char *path; asprintf(&path,"%s/%s/%s/%s.%s",root,theme,directory,name,extensions[i]);
        if (!access(path,R_OK)) { free(*result); *result=path; *best=distance; return; }
        free(path);
    }
}

// Resolve icons in a theme before trying its explicitly inherited themes.
static char *theme_icon(const char *roots, const char *theme, const char *name, int requested, int depth)
{
    if (depth>12) return NULL;
    char *folders=strdup(roots),*save,*result=NULL,*inherits=NULL; int best=100000;
    for (char *root=strtok_r(folders,":",&save);root;root=strtok_r(NULL,":",&save))
    {
        char *index; asprintf(&index,"%s/%s/index.theme",root,theme);
        FILE *input=fopen(index,"r"); free(index); if (!input) continue;
        char *line=NULL,*section=strdup("Icon Theme"),type[32]="Threshold"; size_t capacity=0;
        int size=0,minimum=0,maximum=0,threshold=2,scale=1;
        while (getline(&line,&capacity,input)>=0)
        {
            line[strcspn(line,"\r\n")]=0;
            if (*line=='[')
            {
                if (strcmp(section,"Icon Theme")) icon_candidate(root,theme,section,name,requested,size,minimum,maximum,threshold,scale,type,&best,&result);
                char *end=strchr(line,']'); if (!end) continue; *end=0; free(section); section=strdup(line+1);
                size=minimum=maximum=0; threshold=2; scale=1; strcpy(type,"Threshold"); continue;
            }
            char *equal=strchr(line,'='); if (!equal) continue; *equal=0; const char *value=equal+1;
            if (!strcmp(section,"Icon Theme") && !strcmp(line,"Inherits") && !inherits) inherits=strdup(value);
            if (!strcmp(line,"Size")) size=minimum=maximum=atoi(value);
            if (!strcmp(line,"MinSize")) minimum=atoi(value);
            if (!strcmp(line,"MaxSize")) maximum=atoi(value);
            if (!strcmp(line,"Threshold")) threshold=atoi(value);
            if (!strcmp(line,"Scale")) scale=atoi(value);
            if (!strcmp(line,"Type")) snprintf(type,sizeof(type),"%s",value);
        }
        if (strcmp(section,"Icon Theme")) icon_candidate(root,theme,section,name,requested,size,minimum,maximum,threshold,scale,type,&best,&result);
        free(line); free(section); fclose(input);
    }
    free(folders);
    if (!result && inherits)
    {
        int count; char **parents=ui_list(inherits,&count);
        for (int i=0;i<count && !result;i++) if (strcmp(theme,parents[i])) result=theme_icon(roots,parents[i],name,requested,depth+1);
        ui_list_free(parents,count);
    }
    free(inherits); return result;
}

// Cache theme resolution independently of the Imlib2 image cache.
char *ui_icon_path(const char *name, int size)
{
    if (!*name) return NULL;
    if (strchr(name,'/')) return !access(name,R_OK) ? strdup(name) : NULL;
    for (icon_cache_t *item=icon_cache;item;item=item->next)
        if (item->size==size && !strcmp(item->name,name)) return item->path ? strdup(item->path) : NULL;
    char *roots,*home=NULL; const char *data=getenv("XDG_DATA_HOME"),*dirs=getenv("XDG_DATA_DIRS");
    if (!data || !*data || *data!='/') { asprintf(&home,"%s/.local/share",getenv("HOME")); data=home; }
    asprintf(&roots,"%s/icons:%s/.icons",data,getenv("HOME"));
    char *folders=strdup(dirs && *dirs ? dirs : "/usr/local/share:/usr/share"),*save;
    for (char *folder=strtok_r(folders,":",&save);folder;folder=strtok_r(NULL,":",&save))
    {
        if (*folder!='/') continue;
        char *joined; asprintf(&joined,"%s:%s/icons",roots,folder); free(roots); roots=joined;
    }
    free(folders); free(home);
    char *plain=strdup(name),*extension=strrchr(plain,'.');
    if (extension && (!strcmp(extension,".png") || !strcmp(extension,".svg") || !strcmp(extension,".xpm"))) *extension=0;
    char *path=theme_icon(roots,ui_value("icons.theme"),plain,size,0);
    if (!path && strcmp(ui_value("icons.theme"),"hicolor")) path=theme_icon(roots,"hicolor",plain,size,0);
    if (!path)
    {
        const char *extensions[]={"png","svg","xpm"};
        for (int i=0;i<3 && !path;i++)
        {
            char *candidate; asprintf(&candidate,"/usr/share/pixmaps/%s.%s",plain,extensions[i]);
            if (!access(candidate,R_OK)) path=candidate; else free(candidate);
        }
    }
    free(plain); free(roots);
    icon_cache_t *item=calloc(1,sizeof(*item)); item->name=strdup(name); item->size=size; item->path=path; item->next=icon_cache; icon_cache=item;
    return path ? strdup(path) : NULL;
}

// Cache final-size icon pixels; the caller borrows the image and never frees it.
Imlib_Image ui_icon_image(const char *name, int width, int height)
{
    char *path=ui_icon_path(name,width); if (!path) return NULL;
    for (icon_image_t *item=icon_images;item;item=item->next)
        if (item->w==width && item->h==height && !strcmp(item->path,path)) { free(path); return item->image; }
    Imlib_Image original=imlib_load_image(path); if (!original) { free(path); return NULL; }
    imlib_context_set_image(original); int w=imlib_image_get_width(),h=imlib_image_get_height();
    double scale=fmin((double)width/w,(double)height/h); int dw=fmax(1,lround(w*scale)),dh=fmax(1,lround(h*scale));
    // Preparing an offscreen icon must not inherit the viewport's drawing clip.
    int cx,cy,cw,ch; imlib_context_get_cliprect(&cx,&cy,&cw,&ch); char blend=imlib_context_get_blend();
    imlib_context_set_cliprect(0,0,0,0); imlib_context_set_blend(0);
    const char *extension=strrchr(path,'.');
    int vector=extension && (!strcasecmp(extension,".svg") || !strcasecmp(extension,".svgz"));
    Imlib_Image image=vector ? ui_svg_image(path,dw,dh) : NULL;
    if (!image) { imlib_context_set_image(original); imlib_context_set_anti_alias(1); image=imlib_create_cropped_scaled_image(0,0,w,h,dw,dh); }
    imlib_context_set_image(original); imlib_free_image(); imlib_context_set_image(image);
    if (strstr(path,"-symbolic."))
    {
        DATA32 *data=imlib_image_get_data(); color_t tint=ui_color("icons.color",ui_color("colors.text",text_color));
        DATA32 rgb=(tint.r<<16)|(tint.g<<8)|tint.b;
        for (int i=0;i<dw*dh;i++) data[i]=(((data[i]>>24)*tint.a/255)<<24)|rgb;
        imlib_image_put_back_data(data);
    }
    icon_image_t *item=calloc(1,sizeof(*item)); *item=(icon_image_t){path,width,height,image,icon_images}; icon_images=item;
    imlib_context_set_cliprect(cx,cy,cw,ch); imlib_context_set_blend(blend);
    return image;
}
