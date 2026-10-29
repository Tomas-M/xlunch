#include "ui.h"

// Decode unaligned XSettings integers in either byte order.
static unsigned settings_number(const unsigned char *data, int bytes, int msb)
{
    unsigned value=0;
    for (int i=0;i<bytes;i++) value=(value<<8)|data[msb ? i : bytes-i-1];
    return value;
}

// Read the desktop's selected icon theme without loading a GUI toolkit.
static char *xsettings_theme(void)
{
    if (!disp) return NULL;
    char selection[40]; snprintf(selection,sizeof(selection),"_XSETTINGS_S%d",DefaultScreen(disp));
    Atom property=XInternAtom(disp,"_XSETTINGS_SETTINGS",False),actual;
    unsigned char *data=NULL; unsigned long length=0,remaining=0; int format=0;
    // Keep the settings owner alive until its property has been copied.
    XGrabServer(disp);
    Window owner=XGetSelectionOwner(disp,XInternAtom(disp,selection,False));
    int status=owner ? XGetWindowProperty(disp,owner,property,0,65536,False,property,&actual,&format,&length,&remaining,&data) : BadWindow;
    XUngrabServer(disp); XFlush(disp);
    char *theme=NULL;
    int valid_property=status==Success && actual==property && format==8;
    int valid_size=!remaining && length>=12;
    if (valid_property && valid_size && data[0]<=1)
    {
        int msb=data[0]; unsigned count=settings_number(data+8,4,msb); size_t position=12;
        for (unsigned i=0;i<count && length-position>=4;i++)
        {
            unsigned type=data[position],size=settings_number(data+position+2,2,msb); position+=4;
            size_t padded=((size_t)size+3)&~(size_t)3;
            if (padded>length-position || length-position-padded<4) break;
            int match=size==strlen("Net/IconThemeName") && !memcmp(data+position,"Net/IconThemeName",size);
            position+=padded+4;
            if (type==1)
            {
                if (length-position<4) break;
                size=settings_number(data+position,4,msb); position+=4;
                if (size>length-position) break;
                padded=((size_t)size+3)&~(size_t)3;
                if (padded>length-position) break;
                if (match && size) { theme=strndup((char *)data+position,size); break; }
                position+=padded;
            }
            else
            {
                if (type==0) size=4;
                else if (type==2) size=8;
                else break;
                if (size>length-position) break;
                position+=size;
            }
        }
    }
    if (data) XFree(data);
    return theme;
}

// Read GTK's configured icon theme from one user or system settings file.
static char *settings_theme(const char *path)
{
    FILE *file=fopen(path,"r"); if (!file) return NULL;
    char *line=NULL,*theme=NULL; size_t capacity=0; int settings=0;
    while (getline(&line,&capacity,file)>=0)
    {
        char *key=line+strspn(line," \t"),*end;
        if (*key=='[') { settings=!strncmp(key,"[Settings]",10); continue; }
        char *equal=strchr(key,'='); if (!settings || !equal) continue;
        end=equal;
        while (end>key && isspace((unsigned char)end[-1])) end--;
        *end=0;
        if (strcmp(key,"gtk-icon-theme-name")) continue;
        char *value=equal+1; value+=strspn(value," \t"); end=value+strlen(value);
        while (end>value && isspace((unsigned char)end[-1])) end--;
        *end=0;
        if (end-value>=2 && *value=='"' && end[-1]=='"') { value++; *--end=0; }
        free(theme); theme=*value ? strdup(value) : NULL;
    }
    free(line); fclose(file); return theme;
}

// Keep the fallback and cache key consistent for this launcher process.
const char *ui_system_icon_theme(void)
{
    static char *theme;
    if (theme) return theme;
    theme=xsettings_theme(); if (theme) return theme;
    const char *config=getenv("XDG_CONFIG_HOME"),*dirs=getenv("XDG_CONFIG_DIRS"); char *home=NULL,*folders,*save;
    if (!config || *config!='/') { asprintf(&home,"%s/.config",getenv("HOME")); config=home; }
    asprintf(&folders,"%s:%s:/etc",config,dirs && *dirs ? dirs : "/etc/xdg"); free(home);
    for (char *root=strtok_r(folders,":",&save);root && !theme;root=strtok_r(NULL,":",&save))
    {
        if (*root!='/') continue;
        char *path; asprintf(&path,"%s/gtk-3.0/settings.ini",root);
        theme=settings_theme(path); free(path);
    }
    free(folders); if (!theme) theme=strdup("Adwaita");
    return theme;
}
