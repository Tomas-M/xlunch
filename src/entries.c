#include "ui.h"

// Keep the byte used to distinguish actual stdin data from an already-empty stream.
static int input_byte=-1;

char* strncpyutf8(char* dst, const char* src, size_t num)
{
    if(num)
    {
        size_t sizeSrc = strlen(src); // number of bytes not including null
        while(sizeSrc>num)
        {
            const char* lastByte = src + sizeSrc; // initially \0 at end

            while(lastByte-- > src) // test previous chars
                if((*lastByte & 0xC0) != 0x80) // utf8 start byte found
                    break;

            sizeSrc = lastByte-src;
        }
        memcpy(dst, src, sizeSrc);
        dst[sizeSrc] = '\0';
    }
    return dst;
}

void push_key(char * key)
{
    keynode_t * current = cmdline;

    if (current==NULL)
    {
        cmdline = malloc(sizeof(keynode_t));
        strcpy(cmdline->key,key);
        cmdline->next = NULL;
        return;
    }

    while (current->next != NULL) {
        current = current->next;
    }

    /* now we can add a new item */
    current->next = malloc(sizeof(keynode_t));
    strcpy(current->next->key,key);
    current->next->next = NULL;
}

void pop_key()
{
    if (cmdline==NULL) return;

    // if there is only one item, remove it
    if (cmdline->next==NULL)
    {
        free(cmdline);
        cmdline=NULL;
        return;
    }

    keynode_t * current = cmdline;

    while (current->next->next != NULL) {
        current = current->next;
    }

    /* now we can remove last item */
    free(current->next);
    current->next=NULL;
}

// Free metadata together with the entry that owns it.
void free_entry(node_t *entry)
{
    for (shortcut_t *shortcut=shortcuts;shortcut;shortcut=shortcut->next)
        if (shortcut->entry==entry) shortcut->entry=NULL;
    free(entry->id); free(entry->name); free(entry->categories);
    free(entry->description); free(entry->keywords); free(entry->desktop_file); free(entry->desktop_exec); free(entry->executable);
    if (clear_memory) memset(entry,0,sizeof(*entry));
    free(entry);
}

void clear_entries(){
    node_t * current = entries;
    while (current != NULL) {
        node_t * last = current;
        current = current->next;
        free_entry(last);
    }
    entries = NULL;
}

// Stream resets replace anonymous DSV items while retaining configured custom entries.
static void clear_input_entries(void)
{
    node_t **position=&entries;
    while (*position)
    {
        node_t *entry=*position;
        if (entry->id) position=&entry->next;
        else { *position=entry->next; free_entry(entry); }
    }
}

void push_entry(node_t * new_entry)//(char * title, char * icon, char * cmd, int x, int y)
{
    node_t * current = entries;
    int hasicon = (strlen(new_entry->icon) != 0);
    /* Pre-load the image into the cache, this is done to check for error messages
     * If a user has more images then can be shown this might incur a performance hit */
    if (hasicon) {
        Imlib_Image image = load_image(new_entry->icon);
        if (image == NULL) {
            if (hide_missing) { free_entry(new_entry); return; }
            strcpy(new_entry->icon, "/usr/share/icons/hicolor/48x48/apps/xlunch_ghost.png");
        }
    }

    // Set default values for internal state
    new_entry->x=0;
    new_entry->y=0;
    new_entry->hidden=0;
    new_entry->hovered=0;
    new_entry->clicked=0;
    new_entry->display_title[0]='\0';
    new_entry->text_cell_width=-1;
    new_entry->next = NULL;

    shortcut_t *shortcut = shortcuts;
    while (shortcut != NULL) {
        if (shortcut->entry == NULL) {
            shortcut->entry = new_entry;
            break;
        }
        shortcut = shortcut->next;
    }

    // empty list, add first directly
    if (current==NULL)
    {
        entries = new_entry;
        entries->next = NULL;
        return;
    }

    // Otherwise determine position
    if(!reverse){
        // Go to end of list and add there
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_entry;
        current->next->next = NULL;
    } else {
        // Add at head of list
        new_entry->next = entries;
        entries = new_entry;
    }
    entries_count++;
}

char *strtok_new(char * string, char const * delimiter){
   static char *source = NULL;
   char *p, *riturn = 0;
   if(string != NULL)         source = string;
   if(source == NULL)         return NULL;

   if((p = strpbrk (source, delimiter)) != NULL) {
      *p  = 0;
      riturn = source;
      source = ++p;
   }
    return riturn;
}

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the zero-terminator
    if(result != 0) {
        strcpy(result, s1);
        strcat(result, s2);
        return result;
    }
    exit(ALLOCERROR);
}

FILE * determine_input_source(){
    FILE * fp;
    if(input_source == NULL) {
        char * homeconf = NULL;

        char * home = getenv("HOME");
        if (home!=NULL)
        {
            homeconf = concat(home,"/.config/xlunch/entries.dsv");
        }
        size_t input_file_len = strlen(input_file);
        if (input_file_len == 0 && nostdin) {
            fp = fopen(homeconf, "rb");
        } else if (input_file_len == 0) {
            fp = stdin;
            int flags;
            int fd = fileno(fp);
            flags = fcntl(fd, F_GETFL, 0);
            flags |= O_NONBLOCK;
            fcntl(fd, F_SETFL, flags);
            struct pollfd fds;
            fds.fd = 0; /* this is STDIN */
            fds.events = POLLIN;
            // Give poll a little timeout to make give the piping program some time
            if (poll(&fds, 1, stdin_poll_timeout)>0)
            {
                unsigned char byte;
                if (read(fd,&byte,1)==1) input_byte=byte;
            }
            if (input_byte<0){
                int flags = fcntl(fd, F_GETFL, 0);
                flags &= ~O_NONBLOCK;
                fcntl(fd, F_SETFL, flags);
                fclose(stdin);
                fp = fopen(homeconf, "rb");
            }
        } else {
            fp = fopen(input_file, "rb");
        }
        if (fp == NULL)
        {
            fprintf(stderr, "Error getting entries from %s.\nReverting back to system entries list.\n", strlen(input_file) == 0 ? "stdin" : input_file);
            input_file = "/etc/xlunch/entries.dsv";
            fp = fopen(input_file, "rb");

            if (fp == NULL)
            {
                fprintf(stderr, "Error opening entries file %s\n", input_file);
                fprintf(stderr, "You may need to create it. Icon file format is following:\n");
                fprintf(stderr, "title;icon_path;command\n");
                fprintf(stderr, "title;icon_path;command\n");
                fprintf(stderr, "title;icon_path;command\n");
            }
        }
        free(homeconf);
    } else {
        fp = input_source;
    }
    return fp;
}

void joincmdline()
{
    strcpy(commandline,"");

    keynode_t * current = cmdline;

    while (current != NULL) {
        strcat(commandline,current->key);
        current = current->next;
    }
}

void joincmdlinetext()
{
    if (no_prompt) return;
    if (strlen(prompt)==0) prompt="Run:  ";
    strcpy(commandlinetext,prompt);

    keynode_t * current = cmdline;

    while (current != NULL) {
        strcat(commandlinetext,current->key);
        current = current->next;
    }

    strcat(commandlinetext,"_");
}

void set_hover(int hovered, node_t * cell, int hover)
{
    if (hover) hovered_entry = hovered;
    if (cell==NULL) return;
    if (cell->hidden) return;
    if (cell->hovered!=hover) updates = imlib_update_append_rect(updates, cell->x, cell->y, cell_width, cell_height);
    cell->hovered=hover;
}

void hover_entry(int entry){
    if (focus_mode) { focus_select(entry); return; }
    hoverset = KEYBOARD;
    if (!entries_count) { hovered_entry=0; return; }
    if (entry < 1) entry=1;
    if (entry > entries_count) entry=entries_count;
    if (noscroll && entry > rows*columns) entry=rows*columns;
    set_scroll_level(scrolled_past);
    int to_row=(entry-1)/columns;
    if (to_row < scrolled_past) set_scroll_level(to_row);
    if (to_row > scrolled_past+rows-1) set_scroll_level(to_row-rows+1);
    int j = 1;
    node_t * current = entries;
    while (current != NULL) {
        set_hover(entry, current, j == entry);
        if (!current->hidden) j++;
        current = current->next;
    }
}

void filter_entries()
{
    node_t * current = entries;
    entries_count = 0;

    while (current != NULL)
    {
        int matches=focus_mode ? ui_entry_matches(current) : strlen(commandline)==0 || strcasestr(current->title,commandline)!=NULL || strcasestr(current->cmd,commandline)!=NULL;
        if (matches){
            current->hidden=0;
            entries_count ++;
        } else {
            current->hidden=1;
            current->clicked=0;
            set_hover(0, current, 1);
        }
        current=current->next;
    }
    set_scroll_level(0);
}

void reset_prompt()
{
    while (cmdline != NULL) pop_key();
    joincmdline();
    joincmdlinetext();
    filter_entries();
    arrange_positions();
    node_t * current = entries;
    while (current != NULL) {
        set_hover(0, current, 0);
        current = current->next;
    }
    updates = imlib_update_append_rect(updates, 0, 0, screen_width, screen_height);
}

int parse_entries(void)
{
    int changed = 0; // if the list of elements have changed or not
    int parsing = 0; // section currently being read
    int position = 0; // position in the current entry
    int leading_space = 0;
    int line = 1; // line count for error messages
    int readstatus=-1;

    struct pollfd fds;
    fds.fd = fileno(input_source);
    fds.events = POLLIN;
    node_t * current_entry = NULL;
    char internal_command[128];
    while(input_byte>=0 || poll(&fds, 1, 0)) {
        char b;
        if (input_byte>=0) { b=input_byte; input_byte=-1; readstatus=1; }
        else readstatus = read(fds.fd, &b, 1);
        if(readstatus <= 0){
            break;
        }
        if(parsing == 0 && position == leading_space){
            if(b != ':' && b != '#') {
                current_entry = calloc(1,sizeof(node_t));
            } else {
                current_entry = NULL;
                if(b == '#'){
                    parsing = -1;
                }
            }
        }
        if (current_entry == NULL) {
            if (b == '\n') b = '\0';
            if (position > leading_space && parsing != -1)
                internal_command[position-leading_space-1] = b;
        } else {
            if(b == '\0') {
                fprintf(stderr, "Null-byte encountered while reading entries at line %d. Aborting.\n", line);
            }
            // Check for semi-colons only for the first two elements to support bash commands with semi-colons in them
            if(b == ';' && parsing != 2) {
                b = '\0';
            } else if (b == '\n') {
                line++;
                b = '\0';
                if(parsing == 0){
                    free_entry(current_entry); current_entry=NULL;
                    clear_input_entries();
                    changed = 1;
                    position=leading_space=0;
                    continue;
                }
            }
            if(b == ' ' && position <= leading_space) leading_space++;
            if(b != ' ' && leading_space > 0) leading_space = -leading_space;
            switch(parsing){
                case 0:
                    if (leading_space <= 0)
                        current_entry->title[position+leading_space] = b;
                    break;
                case 1:
                    current_entry->icon[position] = b;
                    break;
                case 2:
                    current_entry->cmd[position] = b;
                    break;
            }
        }
        position++;
        if(b == '\0') {
            position = 0;
            leading_space = 0;
            if(parsing == 2 || current_entry == NULL) {
                if(current_entry != NULL) {
                    push_entry(current_entry);
                    changed = 1;
                } else {
                    if (parsing != -1)
                        run_internal_command(internal_command);
                }
                parsing = 0;
            } else {
                parsing++;
            }
        }
        if(current_entry != NULL) {
            int maxlen = (parsing == 2 ? 511 : 255);
            if(position == maxlen){
                fprintf(stderr, "Entry too long on line %d, maximum length is %d characters!\n", line, maxlen);
                break;
            }
        }
    }
    if(readstatus == 0){
        if(parsing == 2){
            current_entry->cmd[position]='\0';
            push_entry(current_entry);
            changed = 1;
        }
        close(fds.fd);
        if (input_source==stdin) eventfds[1].fd=-1;
        input_source = NULL;
    }
    if(changed) {
        filter_entries();
        if (focus_mode) focus_layout();
        arrange_positions();
    }
    return changed;
}
