#include "ui.h"

typedef struct desktop_id { char *id; struct desktop_id *next; } desktop_id_t;
static desktop_id_t *seen_ids;

// Decode desktop-entry string escapes while retaining UTF-8 bytes.
static char *desktop_string(const char *text)
{
    char *value=malloc(strlen(text)+1); int n=0;
    for (const char *p=text;*p && *p!='\n' && *p!='\r';p++)
    {
        if (*p=='\\' && p[1])
        {
            p++;
            value[n++]=*p=='s' ? ' ' : *p=='n' ? '\n' : *p=='t' ? '\t' : *p=='r' ? '\r' : *p;
        }
        else value[n++]=*p;
    }
    value[n]=0; return value;
}

// Rank localized keys using language, territory and modifier precedence.
static int localized_rank(const char *key, const char *name)
{
    size_t length=strlen(name);
    if (!strcmp(key,name)) return 1;
    if (strncmp(key,name,length) || key[length]!='[') return 0;
    char *locale=strdup(setlocale(LC_MESSAGES,NULL)),*encoding=strchr(locale,'.');
    if (encoding) { char *modifier=strchr(encoding,'@'); if (modifier) memmove(encoding,modifier,strlen(modifier)+1); else *encoding=0; }
    char *actual=strdup(key+length+1),*end=strchr(actual,']'); if (end) *end=0;
    int rank=!strcmp(locale,actual) ? 5 : 0;
    char *modifier=strchr(locale,'@'),*territory=strchr(locale,'_');
    if (modifier)
    {
        char *plain=strndup(locale,modifier-locale);
        if (!strcmp(plain,actual) && rank<4) rank=4;
        free(plain);
    }
    if (territory)
    {
        char *language=strndup(locale,territory-locale),*language_modifier=NULL;
        if (modifier) { asprintf(&language_modifier,"%s%s",language,modifier); if (!strcmp(language_modifier,actual) && rank<3) rank=3; free(language_modifier); }
        if (!strcmp(language,actual) && rank<2) rank=2;
        free(language);
    }
    else if (modifier)
    {
        char *language=strndup(locale,modifier-locale); if (!strcmp(language,actual) && rank<2) rank=2; free(language);
    }
    free(locale); free(actual); return rank;
}

// Match semicolon-delimited desktop environments against XDG_CURRENT_DESKTOP.
static int environment_matches(const char *list)
{
    const char *current=getenv("XDG_CURRENT_DESKTOP"); if (!current) return 0;
    char *environments=strdup(current),*save; int found=0;
    for (char *name=strtok_r(environments,":",&save);name;name=strtok_r(NULL,":",&save))
    {
        char *values=strdup(list),*next;
        for (char *value=strtok_r(values,";",&next);value;value=strtok_r(NULL,";",&next)) if (!strcmp(name,value)) found=1;
        free(values);
    }
    free(environments); return found;
}

// Derive availability from the wrapper's target executable, including cached entries.
int ui_entry_needs_install(node_t *entry)
{
    if (!entry->executable || !*entry->executable) return 0;
    char *path=ui_executable(entry->executable);
    int missing=!path; free(path); return missing;
}

// Parse only the main desktop-entry group, respecting visibility and localization.
static void desktop_entry(const char *file, const char *id)
{
    FILE *input=fopen(file,"r"); if (!input) return;
    node_t *entry=calloc(1,sizeof(*entry)); entry->id=strdup(id); entry->desktop_file=strdup(file);
    char *line=NULL,*icon=NULL,*type=NULL,*try_exec=NULL,*only=NULL,*not=NULL; size_t capacity=0;
    int main_group=0,hidden=0,no_display=0,dbus_activation=0,ranks[3]={0};
    while (getline(&line,&capacity,input)>=0)
    {
        if (*line=='[') { main_group=!strncmp(line,"[Desktop Entry]",15); continue; }
        if (!main_group || *line=='#') continue;
        char *equal=strchr(line,'='); if (!equal) continue; *equal=0;
        const char *key=line,*value=equal+1;
        const char *names[]={"Name","Comment","Keywords"};
        char **destinations[]={&entry->name,&entry->description,&entry->keywords};
        for (int i=0;i<3;i++)
        {
            int rank=localized_rank(key,names[i]);
            if (rank>ranks[i]) { free(*destinations[i]); *destinations[i]=desktop_string(value); ranks[i]=rank; }
        }
        char **destination=NULL;
        if (!strcmp(key,"Icon")) destination=&icon;
        if (!strcmp(key,"Type")) destination=&type;
        if (!strcmp(key,"TryExec")) destination=&try_exec;
        if (!strcmp(key,"OnlyShowIn")) destination=&only;
        if (!strcmp(key,"NotShowIn")) destination=&not;
        if (!strcmp(key,"Categories")) destination=&entry->categories;
        if (!strcmp(key,"Exec")) destination=&entry->desktop_exec;
        if (!strcmp(key,"X-Xlunch-Executable")) destination=&entry->executable;
        if (destination) { free(*destination); *destination=desktop_string(value); }
        if (!strcmp(key,"Hidden")) hidden=!strncmp(value,"true",4);
        if (!strcmp(key,"NoDisplay")) no_display=!strncmp(value,"true",4);
        if (!strcmp(key,"DBusActivatable")) dbus_activation=!strncmp(value,"true",4);
    }
    fclose(input); free(line);
    int application=type && !strcmp(type,"Application"),named=entry->name && *entry->name;
    int launchable=(entry->desktop_exec && *entry->desktop_exec) || dbus_activation;
    int eligible=application && named;
    if (!launchable || hidden || no_display) eligible=0;
    if (try_exec) { char *path=ui_executable(try_exec); if (!path) eligible=0; free(path); }
    if (only && !environment_matches(only)) eligible=0;
    if (not && environment_matches(not)) eligible=0;
    if (eligible)
    {
        strncpyutf8(entry->title,entry->name,sizeof(entry->title)-1);
        char *path=ui_icon_path(icon ? icon : "",icon_size);
        if (path) { snprintf(entry->icon,sizeof(entry->icon),"%s",path); free(path); }
        push_entry(entry);
    }
    else free_entry(entry);
    free(icon); free(type); free(try_exec); free(only); free(not);
}

// Discover desktop IDs recursively; earlier XDG roots override all later roots.
static void scan_desktops(const char *directory, const char *relative, int depth)
{
    if (depth>32) return;
    DIR *folder=opendir(directory); if (!folder) return; struct dirent *item;
    while ((item=readdir(folder)))
    {
        if (*item->d_name=='.') continue;
        char *path,*id; asprintf(&path,"%s/%s",directory,item->d_name); asprintf(&id,"%s%s",relative,item->d_name);
        struct stat info;
        if (!stat(path,&info) && S_ISDIR(info.st_mode))
        {
            char *child; asprintf(&child,"%s-",id); scan_desktops(path,child,depth+1); free(child);
        }
        else if (strlen(id)>8 && !strcmp(id+strlen(id)-8,".desktop"))
        {
            int exists=0; for (desktop_id_t *seen=seen_ids;seen;seen=seen->next) if (!strcmp(seen->id,id)) exists=1;
            if (!exists)
            {
                desktop_id_t *seen=calloc(1,sizeof(*seen)); seen->id=strdup(id); seen->next=seen_ids; seen_ids=seen;
                desktop_entry(path,id);
            }
        }
        free(path); free(id);
    }
    closedir(folder);
}

// Load the optional desktop catalog in XDG precedence and stable title order.
void ui_load_entries(void)
{
    const char *home=getenv("XDG_DATA_HOME"),*dirs=getenv("XDG_DATA_DIRS"); char *user=NULL;
    if (!home || !*home || *home!='/') { asprintf(&user,"%s/.local/share",getenv("HOME")); home=user; }
    char *path; asprintf(&path,"%s/applications",home); scan_desktops(path,"",0); free(path); free(user);
    char *roots=strdup(dirs && *dirs ? dirs : "/usr/local/share:/usr/share"),*save;
    for (char *root=strtok_r(roots,":",&save);root;root=strtok_r(NULL,":",&save))
    {
        if (*root!='/') continue;
        asprintf(&path,"%s/applications",root); scan_desktops(path,"",0); free(path);
    }
    free(roots);
    while (seen_ids) { desktop_id_t *next=seen_ids->next; free(seen_ids->id); free(seen_ids); seen_ids=next; }
    node_t *sorted=NULL;
    while (entries)
    {
        node_t *entry=entries; entries=entries->next; node_t **position=&sorted;
        while (*position && strcoll((*position)->title,entry->title)<0) position=&(*position)->next;
        entry->next=*position; *position=entry;
    }
    entries=sorted;
}

// Add explicit custom entries without extending the legacy DSV format.
void ui_custom_entries(void)
{
    int count; char **ids=ui_sections("entry",&count);
    for (int i=0;i<count;i++)
    {
        const char *name=ui_field("entry",ids[i],"label"),*command=ui_field("entry",ids[i],"command");
        if (!*name || !*command || strlen(command)>=sizeof(((node_t *)0)->cmd)) { fprintf(stderr,"Invalid custom entry: %s\n",ids[i]); exit(CONFIGERROR); }
        node_t *entry=calloc(1,sizeof(*entry)); asprintf(&entry->id,"custom:%s",ids[i]); entry->name=strdup(name);
        entry->categories=strdup(ui_field("entry",ids[i],"categories")); entry->description=strdup(ui_field("entry",ids[i],"description"));
        strncpyutf8(entry->title,name,sizeof(entry->title)-1); strcpy(entry->cmd,command);
        char *icon=ui_icon_path(ui_field("entry",ids[i],"icon"),icon_size);
        if (icon) { snprintf(entry->icon,sizeof(entry->icon),"%s",icon); free(icon); }
        push_entry(entry);
    }
    ui_list_free(ids,count);
}
