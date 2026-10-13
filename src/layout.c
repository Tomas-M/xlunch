#include "ui.h"

void calculate_percentage(int maxvalue, percentable_t* percentable)
{
    if(percentable->percent != -1) {
        percentable->value = (maxvalue * percentable->percent) / 100;
    }
}

void recalc_cells()
{
    if (focus_mode && font_height) { focus_layout(); return; }
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
    // Keep configured row gaps instead of distributing unused vertical space.
    if (no_row_stretch){
        row_margin = least_v_margin;
    } else if (rows == 1){
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

void arrange_positions()
{
    if (focus_mode) { focus_positions(); return; }
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
            current->y = (border * border_ratio) / 50 + prompt_font_height + prompt_spacing + j * (cell_height+row_margin) - (int)(scrolled_past * (cell_height+row_margin));
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

void entry_viewport(int *top, int *bottom)
{
    if (focus_mode) { focus_viewport(top,bottom); return; }
    *top=(border * border_ratio)/50+prompt_font_height+prompt_spacing;
    *bottom=*top+(rows-1)*(cell_height+row_margin)+cell_height;
    if (upside_down)
    {
        int original_top=*top;
        *top=screen_height-*bottom;
        *bottom=screen_height-original_top;
    }
}

int mouse_over_cell(node_t * cell, int mouse_x, int mouse_y)
{
    if (cell->hidden) return 0;
    int top, bottom;
    entry_viewport(&top, &bottom);
    if (mouse_y < top || mouse_y >= bottom) return 0;
    if (mouse_x >= cell->x
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

double max_scroll_level()
{
    if (focus_mode) return focus_max_scroll();
    int max=(entries_count+columns-1)/columns-rows;
    return max > 0 ? max : 0;
}

void set_scroll_level(double new_scroll)
{
    if (noscroll) return;
    if (new_scroll > max_scroll_level()) new_scroll=max_scroll_level();
    if (new_scroll < 0) new_scroll=0;
    scroll_target=new_scroll;
    if (new_scroll == scrolled_past) return;
    scrolled_past=new_scroll;
    arrange_positions();
    updates=imlib_update_append_rect(updates, 0, 0, screen_width, screen_height);
}

void scroll_wheel(int direction)
{
    if (noscroll || scrollbar_drag_y >= 0) return;
    if (scroll_target == scrolled_past) clock_gettime(CLOCK_MONOTONIC, &scroll_time);
    if (focus_mode) { scroll_start=scrolled_past; clock_gettime(CLOCK_MONOTONIC,&scroll_time); }
    double step=focus_mode ? ui_number("scroll.wheel_step")*ui_scale()/(cell_height+row_margin) : 1;
    if (focus_mode && !ui_bool("scroll.smooth")) { set_scroll_level(scrolled_past+direction*step); return; }
    scroll_target+=direction*step;
    if (scroll_target > max_scroll_level()) scroll_target=max_scroll_level();
    if (scroll_target < 0) scroll_target=0;
}

scrollbar_t scrollbar_geometry()
{
    if (focus_mode) return focus_scrollbar();
    int top, bottom;
    entry_viewport(&top, &bottom);
    scrollbar_t bar={.x=screen_width-36, .y=top+8, .width=20, .height=bottom-top-16};
    int max=max_scroll_level();
    if (noscroll || !max || bar.height < 10) { bar.width=0; return bar; }
    bar.thumb_height=bar.height * (double)rows/(rows+max);
    if (bar.thumb_height < 24) bar.thumb_height=24;
    if (bar.thumb_height >= bar.height) bar.thumb_height=bar.height-1;
    bar.thumb_y=bar.y+(int)((bar.height-bar.thumb_height)*scrolled_past/max);
    return bar;
}

void drag_scrollbar(int mouse_y)
{
    scrollbar_t bar=scrollbar_geometry();
    if (!bar.width) return;
    set_scroll_level(scrollbar_drag_scroll+(double)(mouse_y-scrollbar_drag_y)*max_scroll_level()/(bar.height-bar.thumb_height));
}
