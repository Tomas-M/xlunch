#include "ui.h"

void set_clicked(node_t * cell, int clicked)
{
    if (cell==NULL) return;
    if (cell->hidden) return;
    if (cell->clicked!=clicked) updates = imlib_update_append_rect(updates, cell->x, cell->y, cell_width, cell_height);
    cell->clicked=clicked;
}

void recheckHover(XEvent ev) {
    if (focus_mode) { ev.type=MotionNotify; focus_event(&ev); return; }
    node_t * current = entries;
    int j = 1;
    int any_hovered = 0;
    scrollbar_t bar=scrollbar_geometry();
    int over_scrollbar=bar.width && ev.xmotion.x >= bar.x && ev.xmotion.x < bar.x+bar.width
                       && ev.xmotion.y >= bar.y && ev.xmotion.y < bar.y+bar.height;
    int pointer_selects_entries=mouse_moves>3 && !over_scrollbar && scrollbar_drag_y < 0;

    while (current != NULL)
    {
        if (pointer_selects_entries && mouse_over_cell(current, ev.xmotion.x, ev.xmotion.y)) {
            set_hover(j, current, 1);
            any_hovered = 1;
            hoverset=MOUSE;
        }
        else {
            set_hover(j, current,0);
            set_clicked(current,0);
        }
        if(!current->hidden)
            j++;
        current = current->next;
    }
    if (any_hovered == 0) hovered_entry = 0;

    button_t * button = buttons;
    while (button != NULL) {
        int x = (button->x < 0 ? screen_width + button->x + 1 - button->w : button->x);
        int y = (button->y < 0 ? screen_height + button->y + 1 - button->h : button->y);
        if (!over_scrollbar && scrollbar_drag_y < 0 && mouse_over_button(button, ev.xmotion.x, ev.xmotion.y)) {
            if (button->hovered != 1) updates = imlib_update_append_rect(updates, x, y, button->w, button->h);
            button->hovered = 1;
        } else {
            if (button->hovered != 0) updates = imlib_update_append_rect(updates, x, y, button->w, button->h);
            button->hovered = 0;
            button->clicked = 0;
        }
        button = button->next;
    }
}

void handleButtonPress(XEvent ev) {
    if (scrollbar_drag_y >= 0) return;
    switch (ev.xbutton.button) {
        case 3: // right click
            if (!desktop_mode) {
                cleanup();
                exit(RIGHTCLICK);
            }
            break;
        case 4: // scroll up
            scroll_wheel(-1);
            break;
        case 5: // scroll down
            scroll_wheel(1);
            break;
        case 1:; // left click
            scrollbar_t bar=scrollbar_geometry();
            // The scrollbar consumes the click even when void clicks terminate xlunch.
            if (bar.width && ev.xbutton.x >= bar.x && ev.xbutton.x < bar.x+bar.width
                && ev.xbutton.y >= bar.y && ev.xbutton.y < bar.y+bar.height)
            {
                set_scroll_level(scrolled_past);
                if (ev.xbutton.y < bar.thumb_y || ev.xbutton.y >= bar.thumb_y+bar.thumb_height)
                    set_scroll_level((double)(ev.xbutton.y-bar.y-bar.thumb_height/2)*max_scroll_level()/(bar.height-bar.thumb_height));
                scrollbar_drag_y=ev.xbutton.y;
                scrollbar_drag_scroll=scrolled_past;
                recheckHover(ev);
                return;
            }
            set_scroll_level(scrolled_past);
            node_t * current = entries;
            int voidclicked = 1;
            mouse_moves += 4;
            recheckHover(ev);
            while (current != NULL)
            {
                if (mouse_over_cell(current, ev.xmotion.x, ev.xmotion.y)) {
                    set_clicked(current,1);
                    voidclicked = 0;
                }
                else set_clicked(current,0);
                current = current->next;
            }

            button_t * button = buttons;
            button_t * lastbuttonclicked = NULL;
            while (button != NULL) {
                int x = (button->x < 0 ? screen_width + button->x + 1 - button->w : button->x);
                int y = (button->y < 0 ? screen_height + button->y + 1 - button->h : button->y);
                if (mouse_over_button(button, ev.xmotion.x, ev.xmotion.y)) {
                    if (button->clicked != 1) updates = imlib_update_append_rect(updates, x, y, button->w, button->h);
                    button->clicked = 1;
                    voidclicked = 0;
                } else {
                    if (button->clicked != 0) updates = imlib_update_append_rect(updates, x, y, button->w, button->h);
                    button->clicked = 0;
                }
                if (button->clicked) lastbuttonclicked = button;
                button = button->next;
            }

            // mark as clicked only the last (top most visible) button
            if (lastbuttonclicked != NULL) {
               button = buttons;
               while (button != NULL) {
                  button->clicked = 0;
                  button = button->next;
               }
               lastbuttonclicked->clicked = 1;
            }


            if (voidclicked && void_click_terminate) {
                cleanup();
                exit(VOIDCLICK);
            }
            break;
    }
}

void handleButtonRelease(XEvent ev) {
    if (ev.xbutton.button != Button1) return;
    if (scrollbar_drag_y >= 0)
    {
        drag_scrollbar(ev.xbutton.y);
        scrollbar_drag_y=-1;
        recheckHover(ev);
        return;
    }
    node_t * current = entries;

    while (current != NULL)
    {
        if (mouse_over_cell(current, ev.xmotion.x, ev.xmotion.y)) if (current->clicked==1) ui_activate(current);
        set_clicked(current, 0); // button release means all cells are not clicked
        current = current->next;
    }

    button_t * button = buttons;

    while (button != NULL)
    {
        if (mouse_over_button(button, ev.xmotion.x, ev.xmotion.y) && button->clicked == 1) run_command(button->cmd);
        button->clicked = 0;
        int x = (button->x < 0 ? screen_width + button->x + 1 - button->w : button->x);
        int y = (button->y < 0 ? screen_height + button->y + 1 - button->h : button->y);
        updates = imlib_update_append_rect(updates, x, y, button->w, button->h);
        button = button->next;
    }
}

void handleKeyPress(XEvent ev) {
    // keyboard events
    int count = 0;
    KeySym keycode = 0;
    Status status = 0;
    char kbdbuf[20]= {0};
    count = Xutf8LookupString(ic, (XKeyPressedEvent*)&ev, kbdbuf, 20, &keycode, &status);

    if (keycode==XK_Escape && !desktop_mode)
    {
        cleanup();
        exit(ESCAPE);
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
                if (current->hovered) selected = current;
            }
            current=current->next;
        }
        /* if only 1 app was filtered, consider it selected */
        if (nb==1 && selected_one!=NULL) ui_activate(selected_one);
        else if (hoverset==KEYBOARD && selected!=NULL) ui_activate(selected);
        // else run the command entered by commandline, if the command prompt is used
        else if (!no_prompt && !select_only) run_command(commandline);
        // or if --selectonly is specified, run first program regardless of selected state
        else if (select_only && first!=NULL) ui_activate(first);
    }

    if (keycode==XK_Tab || keycode==XK_Up || keycode==XK_Down || keycode==XK_Left || keycode==XK_Right
            || keycode==XK_KP_Up || keycode==XK_KP_Down || keycode==XK_KP_Left || keycode==XK_KP_Right
            || keycode==XK_Page_Down || keycode==XK_Page_Up || keycode==XK_Home || keycode==XK_End)
    {
        int i=0;
        if (keycode==XK_KP_Left || keycode==XK_Left) i=-1;
        if (keycode==XK_Up || keycode==XK_KP_Up) i=-columns;
        if (keycode==XK_Down || keycode==XK_KP_Down) i=columns;
        if (keycode==XK_Tab || keycode==XK_Right || keycode==XK_KP_Right) i=1;
        if (keycode==XK_Page_Up) i=-columns*rows;
        if (keycode==XK_Page_Down) i=columns*rows;
        if (keycode==XK_End) i = entries_count;//(scroll ? scrolled_past*columns+n : n);
        if (keycode==XK_Home) i = -entries_count;//(scroll ? scrolled_past*columns+1 : 1);
        if (hovered_entry == 0) {
            if (keycode != XK_End && keycode != XK_Page_Down) {
                hovered_entry = 1-i;
            } else {
                hovered_entry = 1;
            }
        }
        i = hovered_entry + i;
        hover_entry(i);
        return;
    }

    if (keycode==XK_Delete || keycode==XK_BackSpace)
        pop_key();
    else if (count>1 || (count==1 && kbdbuf[0]>=32)){ // ignore unprintable characterrs
        if (!no_prompt) push_key(kbdbuf);
        shortcut_t *shortcut = shortcuts;
        while(shortcut != NULL) {
            for(int i = 0; i < count; i++){
                if (kbdbuf[i] != shortcut->key[i]) {
                    break;
                } else if (i == count-1) {
                    ui_activate(shortcut->entry);
                }
            }
            shortcut = shortcut->next;
        }
    }

    joincmdline();
    joincmdlinetext();
    filter_entries();
    arrange_positions();

    // we used keyboard to type command. So unselect all icons.
    node_t * current = entries;
    int i = 1;
    while (current != NULL)
    {
        set_hover(i, current,0);
        set_clicked(current,0);
        current = current->next;
        i++;
    }

    updates = imlib_update_append_rect(updates, 0, 0, screen_width, screen_height);
}
