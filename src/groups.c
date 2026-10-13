#include "ui.h"

char focus_group[256]="all";

// Match whole categories from desktop semicolon lists or custom INI lists.
static int in_group(node_t *entry, const char *id)
{
    if (!entry->categories) return 0;
    int count; char **categories=ui_list(ui_field("group",id,"categories"),&count); int found=0;
    if (entry->desktop_file)
    {
        char *values=strdup(entry->categories),*save;
        for (char *value=strtok_r(values,";",&save);value;value=strtok_r(NULL,";",&save))
            for (int i=0;i<count;i++) if (!strcmp(value,categories[i])) found=1;
        free(values);
    }
    else
    {
        int total; char **values=ui_list(entry->categories,&total);
        for (int j=0;j<total;j++) for (int i=0;i<count;i++) if (!strcmp(values[j],categories[i])) found=1;
        ui_list_free(values,total);
    }
    ui_list_free(categories,count); return found;
}

// Search UTF-8 text using locale-aware case folding instead of byte-wise casing.
static int contains(const char *text, const char *query)
{
    if (!text) return 0;
    size_t a=mbstowcs(NULL,text,0),b=mbstowcs(NULL,query,0);
    if (a==(size_t)-1 || b==(size_t)-1) return strcasestr(text,query)!=NULL;
    wchar_t *haystack=calloc(a+1,sizeof(*haystack)),*needle=calloc(b+1,sizeof(*needle));
    mbstowcs(haystack,text,a+1); mbstowcs(needle,query,b+1);
    for (size_t i=0;i<a;i++) haystack[i]=towlower(haystack[i]);
    for (size_t i=0;i<b;i++) needle[i]=towlower(needle[i]);
    int found=wcsstr(haystack,needle)!=NULL; free(haystack); free(needle); return found;
}

// Resolve membership independently of the current query, so menu rows stay stable.
int ui_group_contains(node_t *entry, const char *id)
{
    int matches=1;
    if (strcmp(id,"all"))
    {
        matches=0;
        if (!strcmp(id,"favorites"))
        {
            int count; char **ids=ui_list(ui_value("favorites.ids"),&count);
            for (int i=0;i<count;i++) if (entry->id && !strcmp(entry->id,ids[i])) matches=1;
            ui_list_free(ids,count);
        }
        else if (!strcmp(id,"other"))
        {
            int count; char **ids=ui_sections("group",&count); matches=1;
            for (int i=0;i<count;i++) if (in_group(entry,ids[i])) matches=0;
            ui_list_free(ids,count);
        }
        else matches=in_group(entry,id);
    }
    return matches;
}

// Match searchable metadata independently of an application's category.
int ui_entry_search(node_t *entry)
{
    if (!ui_bool("search.enabled") || !*commandline) return 1;
    if (ui_bool("search.allow_commands") && *ui_value("search.command_prefix") && starts_with(ui_value("search.command_prefix"),commandline)) return 0;
    return contains(entry->name ? entry->name : entry->title,commandline) || contains(entry->description,commandline)
        || contains(entry->keywords,commandline) || contains(entry->desktop_exec,commandline) || contains(entry->cmd,commandline);
}

// Derive group and search visibility without changing the catalog's ordering.
int ui_entry_matches(node_t *entry)
{
    int search_all=*commandline && !strcmp(ui_value("search.scope"),"all");
    return (search_all || ui_group_contains(entry,focus_group)) && ui_entry_search(entry);
}

// Build a derived view, preserving catalog order except for configured favorites.
node_t **ui_visible_entries(node_t *catalog, int *count)
{
    node_t **view=NULL; *count=0;
    for (node_t *entry=catalog;entry;entry=entry->next)
        if (!entry->hidden) { view=realloc(view,(*count+1)*sizeof(*view)); view[(*count)++]=entry; }
    if (catalog==entries && !strcmp(focus_group,"favorites") && !(*commandline && !strcmp(ui_value("search.scope"),"all")))
    {
        int total; char **ids=ui_list(ui_value("favorites.ids"),&total);
        for (int i=0;i<*count;i++) for (int j=i+1;j<*count;j++)
        {
            int first=total,second=total;
            for (int k=0;k<total;k++)
            {
                if (view[i]->id && !strcmp(view[i]->id,ids[k])) first=k;
                if (view[j]->id && !strcmp(view[j]->id,ids[k])) second=k;
            }
            if (first>second) { node_t *swap=view[i]; view[i]=view[j]; view[j]=swap; }
        }
        ui_list_free(ids,total);
    }
    return view;
}
