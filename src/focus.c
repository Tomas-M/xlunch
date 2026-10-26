#include "ui.h"

typedef struct navigation { char *id; const char *label,*icon; int count; ui_rect_t rect; } navigation_t;
typedef struct shape { int w,h,r,b; color_t fill,border; Imlib_Image image; struct shape *next; } shape_t;
typedef enum { TEXT_LEFT,TEXT_CENTER,TEXT_RIGHT } text_alignment_t;
static shape_t *shapes;
static Imlib_Color_Modifier uninstalled_icon_modifier;
static navigation_t *navigation;
static int navigation_count,nav_horizontal,nav_scroll,nav_extent;
static node_t **view;
static node_t *action_entries;
static int view_count,selected=-1,hovered=-1,pressed_target,keyboard_target=5000,mouse_x,mouse_y,search_selected;
static size_t query_cursor;
static int query_scroll;
static int session_open,action_selected,confirm_action=-1,confirm_choice;
static int popup_scroll;
static char **actions;
static int action_count;
static ui_rect_t header_rect,header_icon_rect,nav_rect,search_rect,heading_rect,grid_rect,footer_rect,session_rect,popup_rect,confirm_rect;
static int pointer_target(int x, int y);

// Convert profile dimensions to device pixels.
static int px(const char *key) { return (int)lround(ui_number(key)*ui_scale()); }

// Grid sessions share the application viewport; menu sessions remain a popup.
static int session_grid(void) { return session_open && !strcmp(ui_value("session.view"),"grid"); }

// Release the independent Session catalog without touching installed applications.
static void clear_action_entries(void)
{
    while (action_entries) { node_t *next=action_entries->next; free_entry(action_entries); action_entries=next; }
}

// Load configured actions into the same tile model, preserving their INI order.
static void actions_build(void)
{
    clear_action_entries();
    ui_list_free(actions,action_count); actions=ui_list(ui_value("session.items"),&action_count);
    node_t **position=&action_entries;
    for (int i=0;i<action_count;i++)
    {
        node_t *entry=calloc(1,sizeof(*entry)); entry->id=strdup(actions[i]); entry->name=strdup(ui_field("action",actions[i],"label"));
        strncpyutf8(entry->title,entry->name,sizeof(entry->title)-1);
        entry->keywords=strdup(ui_field("action",actions[i],"command"));
        char *path=ui_icon_path(ui_field("action",actions[i],"icon"),px("grid.icon_size"));
        if (path) { snprintf(entry->icon,sizeof(entry->icon),"%s",path); free(path); }
        *position=entry; position=&entry->next;
    }
}

// Use the same rectangles for drawing and pointer hit testing.
static int inside(ui_rect_t rect, int x, int y)
{
    return rect.w>0 && rect.h>0 && x>=rect.x && x<rect.x+rect.w && y>=rect.y && y<rect.y+rect.h;
}

// Preserve the explicit positioning of existing image-button configurations.
static ui_rect_t legacy_button_rect(button_t *button)
{
    return (ui_rect_t){button->x<0 ? screen_width+button->x+1-button->w : button->x,
        button->y<0 ? screen_height+button->y+1-button->h : button->y,button->w,button->h};
}

// Load scaled fonts through the existing Imlib2 font cache.
static Imlib_Font focus_font(const char *key)
{
    const char *description=ui_value(key);
    if (!*description) description=ui_value("fonts.body");
    if (!*description) description=ui_value("fonts.entries");
    const char *slash=strrchr(description,'/'); char *scaled=NULL;
    if (slash) asprintf(&scaled,"%.*s/%d",(int)(slash-description),description,(int)lround(atof(slash+1)*ui_scale()));
    Imlib_Font font=imlib_load_font(scaled ? scaled : description); free(scaled);
    if (!font) { fprintf(stderr,"Could not load Focus font: %s\n",description); exit(FONTERROR); }
    return font;
}

// Measure fonts before laying out visible components.
static int line_height(const char *key)
{
    Imlib_Font font=focus_font(key); imlib_context_set_font(font);
    int height=imlib_get_maximum_font_ascent()-imlib_get_maximum_font_descent(); imlib_free_font(); return height;
}

// Measure a rounded rectangle at pixel centers for consistent edge coverage.
static double round_distance(double x, double y, double w, double h, double radius)
{
    radius=fmin(radius,fmin(w,h)/2);
    double dx=fabs(x)-w/2+radius,dy=fabs(y)-h/2+radius;
    return hypot(fmax(dx,0),fmax(dy,0))+fmin(fmax(dx,dy),0)-radius;
}

// Rasterize fill and border together, keeping straight one-pixel lines sharp.
static Imlib_Image round_surface(int w, int h, int radius, int border_width, color_t fill, color_t border)
{
    Imlib_Image image=imlib_create_image(w,h); imlib_context_set_image(image); imlib_image_set_has_alpha(1);
    DATA32 *data=imlib_image_get_data();
    for (int y=0;y<h;y++) for (int x=0;x<w;x++)
    {
        double dx=x+.5-w/2.0,dy=y+.5-h/2.0;
        double outer=fmin(1,fmax(0,.5-round_distance(dx,dy,w,h,radius))),inner=outer;
        if (border_width) inner=w>border_width*2 && h>border_width*2
            ? fmin(outer,fmax(0,.5-round_distance(dx,dy,w-border_width*2,h-border_width*2,fmax(0,radius-border_width)))) : 0;
        double a=fill.a*inner,b=border.a*(outer-inner),alpha=a+b;
        DATA32 pixel=0;
        if (alpha>0) pixel=((DATA32)lround(alpha)<<24)|((DATA32)lround((fill.r*a+border.r*b)/alpha)<<16)
            |((DATA32)lround((fill.g*a+border.g*b)/alpha)<<8)|(DATA32)lround((fill.b*a+border.b*b)/alpha);
        data[y*w+x]=pixel;
    }
    imlib_image_put_back_data(data); return image;
}

// Cache rounded surfaces by geometry and state colors, then blend at device resolution.
static void box(Imlib_Image buffer, ui_rect_t rect, int radius, int border_width, color_t fill, color_t border, int ox, int oy)
{
    if (rect.w<=0 || rect.h<=0) return;
    shape_t *shape=shapes;
    while (shape && (shape->w!=rect.w || shape->h!=rect.h || shape->r!=radius || shape->b!=border_width
           || memcmp(&shape->fill,&fill,sizeof(fill)) || memcmp(&shape->border,&border,sizeof(border)))) shape=shape->next;
    if (!shape)
    {
        shape=calloc(1,sizeof(*shape)); shape->w=rect.w; shape->h=rect.h; shape->r=radius; shape->b=border_width;
        shape->fill=fill; shape->border=border; shape->image=round_surface(rect.w,rect.h,radius,border_width,fill,border);
        shape->next=shapes; shapes=shape;
    }
    imlib_context_set_image(buffer); imlib_context_set_blend(1); imlib_context_set_anti_alias(1);
    imlib_blend_image_onto_image(shape->image,1,0,0,rect.w,rect.h,rect.x-ox,rect.y-oy,rect.w,rect.h);
}

// Composite translucent panels over the wallpaper, keeping application labels out of menus.
static void panel(Imlib_Image buffer, ui_rect_t rect, int radius, int border_width, color_t fill, color_t border, int ox, int oy)
{
    imlib_context_set_image(background);
    Imlib_Image image=imlib_create_cropped_image(rect.x,rect.y,rect.w,rect.h);
    box(image,(ui_rect_t){0,0,rect.w,rect.h},radius,border_width,fill,border,0,0);
    imlib_context_set_image(image); imlib_image_set_has_alpha(1); DATA32 *data=imlib_image_get_data();
    for (int y=0;y<rect.h;y++) for (int x=0;x<rect.w;x++)
    {
        double coverage=fmin(1,fmax(0,.5-round_distance(x+.5-rect.w/2.0,y+.5-rect.h/2.0,rect.w,rect.h,radius)));
        data[y*rect.w+x]=(data[y*rect.w+x]&0xffffff)|((DATA32)lround(255*coverage)<<24);
    }
    imlib_image_put_back_data(data); imlib_context_set_image(buffer); imlib_context_set_blend(1);
    imlib_blend_image_onto_image(image,1,0,0,rect.w,rect.h,rect.x-ox,rect.y-oy,rect.w,rect.h);
    imlib_context_set_image(image); imlib_free_image(); imlib_context_set_image(buffer);
}

// Draw a clipped UTF-8 label, preserving complete code points when shortening it.
static void text(Imlib_Image buffer, const char *value, const char *font_key, ui_rect_t rect,
                 color_t color, text_alignment_t alignment, int ox, int oy)
{
    if (!*value || rect.w<=0 || rect.h<=0) return;
    Imlib_Font font=focus_font(font_key); imlib_context_set_font(font); imlib_context_set_image(buffer);
    char *label=strdup(value); int width,height; imlib_get_text_size(label,&width,&height);
    size_t length=strlen(value);
    while (width>rect.w && length)
    {
        length--; while (length && ((unsigned char)value[length]&0xc0)==0x80) length--;
        free(label); asprintf(&label,"%.*s…",(int)length,value); imlib_get_text_size(label,&width,&height);
    }
    if (width<=rect.w)
    {
        imlib_context_set_color(color.r,color.g,color.b,color.a);
        int x=rect.x-ox;
        if (alignment==TEXT_CENTER) x+=(rect.w-width)/2;
        else if (alignment==TEXT_RIGHT) x+=rect.w-width;
        imlib_text_draw(x,rect.y-oy+(rect.h-height)/2,label);
    }
    free(label); imlib_free_font();
}

// Measure and right-align keycaps, omitting helper labels when the footer is narrow.
static int footer_shortcuts(Imlib_Image buffer, int ox, int oy)
{
    const char *keys[]={"↑","↓","Enter","Esc"};
    const char *labels[]={"",ui_value("footer.select_text"),ui_value("footer.open_text"),ui_value("footer.close_text")};
    int widths[4],label_widths[4],height=line_height("fonts.footer")+px("footer.key_padding")*2;
    int gap=px("footer.key_gap"),group_gap=px("footer.shortcut_gap"),total=0,key_total=0;
    Imlib_Font font=focus_font("fonts.footer"); imlib_context_set_font(font);
    for (int i=0;i<4;i++)
    {
        int h; imlib_get_text_size(keys[i],&widths[i],&h); imlib_get_text_size(labels[i],&label_widths[i],&h);
        widths[i]=fmax(height,widths[i]+px("footer.key_padding")*2);
        int spacing=i==1 ? gap : group_gap;
        key_total+=widths[i]+(i ? spacing : 0);
        total+=widths[i]+(i ? spacing : 0)+(label_widths[i] ? gap+label_widths[i] : 0);
    }
    imlib_free_font();
    int available=footer_rect.w-px("footer.padding")*2,show_labels=total<=available;
    if (!show_labels) total=key_total;
    int x=footer_rect.x+footer_rect.w-px("footer.padding")-total;
    color_t muted=ui_color("footer.text_color",ui_color("colors.muted",text_color));
    imlib_context_set_cliprect(footer_rect.x+px("footer.padding")-ox,footer_rect.y-oy,available,footer_rect.h);
    for (int i=0;i<4;i++)
    {
        if (i) x+=i==1 ? gap : group_gap;
        ui_rect_t rect={x,footer_rect.y+(footer_rect.h-height)/2,widths[i],height};
        box(buffer,rect,px("footer.key_radius"),px("footer.key_border_width"),ui_color("footer.key_background",(color_t){0}),ui_color("footer.key_border",muted),ox,oy);
        text(buffer,keys[i],"fonts.footer",rect,ui_color("footer.key_text",muted),TEXT_CENTER,ox,oy);
        x+=widths[i];
        if (show_labels && label_widths[i])
        {
            x+=gap;
            text(buffer,labels[i],"fonts.footer",(ui_rect_t){x,footer_rect.y,label_widths[i],footer_rect.h},muted,TEXT_LEFT,ox,oy);
            x+=label_widths[i];
        }
    }
    imlib_context_set_cliprect(0,0,0,0); return fmin(total,available);
}

// Use identical inset, color and thickness for sidebar dividers.
static void sidebar_separator(Imlib_Image buffer, ui_rect_t rect, int y, int ox, int oy)
{
    int padding=px("sidebar.item_padding");
    box(buffer,(ui_rect_t){rect.x+padding,y,rect.w-padding*2,1},0,0,ui_color("colors.border",text_color),(color_t){0},ox,oy);
}

// Draw file or theme icons with aspect ratio preserved and cached theme resolution.
static void icon(Imlib_Image buffer, const char *name, ui_rect_t rect, int ox, int oy, int transparent)
{
    if (!*name || rect.w<=0 || rect.h<=0) return;
    Imlib_Image image=ui_icon_image(name,rect.w,rect.h); if (!image) return;
    imlib_context_set_image(image); int w=imlib_image_get_width(),h=imlib_image_get_height();
    double scale=fmin((double)rect.w/w,(double)rect.h/h); int dw=lround(w*scale),dh=lround(h*scale);
    imlib_context_set_image(buffer); imlib_context_set_blend(1);
    // Apply opacity only while blending; cached icon pixels remain unchanged.
    Imlib_Color_Modifier previous=imlib_context_get_color_modifier();
    if (transparent && !uninstalled_icon_modifier)
    {
        uninstalled_icon_modifier=imlib_create_color_modifier(); imlib_context_set_color_modifier(uninstalled_icon_modifier);
        unsigned char alpha[256]; double opacity=1-ui_number("grid.uninstalled_icon_transparency")/100.0;
        for (int i=0;i<256;i++) alpha[i]=lround(i*opacity);
        imlib_set_color_modifier_tables(NULL,NULL,NULL,alpha);
    }
    imlib_context_set_color_modifier(transparent ? uninstalled_icon_modifier : NULL);
    imlib_blend_image_onto_image(image,1,0,0,w,h,rect.x-ox+(rect.w-dw)/2,rect.y-oy+(rect.h-dh)/2,dw,dh);
    imlib_context_set_color_modifier(previous);
    imlib_context_set_image(buffer);
}

// Legacy image names are files relative to cwd, including names without a slash.
static void file_icon(Imlib_Image buffer, const char *name, ui_rect_t rect, int ox, int oy)
{
    if (!*name) return;
    char *path; asprintf(&path,"%s%s",strchr(name,'/') ? "" : "./",name);
    icon(buffer,path,rect,ox,oy,0); free(path);
}

// Count eligible catalog items without allowing the current query to change menu rows.
static int group_count(const char *id)
{
    int count=0; for (node_t *entry=entries;entry;entry=entry->next) if (ui_group_contains(entry,id)) count++;
    return count;
}

// Append a configured navigation row with its derived catalog count.
static void navigation_add(const char *id, const char *label, const char *image)
{
    navigation=realloc(navigation,(navigation_count+1)*sizeof(*navigation));
    navigation[navigation_count++]=(navigation_t){.id=strdup(id),.label=label,.icon=image,.count=group_count(id)};
}

// Rebuild navigation from configured ordering, never from the filtered view.
static void navigation_build(void)
{
    for (int i=0;i<navigation_count;i++) free(navigation[i].id);
    free(navigation); navigation=NULL; navigation_count=0;
    int count; char **order=ui_list(ui_value("sidebar.order"),&count);
    for (int i=0;i<count;i++)
    {
        if (!strcmp(order[i],"groups"))
        {
            if (!ui_bool("groups.visible")) continue;
            int total; char **ids=*ui_value("groups.order") ? ui_list(ui_value("groups.order"),&total) : ui_sections("group",&total);
            for (int j=0;j<total;j++)
                if (ui_bool("groups.show_empty") || group_count(ids[j])) navigation_add(ids[j],ui_field("group",ids[j],"label"),ui_field("group",ids[j],"icon"));
            ui_list_free(ids,total);
        }
        else
        {
            char *key; asprintf(&key,"navigation.%s.visible",order[i]);
            if (ui_bool(key))
            {
                char *label,*image; asprintf(&label,"navigation.%s.label",order[i]); asprintf(&image,"navigation.%s.icon",order[i]);
                navigation_add(order[i],ui_value(label),ui_value(image)); free(label); free(image);
            }
            free(key);
        }
    }
    ui_list_free(order,count);
}

// Derive separators from the configured list of navigation IDs.
static int navigation_separator(const char *id)
{
    int count,found=0; char **ids=ui_list(ui_value("sidebar.separator_after"),&count);
    for (int i=0;i<count;i++) if (!strcmp(ids[i],id)) found=1;
    ui_list_free(ids,count); return found;
}

// Keep the Session divider outside the scrolling navigation viewport.
static int session_separator(void)
{
    int visible=ui_bool("sidebar.visible") && ui_bool("session.visible") && action_count;
    int vertical_navigation=!nav_horizontal && navigation_count;
    return visible && vertical_navigation && ui_bool("sidebar.separator_before_session");
}

// Position navigation inside a separately scrollable vertical or horizontal viewport.
static void navigation_positions(void)
{
    int extent=0,height=px("sidebar.item_height"),gap=px("sidebar.gap");
    for (int i=0;i<navigation_count;i++)
    {
        int width=nav_horizontal ? fmin(nav_rect.w,lround(144*ui_scale())) : nav_rect.w;
        navigation[i].rect=(ui_rect_t){nav_rect.x+(nav_horizontal ? extent-nav_scroll : 0),
            nav_rect.y+(nav_horizontal ? 0 : extent-nav_scroll),width,height};
        extent+=(nav_horizontal ? width : height)+gap;
        int separator=!nav_horizontal && navigation_separator(navigation[i].id);
        if (separator && (i<navigation_count-1 || !session_separator())) extent+=gap*2;
    }
    nav_extent=extent>0 ? extent-gap : 0;
    int maximum=nav_extent-(nav_horizontal ? nav_rect.w : nav_rect.h);
    if (nav_scroll<0) nav_scroll=0;
    if (nav_scroll>maximum) nav_scroll=maximum>0 ? maximum : 0;
}

// Lay out only visible components, collapsing the sidebar to a strip on narrow windows.
void focus_layout(void)
{
    pressed_target=0; hovered=-1;
    while (shapes) { shape_t *next=shapes->next; imlib_context_set_image(shapes->image); imlib_free_image(); free(shapes); shapes=next; }
    navigation_build();
    actions_build();
    int margin=px("ui.margin")+px("ui.padding"),gap=lround(16*ui_scale()),top=margin,bottom=screen_height-margin;
    header_rect=(ui_rect_t){margin,top,screen_width-margin*2,0};
    header_icon_rect=(ui_rect_t){0};
    if (ui_bool("header.visible") && (*ui_value("header.text") || *ui_value("header.icon") || *ui_value("header.caption")))
    {
        int size=*ui_value("header.icon") ? px("header.icon_size") : 0;
        // Fit the header around the prepared image, including non-square wordmarks.
        Imlib_Image image=size>0 ? ui_icon_image(ui_value("header.icon"),size,size) : NULL;
        if (image)
        {
            imlib_context_set_image(image);
            header_icon_rect.w=imlib_image_get_width(); header_icon_rect.h=imlib_image_get_height();
        }
        header_rect.h=fmax(header_icon_rect.h,line_height(*ui_value("header.text") ? "fonts.header" : "fonts.small"))+px("header.padding")*2;
        header_icon_rect.x=header_rect.x+px("header.padding");
        header_icon_rect.y=header_rect.y+(header_rect.h-header_icon_rect.h)/2;
        top+=header_rect.h+gap;
    }
    footer_rect=(ui_rect_t){margin,bottom,screen_width-margin*2,0};
    if (ui_bool("footer.visible"))
    {
        // A dedicated bottom margin moves the footer and frees the same height for the grid.
        if (*ui_value("footer.bottom_margin")) bottom=screen_height-px("footer.bottom_margin");
        footer_rect.h=line_height("fonts.footer")+px("footer.padding")*2;
        if (ui_bool("footer.show_shortcuts")) footer_rect.h=fmax(footer_rect.h,line_height("fonts.footer")+px("footer.key_padding")*2);
        footer_rect.y=bottom-footer_rect.h;
        bottom=footer_rect.y-gap;
    }
    int left=margin,width=screen_width-margin*2;
    nav_rect=session_rect=(ui_rect_t){0}; nav_horizontal=0;
    int show_session=ui_bool("session.visible") && action_count;
    if (ui_bool("sidebar.visible") && (navigation_count || show_session))
    {
        int sidebar=px("sidebar.width"),padding=px("sidebar.padding"),item_height=px("sidebar.item_height");
        nav_horizontal=width<sidebar+px("grid.tile_min_width")+gap*2;
        if (nav_horizontal)
        {
            int session_width=show_session ? fmin(width/2,lround(144*ui_scale())) : 0;
            nav_rect=(ui_rect_t){left,top,width-session_width-(session_width ? gap : 0),item_height};
            if (session_width) session_rect=(ui_rect_t){left+width-session_width,top,session_width,item_height};
            top+=item_height+gap;
        }
        else
        {
            nav_rect=(ui_rect_t){left+padding,top,sidebar-padding*2,bottom-top};
            if (show_session)
            {
                int height=fmax(item_height,px("sidebar.icon_size")+px("sidebar.item_padding")*2);
                int spacing=navigation_count ? px("session.margin_top") : 0;
                if (session_separator()) spacing+=px("sidebar.gap")*3;
                int available=bottom-top-height-spacing;
                if (available<0) { fprintf(stderr,"Session spacing does not fit the sidebar.\n"); exit(CONFIGERROR); }
                // Follow the menu content; only a long menu needs a scrolling viewport.
                nav_rect.h=available; navigation_positions(); navigation_positions();
                nav_rect.h=fmin(nav_rect.h,nav_extent);
                session_rect=(ui_rect_t){nav_rect.x,nav_rect.y+nav_rect.h+spacing,nav_rect.w,height};
            }
            left+=sidebar+gap*2; width=screen_width-margin-left;
        }
    }
    search_rect=(ui_rect_t){left,top,width,0};
    if (ui_bool("search.visible")) { search_rect.h=px("search.height"); top+=search_rect.h+gap; }
    heading_rect=(ui_rect_t){left,top,width,0};
    if (ui_bool("heading.visible")) { heading_rect.h=line_height("fonts.heading"); top+=heading_rect.h+px("heading.gap"); }
    int reserved=ui_bool("scrollbar.visible") ? px("scrollbar.hit_width")+px("scrollbar.margin") : 0;
    grid_rect=(ui_rect_t){left,top,width-reserved,bottom-top};
    if (!strcmp(ui_value("footer.align"),"grid"))
    {
        footer_rect.x=grid_rect.x;
        footer_rect.w=screen_width-margin-footer_rect.x;
    }
    if (grid_rect.w<=0 || grid_rect.h<=0) { fprintf(stderr,"Focus components do not fit this window; reduce margins or hide components.\n"); exit(CONFIGERROR); }
    navigation_positions(); navigation_positions();
    if (!search_rect.h && keyboard_target==5000) keyboard_target=0;
    focus_positions();
    if (ui_bool("grid.visible") && selected<0 && view_count) selected=0;
}

// Build the visible view and apply smooth pixel positions without changing catalog order.
void focus_positions(void)
{
    if (!grid_rect.w || !grid_rect.h) return; // Entries can arrive before the first window layout.
    column_margin=px("grid.gap_x"); row_margin=px("grid.gap_y");
    int size=px("grid.icon_size");
    int minimum=fmax(px("grid.tile_min_width"),size+px("grid.padding")*2);
    int fixed=ucolumns;
    columns=fixed ? fixed : (grid_rect.w+column_margin)/(minimum+column_margin);
    if (columns<1) columns=1;
    if (fixed && fixed*minimum+(fixed-1)*column_margin>grid_rect.w) { fprintf(stderr,"Configured Focus columns do not fit this window.\n"); exit(CONFIGERROR); }
    cell_width=(grid_rect.w-(columns-1)*column_margin)/columns;
    cell_height=fmax(px("grid.tile_min_height"),size+px("grid.icon_text_gap")+px("grid.padding")*2+ui_number("grid.label_lines")*line_height("fonts.entries"));
    rows=grid_rect.h/(cell_height+row_margin); if (rows<1) rows=1;
    for (node_t *entry=action_entries;entry;entry=entry->next) entry->hidden=!ui_entry_search(entry);
    int global_search=*commandline && !strcmp(ui_value("search.scope"),"all");
    free(view); view=ui_visible_entries(session_grid() && !global_search ? action_entries : entries,&view_count);
    if (session_grid() && global_search)
    {
        int count; node_t **matches=ui_visible_entries(action_entries,&count);
        view=realloc(view,(view_count+count)*sizeof(*view));
        if (count) memcpy(view+view_count,matches,count*sizeof(*view));
        view_count+=count; free(matches);
    }
    // Keep each group's original order; hovering never changes availability or ordering.
    if (ui_bool("grid.installed_first") && view_count>1)
    {
        node_t **ordered=malloc(view_count*sizeof(*view)); int installed=0,missing=0;
        for (int i=0;i<view_count;i++)
        {
            node_t *entry=view[i];
            if (ui_entry_needs_install(entry)) view[missing++]=entry;
            else ordered[installed++]=entry;
        }
        memcpy(ordered+installed,view,missing*sizeof(*view)); free(view); view=ordered;
    }
    for (int i=0;i<view_count;i++)
    {
        view[i]->x=grid_rect.x+(i%columns)*(cell_width+column_margin);
        view[i]->y=grid_rect.y+(i/columns)*(cell_height+row_margin)-(int)lround(scrolled_past*(cell_height+row_margin));
    }
    if (selected>=view_count) selected=view_count-1;
}

// Share the Focus viewport with generic clipping and hit testing.
void focus_viewport(int *top, int *bottom) { *top=grid_rect.y; *bottom=grid_rect.y+grid_rect.h; }

// Preserve partial rows while clamping the last row to the bottom of the viewport.
double focus_max_scroll(void)
{
    int height=((view_count+columns-1)/columns)*(cell_height+row_margin)-row_margin;
    return height>grid_rect.h ? (double)(height-grid_rect.h)/(cell_height+row_margin) : 0;
}

// Compute track and thumb from actual content height and the configured hit width.
scrollbar_t focus_scrollbar(void)
{
    scrollbar_t bar={.x=heading_rect.x+heading_rect.w-px("scrollbar.margin")-px("scrollbar.hit_width"),
        .y=grid_rect.y+px("scrollbar.inset"),.width=px("scrollbar.hit_width"),.height=grid_rect.h-px("scrollbar.inset")*2};
    double maximum=focus_max_scroll();
    if (!ui_bool("grid.visible") || !ui_bool("scrollbar.visible") || noscroll || maximum<=0 || bar.height<2) { bar.width=0; return bar; }
    int content=((view_count+columns-1)/columns)*(cell_height+row_margin)-row_margin;
    bar.thumb_height=fmax(px("scrollbar.min_thumb_height"),(double)bar.height*grid_rect.h/content);
    if (bar.thumb_height>=bar.height) bar.thumb_height=bar.height-1;
    bar.thumb_y=bar.y+lround((bar.height-bar.thumb_height)*scrolled_past/maximum); return bar;
}

// Queue a complete Focus redraw, including selection details and navigation states.
void focus_refresh(void) { updates=imlib_update_append_rect(updates,0,0,screen_width,screen_height); }

// Replace refreshed metadata while keeping the query, category, scroll and selection identity.
void focus_catalog_changed(node_t *catalog)
{
    char *selection=selected>=0 && selected<view_count && view[selected]->id ? strdup(view[selected]->id) : NULL;
    clear_entries(); entries=catalog; ui_custom_entries(); filter_entries();
    navigation_build(); navigation_positions(); navigation_positions(); focus_positions();
    selected=-1;
    for (int i=0;i<view_count;i++) if (selection && view[i]->id && !strcmp(selection,view[i]->id)) selected=i;
    free(selection);
    if (selected<0 && view_count) selected=0;
    if (keyboard_target>=10000) keyboard_target=selected<0 ? 5000 : 10000+selected;
    // Preserve full icon opacity under a stationary pointer after the background refresh.
    int target=pointer_target(mouse_x,mouse_y); hovered=target>=10000 ? target-10000 : -1;
    pressed_target=0; hovered_entry=0;
}

// Render shared component interaction states from global colors and local overrides.
static void item_box(Imlib_Image buffer, const char *component, ui_rect_t rect, int radius, int border,
                     color_t normal, int is_hovered, int is_selected, int is_focused, int ox, int oy)
{
    color_t fill=normal,edge=ui_color("colors.border",normal); char *key;
    asprintf(&key,"%s.%s",component,!strcmp(component,"grid") ? "tile_border" : "item_border"); edge=ui_color(key,edge); free(key);
    if (is_selected)
    {
        asprintf(&key,"%s.selected_background",component); fill=ui_color(key,ui_color("colors.selected",normal)); free(key);
        asprintf(&key,"%s.selected_border",component); edge=ui_color(key,ui_color("colors.selected_border",edge)); free(key);
    }
    else if (is_hovered)
    {
        asprintf(&key,"%s.hover_background",component); fill=ui_color(key,ui_color("colors.hover",normal)); free(key);
        asprintf(&key,"%s.hover_border",component); edge=ui_color(key,edge); free(key);
    }
    if (is_focused) { asprintf(&key,"%s.focus_border",component); edge=ui_color(key,ui_color("colors.focus_border",edge)); free(key); if (!border) border=fmax(1,lround(ui_scale())); }
    box(buffer,rect,radius,border,fill,edge,ox,oy);
}

// Resolve independent text colors for normal, hover and selected component states.
static color_t item_text(const char *component, color_t normal, int hovered, int selected)
{
    char *key; asprintf(&key,"%s.%s",component,!strcmp(component,"grid") ? "tile_text" : "item_text");
    color_t color=ui_color(key,normal); free(key);
    if (selected || hovered)
    {
        asprintf(&key,"%s.%s_text",component,selected ? "selected" : "hover"); color=ui_color(key,color); free(key);
    }
    return color;
}

// Draw a fixed number of UTF-8 label lines, reserving the same height in every tile.
static void tile_label(Imlib_Image buffer, node_t *entry, int y, int ox, int oy, color_t color)
{
    int lines=ui_number("grid.label_lines"),padding=px("grid.padding"),height=line_height("fonts.entries");
    if (!lines) return;
    const char *value=entry->name ? entry->name : entry->title; size_t offset=0,length=strlen(value);
    Imlib_Font font=focus_font("fonts.entries"); imlib_context_set_font(font);
    for (int line=0;line<lines && offset<length;line++)
    {
        size_t end=length;
        if (line<lines-1)
        {
            int width;
            do { char *part=strndup(value+offset,end-offset); imlib_get_text_size(part,&width,NULL); free(part);
                 if (width<=cell_width-padding*2) break;
                 end--; while (end>offset && ((unsigned char)value[end]&0xc0)==0x80) end--;
            } while (end>offset);
            if (end==offset) { end=offset+1; while (end<length && ((unsigned char)value[end]&0xc0)==0x80) end++; }
            size_t word=end; while (word>offset && !isspace((unsigned char)value[word-1])) word--;
            if (word>offset && end<length) end=word;
        }
        char *part=strndup(value+offset,end-offset);
        text(buffer,part,"fonts.entries",(ui_rect_t){entry->x+padding,y+line*height,cell_width-padding*2,height},color,1,ox,oy);
        free(part); offset=end; while (offset<length && isspace((unsigned char)value[offset])) offset++;
        imlib_context_set_font(font);
    }
    imlib_free_font();
}

// Fade clipped tiles into the actual background, preserving transparent profiles.
static void fade_grid_edge(Imlib_Image buffer, int ox, int oy, int upper)
{
    if (!view_count) return;
    int edge=upper ? grid_rect.y : grid_rect.y+grid_rect.h;
    int remaining=upper ? edge-view[0]->y : view[view_count-1]->y+cell_height-edge;
    int height=fmin(px("grid.fade_height"),fmin(grid_rect.h,remaining));
    if (height<=0) return;
    int start=upper ? edge : edge-height;
    imlib_context_set_image(buffer);
    int width=imlib_image_get_width(),left=fmax(0,grid_rect.x-ox),right=fmin(width,grid_rect.x+grid_rect.w-ox);
    int top=fmax(0,start-oy),end=fmin(imlib_image_get_height(),start+height-oy);
    if (left>=right || top>=end) return;
    imlib_context_set_image(background); DATA32 *base=imlib_image_get_data_for_reading_only();
    imlib_context_set_image(buffer); DATA32 *pixels=imlib_image_get_data();
    for (int y=top;y<end;y++)
    {
        int distance=upper ? start+height-y-oy : y+oy-start+1;
        double progress=(double)distance/height;
        unsigned weight=lround(255*progress*progress*(3-2*progress));
        for (int x=left;x<right;x++)
        {
            DATA32 pixel=pixels[y*width+x],back=base[(y+oy)*screen_width+x+ox];
            if (pixel==back) continue;
            unsigned front_alpha=(pixel>>24)*(255-weight),back_alpha=(back>>24)*weight,total=front_alpha+back_alpha;
            DATA32 faded=((total+127)/255)<<24;
            if (total) for (int shift=0;shift<24;shift+=8)
                faded|=((((pixel>>shift)&255)*front_alpha+((back>>shift)&255)*back_alpha+total/2)/total)<<shift;
            pixels[y*width+x]=faded;
        }
    }
    imlib_image_put_back_data(pixels);
}

// Keep menu actions within the window and scroll long lists.
static ui_rect_t action_rect(int index)
{
    int padding=px("session.padding"),gap=px("session.gap"),size=px("session.icon_size");
    int margin=px("ui.margin"),width=lround(264*ui_scale()),height=fmax(size+padding,px("sidebar.item_height"));
    int rows=action_count;
    int content=rows*(height+gap)-gap+padding*2;
    int visible=fmin(content,screen_height-margin*2);
    if (popup_scroll<0) popup_scroll=0;
    if (popup_scroll>content-visible) popup_scroll=content-visible;
    popup_rect=(ui_rect_t){session_rect.x+session_rect.w+gap,session_rect.y-rows*(height+gap)-padding*2,
        width+padding*2,visible};
    if (popup_rect.w>screen_width-margin*2) popup_rect.w=screen_width-margin*2;
    if (popup_rect.x+popup_rect.w>screen_width-margin) popup_rect.x=screen_width-margin-popup_rect.w;
    if (popup_rect.y<margin) popup_rect.y=margin;
    if (popup_rect.y+popup_rect.h>screen_height-margin) popup_rect.y=screen_height-margin-popup_rect.h;
    return (ui_rect_t){popup_rect.x+padding,popup_rect.y+padding+index*(height+gap)-popup_scroll,popup_rect.w-padding*2,height};
}

// Reveal actions selected by the keyboard in long menus.
static void reveal_action(void)
{
    if (!action_count) return;
    ui_rect_t rect=action_rect(action_selected); int padding=px("session.padding");
    if (rect.y<popup_rect.y+padding) popup_scroll+=rect.y-popup_rect.y-padding;
    if (rect.y+rect.h>popup_rect.y+popup_rect.h-padding) popup_scroll+=rect.y+rect.h-popup_rect.y-popup_rect.h+padding;
    action_rect(action_selected);
}

// Share the editable text viewport between rendering and pointer positioning.
static ui_rect_t query_label(void)
{
    int padding=px("search.padding"),size=line_height("fonts.search"),gap=lround(12*ui_scale());
    return (ui_rect_t){search_rect.x+padding+size+gap,search_rect.y,
        search_rect.w-padding*2-size-gap-(ui_bool("search.show_shortcut") ? gap*5 : 0),search_rect.h};
}

// Measure a UTF-8 prefix at the same font size used by the search field.
static int query_width(size_t length)
{
    Imlib_Font font=focus_font("fonts.search"); imlib_context_set_font(font);
    char *prefix=strndup(commandline,length); int width; imlib_get_text_size(prefix,&width,NULL);
    free(prefix); imlib_free_font(); return width;
}

// Move only across complete UTF-8 characters, including at either end of the input.
static size_t query_step(size_t position, int direction)
{
    if (direction<0)
    {
        if (position) position--;
        while (position && ((unsigned char)commandline[position]&0xc0)==0x80) position--;
    }
    else if (commandline[position])
    {
        position++;
        while (((unsigned char)commandline[position]&0xc0)==0x80) position++;
    }
    return position;
}

// Place the caret at the nearest visible character boundary, accounting for text scrolling.
static void query_place(int x)
{
    int target=x-query_label().x+query_scroll,previous=0; query_cursor=0;
    while (commandline[query_cursor])
    {
        size_t next=query_step(query_cursor,1); int width=query_width(next);
        if (target<(previous+width)/2) break;
        query_cursor=next; previous=width;
    }
    search_selected=0;
}

// Draw the opt-in Focus interface over its cached background.
void focus_render(Imlib_Image buffer, int ox, int oy)
{
    int target=pointer_target(mouse_x,mouse_y);
    pointer_cursor(target!=0 && target!=5000);
    color_t foreground=ui_color("colors.text",text_color),muted=ui_color("colors.muted",foreground),transparent={0};
    int gap=lround(12*ui_scale());
    if (header_rect.h)
    {
        int padding=px("header.padding");
        box(buffer,header_rect,0,0,ui_color("header.background",transparent),transparent,ox,oy);
        box(buffer,(ui_rect_t){header_rect.x,header_rect.y+header_rect.h-px("header.separator_width"),header_rect.w,px("header.separator_width")},0,0,ui_color("header.separator_color",ui_color("colors.border",muted)),transparent,ox,oy);
        icon(buffer,ui_value("header.icon"),header_icon_rect,ox,oy,0);
        int start=header_rect.x+padding+(header_icon_rect.w ? header_icon_rect.w+px("header.gap") : 0);
        int caption_width=header_rect.w>lround(480*ui_scale()) ? lround(220*ui_scale()) : 0;
        text(buffer,ui_value("header.text"),"fonts.header",(ui_rect_t){start,header_rect.y,header_rect.w-padding*2-(start-header_rect.x)-caption_width,header_rect.h},ui_color("header.text_color",foreground),0,ox,oy);
        if (header_rect.w>lround(480*ui_scale()))
            text(buffer,ui_value("header.caption"),"fonts.small",(ui_rect_t){header_rect.x+header_rect.w-lround(220*ui_scale()),header_rect.y,lround(200*ui_scale()),header_rect.h},ui_color("header.caption_color",muted),1,ox,oy);
    }
    if (ui_bool("sidebar.visible"))
    {
        int session_divider=session_separator();
        if (nav_rect.w && nav_rect.h)
        {
            ui_rect_t surface=nav_rect;
            if (!nav_horizontal && session_rect.h) surface.h=session_rect.y+session_rect.h-surface.y;
            box(buffer,surface,px("sidebar.radius"),px("sidebar.border_width"),ui_color("sidebar.background",transparent),ui_color("sidebar.border_color",ui_color("colors.border",muted)),ox,oy);
        }
        imlib_context_set_cliprect(nav_rect.x-ox,nav_rect.y-oy,nav_rect.w,nav_rect.h);
        for (int i=0;i<navigation_count;i++)
        {
            navigation_t *nav=&navigation[i]; ui_rect_t rect=nav->rect; int padding=px("sidebar.item_padding"),size=px("sidebar.icon_size");
            if (!nav_horizontal && navigation_separator(nav->id) && !(session_divider && i==navigation_count-1))
                sidebar_separator(buffer,rect,rect.y+rect.h+px("sidebar.gap")*3/2,ox,oy);
            int active=!session_grid() && !strcmp(focus_group,nav->id),focus=keyboard_target==1000+i;
            item_box(buffer,"sidebar",rect,px("sidebar.item_radius"),px("sidebar.item_border_width"),ui_color("sidebar.item_background",transparent),inside(rect,mouse_x,mouse_y),active,focus,ox,oy);
            icon(buffer,nav->icon,(ui_rect_t){ui_bool("sidebar.show_labels") ? rect.x+padding : rect.x+(rect.w-size)/2,rect.y+(rect.h-size)/2,size,size},ox,oy,0);
            int count_width=ui_bool("sidebar.show_counts") ? lround(36*ui_scale()) : 0;
            if (ui_bool("sidebar.show_labels")) text(buffer,nav->label,"fonts.sidebar",(ui_rect_t){rect.x+padding+size+gap,rect.y,rect.w-padding*2-size-gap-count_width,rect.h},item_text("sidebar",foreground,inside(rect,mouse_x,mouse_y),active),0,ox,oy);
            if (ui_bool("sidebar.show_counts")) { char count[32]; snprintf(count,sizeof(count),"%d",nav->count); text(buffer,count,"fonts.small",(ui_rect_t){rect.x+rect.w-padding-30,rect.y,30,rect.h},muted,1,ox,oy); }
        }
        imlib_context_set_cliprect(0,0,0,0);
        if (session_rect.h)
        {
            if (session_divider)
                sidebar_separator(buffer,session_rect,session_rect.y-px("session.margin_top")-px("sidebar.gap")*3/2,ox,oy);
            color_t normal=ui_color("session.item_background",ui_color("sidebar.item_background",transparent));
            item_box(buffer,"session",session_rect,px("sidebar.item_radius"),px("session.item_border_width"),normal,inside(session_rect,mouse_x,mouse_y),session_open,keyboard_target==2000,ox,oy);
            int size=px("sidebar.icon_size"),padding=px("sidebar.item_padding");
            icon(buffer,ui_value("session.icon"),(ui_rect_t){session_rect.x+padding,session_rect.y+(session_rect.h-size)/2,size,size},ox,oy,0);
            text(buffer,ui_value("session.label"),"fonts.sidebar",(ui_rect_t){session_rect.x+padding+size+gap,session_rect.y,session_rect.w-size-padding*2-gap,session_rect.h},item_text("session",foreground,inside(session_rect,mouse_x,mouse_y),session_open),0,ox,oy);
        }
    }
    if (search_rect.h)
    {
        int over=inside(search_rect,mouse_x,mouse_y);
        item_box(buffer,"search",search_rect,px("search.radius"),px("search.border_width"),ui_color("search.background",transparent),over,0,keyboard_target==5000,ox,oy);
        int padding=px("search.padding"),size=line_height("fonts.search");
        icon(buffer,ui_value("search.icon"),(ui_rect_t){search_rect.x+padding,search_rect.y+(search_rect.h-size)/2,size,size},ox,oy,0);
        const char *value=*commandline ? commandline : ui_value("search.placeholder");
        ui_rect_t label=query_label();
        query_cursor=fmin(query_cursor,strlen(commandline));
        int caret=query_width(query_cursor),width,height;
        Imlib_Font font=focus_font("fonts.search"); imlib_context_set_font(font); imlib_get_text_size(value,&width,&height);
        query_scroll=fmax(0,fmin(query_scroll,*commandline ? width-label.w+2 : 0));
        if (keyboard_target==5000)
        {
            if (caret<query_scroll) query_scroll=caret;
            if (caret-query_scroll>=label.w-2) query_scroll=caret-label.w+2;
        }
        imlib_context_set_cliprect(label.x-ox,label.y-oy,label.w,label.h);
        if (search_selected && *commandline)
        {
            box(buffer,(ui_rect_t){label.x,label.y+(label.h-size)/2,fmin(width-query_scroll,label.w),size},px("search.radius")/2,px("search.border_width"),
                ui_color("search.selected_background",ui_color("colors.selected",transparent)),ui_color("search.selected_border",ui_color("colors.selected_border",transparent)),ox,oy);
        }
        color_t query_color=*commandline ? ui_color("search.text",foreground) : ui_color("search.placeholder_color",muted);
        if (over) query_color=ui_color("search.hover_text",query_color);
        if (search_selected) query_color=ui_color("search.selected_text",query_color);
        imlib_context_set_image(buffer); imlib_context_set_font(font); imlib_context_set_color(query_color.r,query_color.g,query_color.b,query_color.a);
        imlib_text_draw(label.x-query_scroll-ox,label.y+(label.h-height)/2-oy,value); imlib_free_font();
        if (keyboard_target==5000 && !search_selected)
        {
            box(buffer,(ui_rect_t){label.x+caret-query_scroll,label.y+(label.h-size)/2,fmax(1,lround(ui_scale())),size},0,0,ui_color("colors.accent",foreground),transparent,ox,oy);
        }
        imlib_context_set_cliprect(0,0,0,0);
        if (ui_bool("search.show_shortcut")) text(buffer,"Ctrl F","fonts.small",(ui_rect_t){search_rect.x+search_rect.w-padding-gap*5,search_rect.y,gap*5,search_rect.h},muted,1,ox,oy);
    }
    if (heading_rect.h)
    {
        const char *title=NULL;
        if (*commandline) title=ui_value("heading.search_text");
        else if (session_grid()) title=ui_value("session.label");
        if (!title && !strcmp(focus_group,"all")) title=ui_value("heading.all_text");
        if (!title)
        {
            for (int i=0;i<navigation_count;i++) if (!strcmp(focus_group,navigation[i].id)) title=navigation[i].label;
            if (!title) title=focus_group;
        }
        int count_width=ui_bool("heading.show_count") ? fmin(heading_rect.w/3,gap*10) : 0;
        text(buffer,title,"fonts.heading",(ui_rect_t){heading_rect.x,heading_rect.y,heading_rect.w-count_width,heading_rect.h},ui_color("heading.text_color",foreground),0,ox,oy);
        if (ui_bool("heading.show_count"))
        {
            const char *format=ui_value(session_grid() ? "session.count_text" : "heading.count_text"),*token=strstr(format,"{count}"); char *count;
            if (token) asprintf(&count,"%.*s%d%s",(int)(token-format),format,view_count,token+7); else count=strdup(format);
            text(buffer,count,"fonts.small",(ui_rect_t){heading_rect.x+heading_rect.w-count_width,heading_rect.y,count_width,heading_rect.h},ui_color("heading.count_color",muted),1,ox,oy); free(count);
        }
    }
    if (ui_bool("grid.visible"))
    {
        imlib_context_set_cliprect(grid_rect.x-ox,grid_rect.y-oy,grid_rect.w,grid_rect.h);
        for (int i=0;i<view_count;i++)
        {
            node_t *entry=view[i]; if (entry->y+cell_height<grid_rect.y || entry->y>grid_rect.y+grid_rect.h) continue;
            ui_rect_t rect={entry->x,entry->y,cell_width,cell_height};
            item_box(buffer,"grid",rect,px("grid.tile_radius"),px("grid.tile_border_width"),ui_color("grid.tile_background",transparent),i==hovered,i==selected,keyboard_target==10000+i,ox,oy);
            int size=px("grid.icon_size"),lines=ui_number("grid.label_lines");
            int label_height=lines*line_height("fonts.entries"),text_gap=lines ? px("grid.icon_text_gap") : 0;
            // Center the reserved icon/label block so every cell shares the same baseline.
            int top=entry->y+fmax(px("grid.padding"),(cell_height-size-text_gap-label_height)/2);
            ui_rect_t image_rect={entry->x+(cell_width-size)/2,top,size,size};
            int needs_install=ui_entry_needs_install(entry);
            if (entry->id) icon(buffer,entry->icon,image_rect,ox,oy,i!=hovered && needs_install);
            else file_icon(buffer,entry->icon,image_rect,ox,oy);
            // Reveal the install affordance for mouse hover and keyboard focus alike.
            int active=i==hovered || (hoverset==KEYBOARD && keyboard_target==10000+i);
            if (needs_install && (active || !ui_bool("grid.install_icon_hover_only")))
            {
                int badge=fmin(px("grid.install_icon_size"),size),inset=px("grid.install_icon_padding");
                icon(buffer,ui_value("grid.install_icon"),(ui_rect_t){rect.x+rect.w-inset-badge,rect.y+inset,badge,badge},ox,oy,0);
            }
            tile_label(buffer,entry,top+size+text_gap,ox,oy,item_text("grid",foreground,i==hovered,i==selected));
        }
        if (!view_count) text(buffer,!session_grid() && !strcmp(focus_group,"favorites") ? ui_value("favorites.empty_text") : ui_value("heading.empty_text"),"fonts.body",grid_rect,muted,1,ox,oy);
        imlib_context_set_cliprect(0,0,0,0);
        fade_grid_edge(buffer,ox,oy,1);
        fade_grid_edge(buffer,ox,oy,0);
    }
    scrollbar_t bar=focus_scrollbar();
    if (bar.width)
    {
        int width=px("scrollbar.width"),x=bar.x+(bar.width-width)/2;
        box(buffer,(ui_rect_t){x,bar.y,width,bar.height},px("scrollbar.radius"),0,ui_color("scrollbar.track",transparent),transparent,ox,oy);
        color_t thumb=ui_color("scrollbar.thumb",foreground);
        if (scrollbar_drag_y>=0) thumb=ui_color("scrollbar.thumb_drag",thumb);
        else if (inside((ui_rect_t){bar.x,bar.y,bar.width,bar.height},mouse_x,mouse_y)) thumb=ui_color("scrollbar.thumb_hover",thumb);
        box(buffer,(ui_rect_t){x,bar.thumb_y,width,bar.thumb_height},px("scrollbar.radius"),0,thumb,transparent,ox,oy);
    }
    if (footer_rect.h)
    {
        box(buffer,footer_rect,0,0,ui_color("footer.background",transparent),transparent,ox,oy);
        box(buffer,(ui_rect_t){footer_rect.x,footer_rect.y,footer_rect.w,px("footer.separator_width")},0,0,ui_color("footer.separator_color",ui_color("colors.border",muted)),transparent,ox,oy);
        int padding=px("footer.padding"),width=footer_rect.w-padding*2;
        if (ui_bool("footer.show_shortcuts")) width-=footer_shortcuts(buffer,ox,oy)+px("footer.shortcut_gap");
        if (ui_bool("footer.show_selection") && selected>=0 && selected<view_count)
        {
            const char *value=view[selected]->name ? view[selected]->name : view[selected]->title; char *details=NULL;
            if (ui_bool("footer.show_description") && view[selected]->description && *view[selected]->description)
            {
                asprintf(&details,"%s · %s",value,view[selected]->description); value=details;
            }
            text(buffer,value,"fonts.footer",(ui_rect_t){footer_rect.x+padding,footer_rect.y,width,footer_rect.h},ui_color("footer.text_color",muted),0,ox,oy);
            free(details);
        }
    }
    for (button_t *button=buttons;button;button=button->next)
    {
        ui_rect_t rect=legacy_button_rect(button);
        if (button->clicked) { rect.x+=2; rect.y+=2; rect.w-=4; rect.h-=4; }
        file_icon(buffer,button->hovered && *button->icon_highlight ? button->icon_highlight : button->icon_normal,rect,ox,oy);
    }
    if (session_open && !session_grid() && action_count)
    {
        action_rect(0); panel(buffer,popup_rect,px("session.radius"),lround(ui_scale()),ui_color("session.background",transparent),ui_color("colors.border",muted),ox,oy);
        int padding=px("session.padding");
        imlib_context_set_cliprect(popup_rect.x+padding-ox,popup_rect.y+padding-oy,popup_rect.w-padding*2,popup_rect.h-padding*2);
        for (int i=0;i<action_count;i++)
        {
            ui_rect_t rect=action_rect(i); int size=px("session.icon_size"),padding=px("session.padding");
            item_box(buffer,"session",rect,px("session.radius")/2,px("session.item_border_width"),ui_color("session.item_background",transparent),inside(rect,mouse_x,mouse_y),action_selected==i,0,ox,oy);
            icon(buffer,ui_field("action",actions[i],"icon"),(ui_rect_t){rect.x+padding/2,rect.y+(rect.h-size)/2,size,size},ox,oy,0);
            text(buffer,ui_field("action",actions[i],"label"),"fonts.body",(ui_rect_t){rect.x+size+padding,rect.y,rect.w-size-padding*2,rect.h},item_text("session",foreground,inside(rect,mouse_x,mouse_y),action_selected==i),0,ox,oy);
        }
        imlib_context_set_cliprect(0,0,0,0);
    }
    if (confirm_action>=0)
    {
        box(buffer,(ui_rect_t){0,0,screen_width,screen_height},0,0,(color_t){0,0,0,160},transparent,ox,oy);
        confirm_rect=(ui_rect_t){(screen_width-fmin(screen_width-gap*4,lround(420*ui_scale())))/2,(screen_height-lround(220*ui_scale()))/2,fmin(screen_width-gap*4,lround(420*ui_scale())),lround(220*ui_scale())};
        panel(buffer,confirm_rect,px("session.radius"),1,ui_color("colors.surface",transparent),ui_color("colors.border",muted),ox,oy);
        text(buffer,ui_value("labels.confirm_title"),"fonts.heading",(ui_rect_t){confirm_rect.x+gap*2,confirm_rect.y+gap,confirm_rect.w-gap*4,gap*4},foreground,0,ox,oy);
        const char *message=ui_field("action",actions[confirm_action],"confirm_text"); if (!*message) message=ui_field("action",actions[confirm_action],"label");
        text(buffer,message,"fonts.body",(ui_rect_t){confirm_rect.x+gap*2,confirm_rect.y+gap*5,confirm_rect.w-gap*4,gap*4},muted,0,ox,oy);
        for (int i=0;i<2;i++)
        {
            ui_rect_t button={confirm_rect.x+gap*2+i*(confirm_rect.w-gap*3)/2,confirm_rect.y+confirm_rect.h-gap*5,(confirm_rect.w-gap*5)/2,gap*3};
            item_box(buffer,"session",button,px("session.radius")/2,1,transparent,inside(button,mouse_x,mouse_y),confirm_choice==i,0,ox,oy);
            text(buffer,ui_value(i ? "labels.confirm_accept" : "labels.confirm_cancel"),"fonts.body",button,foreground,1,ox,oy);
        }
    }
}

// Build the Focus background once per resize, keeping scrolling frames inexpensive.
void focus_background(void)
{
    if (background) { imlib_context_set_image(background); imlib_free_image(); }
    background=imlib_create_image(screen_width,screen_height); imlib_context_set_image(background); imlib_image_set_has_alpha(1);
    color_t color=ui_color("background.color",background_color); imlib_context_set_color(color.r,color.g,color.b,color.a);
    imlib_context_set_blend(0); imlib_image_fill_rectangle(0,0,screen_width,screen_height);
    if (use_root_img)
    {
        DATA32 *data=imlib_image_get_data(); if (get_root_image_to_imlib_data(data)) imlib_image_put_back_data(data);
    }
    if (*background_file)
    {
        Imlib_Image wallpaper=imlib_load_image(background_file);
        if (!wallpaper) { fprintf(stderr,"Cannot load Focus background: %s\n",background_file); exit(CONFIGERROR); }
        imlib_context_set_image(wallpaper); int w=imlib_image_get_width(),h=imlib_image_get_height();
        const char *placement=ui_value("background.placement"); int dw=screen_width,dh=screen_height;
        if (!strcmp(placement,"center")) { dw=w; dh=h; }
        else if (strcmp(placement,"stretch"))
        {
            double scale=!strcmp(placement,"fit") ? fmin((double)screen_width/w,(double)screen_height/h) : fmax((double)screen_width/w,(double)screen_height/h);
            dw=lround(w*scale); dh=lround(h*scale);
        }
        imlib_context_set_image(background); imlib_context_set_blend(1);
        imlib_blend_image_onto_image(wallpaper,1,0,0,w,h,(screen_width-dw)/2,(screen_height-dh)/2,dw,dh);
        imlib_context_set_image(wallpaper); imlib_free_image(); imlib_context_set_image(background);
    }
    if (ui_number("background.blur")) imlib_image_blur(px("background.blur"));
    color=ui_color("background.overlay",(color_t){0}); imlib_context_set_color(color.r,color.g,color.b,color.a);
    imlib_context_set_blend(1); imlib_image_fill_rectangle(0,0,screen_width,screen_height);
}

// Select an application and reveal its whole row within the actual viewport.
void focus_select(int index)
{
    if (!view_count || !ui_bool("grid.visible")) { selected=-1; return; }
    selected=index<1 ? 0 : index>view_count ? view_count-1 : index-1;
    keyboard_target=10000+selected; hoverset=KEYBOARD; hovered_entry=selected+1;
    double row=(selected/columns),bottom=row+(double)cell_height/(cell_height+row_margin);
    if (row<scrolled_past) set_scroll_level(row);
    if (bottom>scrolled_past+(double)grid_rect.h/(cell_height+row_margin))
        set_scroll_level(bottom-(double)grid_rect.h/(cell_height+row_margin));
    focus_refresh();
}

// Keep a keyboard-focused navigation row inside its own scroll viewport.
static void reveal_navigation(int index)
{
    ui_rect_t rect=navigation[index].rect;
    int start=nav_horizontal ? rect.x-nav_rect.x : rect.y-nav_rect.y;
    int end=start+(nav_horizontal ? rect.w : rect.h),length=nav_horizontal ? nav_rect.w : nav_rect.h;
    if (start<0) nav_scroll+=start;
    if (end>length) nav_scroll+=end-length;
    navigation_positions(); navigation_positions(); focus_refresh();
}

// Cycle only through controls that actually exist in the current layout.
static void next_target(int backwards)
{
    int button_count=0; for (button_t *button=buttons;button;button=button->next) button_count++;
    int *targets=malloc((navigation_count+view_count+button_count+2)*sizeof(*targets)),count=0,current=-1;
    if (ui_bool("sidebar.visible")) for (int i=0;i<navigation_count;i++) targets[count++]=1000+i;
    if (session_rect.h) targets[count++]=2000;
    for (int i=0;i<button_count;i++) targets[count++]=6000+i;
    if (search_rect.h) targets[count++]=5000;
    if (ui_bool("grid.visible")) for (int i=0;i<view_count;i++) targets[count++]=10000+i;
    for (int i=0;i<count;i++) if (targets[i]==keyboard_target) current=i;
    if (count)
    {
        current=current<0 ? (backwards ? count-1 : 0) : (current+count+(backwards ? -1 : 1))%count;
        keyboard_target=targets[current];
        if (keyboard_target>=10000) focus_select(keyboard_target-10000+1);
        else if (keyboard_target>=1000 && keyboard_target<2000) reveal_navigation(keyboard_target-1000);
    }
    free(targets); search_selected=0; focus_refresh();
}

// Refresh filtering and selection after editing a query or changing groups.
static void query_changed(void)
{
    scrollbar_drag_y=-1;
    joincmdline(); joincmdlinetext(); filter_entries(); arrange_positions();
    selected=view_count ? 0 : -1; hovered=-1; pressed_target=0; focus_refresh();
}

// Clear the input without leaving an invisible search target behind.
static void clear_query(void)
{
    while (cmdline) pop_key();
    search_selected=0; query_cursor=0; query_scroll=0; query_changed();
}

// Edit shared input nodes at UTF-8 boundaries without changing the classic append-only path.
static void query_edit(size_t start, size_t end, const char *input)
{
    keynode_t **position=&cmdline; size_t offset=0;
    while (*position && offset<start) { offset+=strlen((*position)->key); position=&(*position)->next; }
    while (*position && offset<end)
    {
        keynode_t *removed=*position; offset+=strlen(removed->key); *position=removed->next; free(removed);
    }
    for (const char *p=input;*p;)
    {
        const char *next=p+1; while (((unsigned char)*next&0xc0)==0x80) next++;
        keynode_t *node=malloc(sizeof(*node)); memcpy(node->key,p,next-p); node->key[next-p]=0;
        node->next=*position; *position=node; position=&node->next; p=next;
    }
    query_cursor=start+strlen(input); search_selected=0; keyboard_target=search_rect.h ? 5000 : 0; query_changed();
}

// Apply optional per-action confirmation before execution or stdout selection.
static void activate_action(int index, int confirmed)
{
    if (index<0 || index>=action_count) return;
    if (!confirmed && !strcmp(ui_field("action",actions[index],"confirm"),"true"))
    {
        confirm_action=index; confirm_choice=0; focus_refresh(); return;
    }
    confirm_action=-1; if (!session_grid()) session_open=0; pressed_target=0;
    ui_action(ui_field("action",actions[index],"command"),ui_field("action",actions[index],"output")); focus_refresh();
}

// Resolve the topmost interactive control at a pointer position.
static int pointer_target(int x, int y)
{
    if (confirm_action>=0)
    {
        int gap=lround(12*ui_scale());
        for (int i=0;i<2;i++)
        {
            ui_rect_t rect={confirm_rect.x+gap*2+i*(confirm_rect.w-gap*3)/2,confirm_rect.y+confirm_rect.h-gap*5,(confirm_rect.w-gap*5)/2,gap*3};
            if (inside(rect,x,y)) return 4000+i;
        }
        return 0;
    }
    if (session_open && !session_grid())
    {
        action_rect(0);
        if (inside(popup_rect,x,y)) for (int i=0;i<action_count;i++) if (inside(action_rect(i),x,y)) return 3000+i;
        if (inside(session_rect,x,y)) return 2000;
        return 0;
    }
    int index=0;
    for (button_t *button=buttons;button;button=button->next,index++) if (inside(legacy_button_rect(button),x,y)) return 6000+index;
    if (inside(session_rect,x,y)) return 2000;
    if (inside(nav_rect,x,y)) for (int i=0;i<navigation_count;i++) if (inside(navigation[i].rect,x,y)) return 1000+i;
    if (inside(search_rect,x,y)) return 5000;
    if (ui_bool("grid.visible") && inside(grid_rect,x,y))
        for (int i=0;i<view_count;i++) if (inside((ui_rect_t){view[i]->x,view[i]->y,cell_width,cell_height},x,y)) return 10000+i;
    return 0;
}

// Activate a control only when press and release identify the same target.
static void activate_target(int target)
{
    if (target>=10000)
    {
        int index=target-10000;
        if (index<view_count)
        {
            selected=index;
            int action=0;
            for (node_t *entry=action_entries;entry;entry=entry->next,action++)
                if (entry==view[index]) { activate_action(action,0); return; }
            ui_activate(view[index]);
        }
        return;
    }
    if (target>=6000)
    {
        int index=0;
        for (button_t *button=buttons;button;button=button->next,index++) if (target==6000+index) { run_command(button->cmd); return; }
        return;
    }
    if (target>=4000 && target<4002)
    {
        if (target==4001) activate_action(confirm_action,1);
        else { confirm_action=-1; focus_refresh(); }
        return;
    }
    if (target>=3000 && target<3000+action_count) { activate_action(target-3000,0); return; }
    if (target==2000)
    {
        if (!strcmp(ui_value("session.view"),"grid")) { session_open=1; clear_query(); }
        else { session_open=!session_open; action_selected=0; popup_scroll=0; focus_refresh(); }
        return;
    }
    if (target>=1000 && target<1000+navigation_count)
    {
        snprintf(focus_group,sizeof(focus_group),"%s",navigation[target-1000].id); keyboard_target=target;
        session_open=0;
        query_changed(); return;
    }
    if (target==5000) { keyboard_target=target; search_selected=0; focus_refresh(); }
}

// Handle Focus keyboard navigation, explicit command entry and UTF-8 editing.
static void focus_key(XEvent *event)
{
    query_cursor=fmin(query_cursor,strlen(commandline));
    KeySym key=NoSymbol; Status status=0; char *input=calloc(128,1); int count=Xutf8LookupString(ic,&event->xkey,input,127,&key,&status);
    if (status==XBufferOverflow) { input=realloc(input,count+1); count=Xutf8LookupString(ic,&event->xkey,input,count,&key,&status); }
    input[count]=0;
    if (confirm_action>=0)
    {
        if (key==XK_Escape) confirm_action=-1;
        else if (key==XK_Tab || key==XK_Left || key==XK_Right) confirm_choice=!confirm_choice;
        else if (key==XK_Return || key==XK_KP_Enter) activate_target(4000+confirm_choice);
        free(input); focus_refresh(); return;
    }
    if (session_open && !session_grid())
    {
        if (key==XK_Escape) session_open=0;
        else if (action_count && (key==XK_Tab || key==XK_Down || key==XK_Right)) action_selected=(action_selected+1)%action_count;
        else if (action_count && (key==XK_Up || key==XK_Left)) action_selected=(action_selected+action_count-1)%action_count;
        else if (key==XK_Return || key==XK_KP_Enter) activate_action(action_selected,0);
        reveal_action(); free(input); focus_refresh(); return;
    }
    if (key==XK_Escape)
    {
        if (*commandline) clear_query();
        else if (session_grid()) { session_open=0; query_changed(); }
        else if (!desktop_mode) { free(input); cleanup(); exit(ESCAPE); }
        free(input); return;
    }
    if ((event->xkey.state&ControlMask) && (key==XK_f || key==XK_F) && ui_bool("search.enabled"))
    {
        keyboard_target=search_rect.h ? 5000 : 0; search_selected=!!*commandline; free(input); focus_refresh(); return;
    }
    if ((event->xkey.state&ControlMask) && (key==XK_a || key==XK_A)) { search_selected=1; free(input); focus_refresh(); return; }
    if ((event->xkey.state&ControlMask) && (key==XK_u || key==XK_U)) { clear_query(); free(input); return; }
    if (key==XK_Tab || key==XK_ISO_Left_Tab)
    {
        next_target((event->xkey.state&ShiftMask) || key==XK_ISO_Left_Tab); free(input); return;
    }
    if (key==XK_Return || key==XK_KP_Enter)
    {
        if (keyboard_target==5000 || keyboard_target==0)
        {
            if (!ui_command(commandline) && ui_bool("grid.visible") && view_count) activate_target(10000+(selected>=0 ? selected : 0));
        }
        else activate_target(keyboard_target);
        free(input); return;
    }
    int caret_key=key==XK_Left || key==XK_Right || key==XK_Home || key==XK_End;
    if (keyboard_target==5000 && caret_key)
    {
        if (key==XK_Home || (search_selected && key==XK_Left)) query_cursor=0;
        else if (key==XK_End || search_selected) query_cursor=strlen(commandline);
        else query_cursor=query_step(query_cursor,key==XK_Left ? -1 : 1);
        search_selected=0; free(input); focus_refresh(); return;
    }
    int navigation_key=key==XK_Left || key==XK_Right || key==XK_Up || key==XK_Down || key==XK_Home || key==XK_End || key==XK_Page_Up || key==XK_Page_Down;
    if (navigation_key)
    {
        if (keyboard_target==2000 && session_grid() && (key==XK_Right || key==XK_Down)) focus_select(1);
        else if (keyboard_target>=1000 && keyboard_target<2000 && navigation_count)
        {
            int index=keyboard_target-1000;
            if (key==XK_Right && !nav_horizontal) focus_select(1);
            else { index=(index+navigation_count+((key==XK_Up || key==XK_Left) ? -1 : 1))%navigation_count; keyboard_target=1000+index; reveal_navigation(index); }
        }
        else
        {
            int index=selected<0 ? 0 : selected,step=0;
            if (key==XK_Left) step=-1;
            if (key==XK_Right) step=1;
            if (key==XK_Up) step=-columns;
            if (key==XK_Down) step=columns;
            if (key==XK_Page_Up) step=-columns*rows;
            if (key==XK_Page_Down) step=columns*rows;
            if (key==XK_Home) index=0;
            if (key==XK_End) index=view_count-1;
            focus_select(index+step+1);
        }
        free(input); return;
    }
    if (!ui_bool("search.enabled")) { free(input); return; }
    if (key==XK_BackSpace || key==XK_Delete)
    {
        if (search_selected) clear_query();
        else if (key==XK_BackSpace) query_edit(query_step(query_cursor,-1),query_cursor,"");
        else query_edit(query_cursor,query_step(query_cursor,1),"");
    }
    else if (count>0 && !(event->xkey.state&(ControlMask|Mod1Mask)) && (unsigned char)input[0]>=32)
    {
        if (search_selected) clear_query();
        if (strlen(commandline)+count<sizeof(commandline)) query_edit(query_cursor,query_cursor,input);
    }
    free(input);
}

// Consume Focus interactions before classic handlers while retaining the shared X11 loop.
int focus_event(XEvent *event)
{
    if (event->type==KeyPress) { focus_key(event); return 1; }
    if (event->type==MotionNotify || event->type==EnterNotify || event->type==LeaveNotify)
    {
        mouse_x=event->type==LeaveNotify ? -1 : event->xmotion.x; mouse_y=event->type==LeaveNotify ? -1 : event->xmotion.y;
        if (scrollbar_drag_y>=0 && event->type==MotionNotify) drag_scrollbar(mouse_y);
        int target=pointer_target(mouse_x,mouse_y); hovered=target>=10000 ? target-10000 : -1;
        if (hovered>=0) { selected=hovered; hoverset=MOUSE; }
        if (target>=3000 && target<4000) action_selected=target-3000;
        int index=0;
        for (button_t *button=buttons;button;button=button->next,index++) { button->hovered=target==6000+index; button->clicked=pressed_target==target && button->hovered; }
        focus_refresh(); return 1;
    }
    if (event->type==ButtonPress)
    {
        mouse_x=event->xbutton.x; mouse_y=event->xbutton.y;
        if (event->xbutton.button==4 || event->xbutton.button==5)
        {
            int direction=event->xbutton.button==4 ? -1 : 1;
            if (session_open && !session_grid() && confirm_action<0 && inside(popup_rect,mouse_x,mouse_y))
            {
                popup_scroll+=direction*px("sidebar.item_height"); action_rect(0); focus_refresh();
            }
            else if ((!session_open || session_grid()) && confirm_action<0 && inside(nav_rect,mouse_x,mouse_y))
            {
                nav_scroll+=direction*px("sidebar.item_height"); navigation_positions(); navigation_positions(); focus_refresh();
            }
            else if ((!session_open || session_grid()) && confirm_action<0) scroll_wheel(direction);
            return 1;
        }
        if (event->xbutton.button==3)
        {
            if (session_open) { session_open=0; query_changed(); }
            else if (!desktop_mode) { cleanup(); exit(RIGHTCLICK); }
            return 1;
        }
        if (event->xbutton.button!=1) return 1;
        if ((!session_open || session_grid()) && confirm_action<0) set_scroll_level(scrolled_past);
        scrollbar_t bar=focus_scrollbar();
        if ((!session_open || session_grid()) && confirm_action<0 && bar.width && inside((ui_rect_t){bar.x,bar.y,bar.width,bar.height},mouse_x,mouse_y))
        {
            set_scroll_level(scrolled_past);
            if (mouse_y<bar.thumb_y || mouse_y>=bar.thumb_y+bar.thumb_height)
                set_scroll_level((double)(mouse_y-bar.y-bar.thumb_height/2)*focus_max_scroll()/(bar.height-bar.thumb_height));
            scrollbar_drag_y=mouse_y; scrollbar_drag_scroll=scrolled_past; pressed_target=0; focus_refresh(); return 1;
        }
        int target=pointer_target(mouse_x,mouse_y);
        if (session_open && !session_grid() && !target) { session_open=0; pressed_target=0; focus_refresh(); return 1; }
        pressed_target=target; if (target) keyboard_target=target;
        if (target==5000) query_place(mouse_x);
        if (!target && confirm_action<0 && void_click_terminate) { cleanup(); exit(VOIDCLICK); }
        focus_refresh(); return 1;
    }
    if (event->type==ButtonRelease)
    {
        if (event->xbutton.button!=1) return 1;
        if (scrollbar_drag_y>=0) { drag_scrollbar(event->xbutton.y); scrollbar_drag_y=-1; focus_refresh(); return 1; }
        int target=pointer_target(event->xbutton.x,event->xbutton.y),pressed=pressed_target; pressed_target=0;
        if (pressed && target==pressed) activate_target(target);
        focus_refresh(); return 1;
    }
    return 0;
}

// Free Focus-owned catalogs, navigation and cached shapes before the X11 connection closes.
void focus_cleanup(void)
{
    if (uninstalled_icon_modifier) { imlib_context_set_color_modifier(uninstalled_icon_modifier); imlib_free_color_modifier(); uninstalled_icon_modifier=NULL; }
    clear_action_entries(); free(view); view=NULL;
    ui_list_free(actions,action_count); actions=NULL; action_count=0;
    for (int i=0;i<navigation_count;i++) free(navigation[i].id);
    free(navigation); navigation=NULL; navigation_count=0;
    while (shapes) { shape_t *next=shapes->next; imlib_context_set_image(shapes->image); imlib_free_image(); free(shapes); shapes=next; }
}
