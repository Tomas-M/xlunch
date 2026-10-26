#include "ui.h"

enum ui_type { UI_TEXT,UI_BOOL,UI_NUMBER,UI_SCALE,UI_COLOR,UI_LIST,UI_ENUM,UI_ICON,UI_IMAGE };
typedef struct ui_definition { const char *key; enum ui_type type; const char *value,*allowed; } ui_definition_t;
typedef struct ui_setting { char *key,*value,*file; int line; struct ui_setting *next; } ui_setting_t;
static const ui_definition_t definitions[]={
#include "ui_properties.h"
};
static ui_setting_t *settings;
static const ui_definition_t *definition(const char *key);
int focus_mode;
const char *ui_check_file;

// Fingerprint effective profile values independently of the INI's ordering and filename.
uint64_t ui_cache_config(uint64_t hash)
{
    for (size_t i=0;i<sizeof(definitions)/sizeof(*definitions);i++)
        if (!strchr(definitions[i].key,'*')) hash=cache_hash(cache_hash(hash,definitions[i].key),ui_value(definitions[i].key));
    uint64_t named=0;
    for (ui_setting_t *item=settings;item;item=item->next)
        if (strchr(definition(item->key)->key,'*')) named+=cache_hash(cache_hash(14695981039346656037ULL,item->key),item->value);
    char value[24]; snprintf(value,sizeof(value),"%016llx",(unsigned long long)named);
    return cache_hash(hash,value);
}

// Prepare application and component icon sizes without requiring an X11 connection.
void ui_warm_icons(void)
{
    int sizes[]={lround(icon_size*ui_scale()),lround(ui_number("sidebar.icon_size")*ui_scale()),
        lround(ui_number("header.icon_size")*ui_scale()),lround(ui_number("session.icon_size")*ui_scale())};
    for (node_t *entry=entries;entry;entry=entry->next) ui_icon_image(entry->icon,sizes[0],sizes[0]);
    int install_size=lround(fmin(ui_number("grid.install_icon_size"),icon_size)*ui_scale());
    if (install_size>0) ui_icon_image(ui_value("grid.install_icon"),install_size,install_size);
    for (size_t i=0;i<sizeof(definitions)/sizeof(*definitions);i++)
        if (definitions[i].type==UI_ICON && !strchr(definitions[i].key,'*'))
            for (int k=0;k<4;k++) if (sizes[k]>0) ui_icon_image(ui_value(definitions[i].key),sizes[k],sizes[k]);
    for (ui_setting_t *item=settings;item;item=item->next)
        if (definition(item->key)->type==UI_ICON && strchr(definition(item->key)->key,'*'))
            for (int k=0;k<4;k++) if (sizes[k]>0) ui_icon_image(item->value,sizes[k],sizes[k]);
}

// Report configuration failures without opening X11 or writing protocol output.
static void config_error(const char *file, int line, const char *message, const char *key)
{
    fprintf(stderr,"%s:%d: %s: %s\n",file,line,message,key);
    exit(CONFIGERROR);
}

// Find scalar properties and the schema for named groups, actions and entries.
static const ui_definition_t *definition(const char *key)
{
    for (size_t i=0;i<sizeof(definitions)/sizeof(*definitions);i++)
    {
        const char *wild=strchr(definitions[i].key,'*');
        if (!wild && !strcmp(key,definitions[i].key)) return &definitions[i];
        if (wild && !strncmp(key,definitions[i].key,wild-definitions[i].key))
        {
            const char *field=strrchr(key,'.');
            if (field && field>key+(wild-definitions[i].key) && !strcmp(field,wild+1)) return &definitions[i];
        }
    }
    return NULL;
}

// Read one configured value, retaining the existing font and image aliases.
const char *ui_value(const char *key)
{
    if (!strcmp(key,"fonts.entries")) return *font_name ? font_name : "DejaVuSans/10";
    if (!strcmp(key,"fonts.search")) return *prompt_font_name ? prompt_font_name : ui_value("fonts.entries");
    if (!strcmp(key,"background.image")) return background_file;
    for (ui_setting_t *item=settings;item;item=item->next)
        if (!strcmp(item->key,key)) return item->value;
    const ui_definition_t *property=definition(key);
    return property ? property->value : "";
}

// Keep CLI aliases and INI properties on the same canonical numeric state.
int ui_number(const char *key)
{
    if (!strcmp(key,"grid.icon_size")) return icon_size;
    if (!strcmp(key,"grid.columns")) return ucolumns;
    if (!strcmp(key,"grid.label_lines") && no_title) return 0;
    return atoi(ui_value(key));
}

// Visibility and scrolling retain their existing command-line aliases.
int ui_bool(const char *key)
{
    if (!strcmp(key,"search.visible")) return !no_prompt && ui_bool("search.enabled");
    if (!strcmp(key,"scroll.enabled")) return !noscroll;
    if (!strcmp(key,"background.use_root")) return use_root_img;
    return !strcmp(ui_value(key),"true");
}

// Scale logical layout dimensions without changing the legacy renderer.
double ui_scale(void) { return atof(ui_value("ui.scale")); }

// Resolve optional state colors against their shared defaults.
color_t ui_color(const char *key, color_t fallback)
{
    if (!strcmp(key,"colors.text")) return text_color;
    if (!strcmp(key,"search.text")) return prompt_color;
    if (!strcmp(key,"background.color")) return background_color;
    if (!strcmp(key,"scrollbar.track")) return scrollbar_color;
    if (!strcmp(key,"scrollbar.thumb")) return scrollindicator_color;
    if (!strcmp(key,"colors.hover")) return highlight_color;
    const char *value=ui_value(key);
    if (!*value) return fallback;
    if (*value=='#') value++;
    color_t color={0,0,0,255};
    sscanf(value,"%02x%02x%02x%02x",&color.r,&color.g,&color.b,&color.a);
    return color;
}

// Parse quoted list items without treating desktop IDs or arguments as shell code.
char **ui_list(const char *value, int *count)
{
    char **items=NULL;
    *count=0;
    while (*value)
    {
        while (isspace((unsigned char)*value) || *value==',') value++;
        if (!*value) break;
        char *item=malloc(strlen(value)+1); int length=0,quoted=0,content=0;
        while (*value && (quoted || *value!=','))
        {
            if (*value=='"') quoted=!quoted;
            else if (*value=='\\' && value[1] && strchr("\\\",",value[1])) { item[length++]=*++value; content=length; }
            else { item[length++]=*value; if (quoted || !isspace((unsigned char)*value)) content=length; }
            value++;
        }
        item[length]=0;
        if (quoted) config_error("INI",0,"Unclosed list quote",item);
        item[content]=0;
        items=realloc(items,(*count+1)*sizeof(*items)); items[(*count)++]=item;
    }
    return items;
}

// Release the temporary argument or identifier list.
void ui_list_free(char **items, int count)
{
    for (int i=0;i<count;i++) free(items[i]);
    free(items);
}

// Apply typed values and shared legacy aliases at their position in argv.
void ui_set(const char *key, const char *value, const char *file, int line)
{
    const ui_definition_t *property=definition(key);
    if (!property) config_error(file,line,"Unknown property",key);
    if (property->type==UI_BOOL && strcmp(value,"true") && strcmp(value,"false")) config_error(file,line,"Expected true or false",key);
    if (property->type==UI_NUMBER || property->type==UI_SCALE)
    {
        char *end; double number=strtod(value,&end);
        if (!*value || *end || !isfinite(number) || number<0 || number>10000) config_error(file,line,"Invalid number",key);
        if (property->type==UI_NUMBER && number!=(int)number) config_error(file,line,"Expected integer",key);
        if (property->type==UI_SCALE && (number<.25 || number>4)) config_error(file,line,"Scale must be between 0.25 and 4",key);
        if (!strcmp(key,"grid.uninstalled_icon_transparency") && number>100) config_error(file,line,"Transparency must be between 0 and 100",key);
    }
    if (property->type==UI_COLOR && *value)
    {
        const char *color=*value=='#' ? value+1 : value;
        if (strlen(color)!=6 && strlen(color)!=8) config_error(file,line,"Expected RRGGBB or RRGGBBAA",key);
        for (const char *p=color;*p;p++) if (!isxdigit((unsigned char)*p)) config_error(file,line,"Invalid color",key);
    }
    if (property->type==UI_ENUM)
    {
        char *allowed=strdup(property->allowed),*save; int found=0;
        for (char *word=strtok_r(allowed,"|",&save);word;word=strtok_r(NULL,"|",&save)) if (!strcmp(word,value)) found=1;
        free(allowed);
        if (!found) config_error(file,line,"Invalid choice",key);
    }
    if (property->type==UI_LIST)
    {
        int quoted=0;
        for (const char *p=value;*p;p++) { if (*p=='\\' && p[1] && strchr("\\\",",p[1])) p++; else if (*p=='"') quoted=!quoted; }
        if (quoted) config_error(file,line,"Unclosed list quote",key);
        int count; char **list=ui_list(value,&count); ui_list_free(list,count);
    }
    ui_setting_t *item=settings;
    while (item && strcmp(item->key,key)) item=item->next;
    if (!item) { item=calloc(1,sizeof(*item)); item->key=strdup(key); item->next=settings; settings=item; }
    free(item->value); item->value=strdup(value);
    free(item->file); item->file=strdup(file); item->line=line;
    if (!strcmp(key,"ui.mode")) focus_mode=!strcmp(value,"focus");
    if (!strcmp(key,"fonts.entries")) font_name=item->value;
    if (!strcmp(key,"fonts.search")) prompt_font_name=item->value;
    if (!strcmp(key,"background.image")) background_file=item->value;
    if (!strcmp(key,"grid.icon_size")) icon_size=atoi(value);
    if (!strcmp(key,"grid.columns")) ucolumns=atoi(value);
    if (!strcmp(key,"search.visible")) no_prompt=!strcmp(value,"false");
    if (!strcmp(key,"scroll.enabled")) noscroll=!strcmp(value,"false");
    if (!strcmp(key,"background.use_root")) use_root_img=!strcmp(value,"true");
    const char *aliases[]={"colors.text","search.text","background.color","scrollbar.track","scrollbar.thumb","colors.hover"};
    color_t *destinations[]={&text_color,&prompt_color,&background_color,&scrollbar_color,&scrollindicator_color,&highlight_color};
    for (int i=0;i<6;i++) if (!strcmp(key,aliases[i]) && *value)
    {
        const char *color=*value=='#' ? value+1 : value;
        *destinations[i]=(color_t){0,0,0,255};
        sscanf(color,"%02x%02x%02x%02x",&destinations[i]->r,&destinations[i]->g,&destinations[i]->b,&destinations[i]->a);
        if (i==2) background_color_set=1;
    }
}

// Trim INI syntax whitespace while preserving the value's internal text.
static char *trim(char *text)
{
    while (isspace((unsigned char)*text)) text++;
    char *end=text+strlen(text);
    while (end>text && isspace((unsigned char)end[-1])) *--end=0;
    return text;
}

// Read UTF-8 INI with full-line comments, duplicate checks and relative assets.
void ui_read(const char *file)
{
    FILE *input=fopen(file,"r");
    if (!input) config_error(file,0,strerror(errno),"open");
    char *line=NULL,*section=strdup(""),**seen=NULL; size_t capacity=0; int number=0,count=0;
    while (getline(&line,&capacity,input)>=0)
    {
        number++; char *text=trim(line);
        if (number==1 && !strncmp(text,"\xef\xbb\xbf",3)) text+=3;
        if (!*text || *text=='#' || *text==';') continue;
        if (*text=='[')
        {
            char *end=strrchr(text,']');
            if (!end || *trim(end+1)) config_error(file,number,"Invalid section",text);
            *end=0; free(section); section=strdup(trim(text+1));
            int known=0;
            for (size_t i=0;i<sizeof(definitions)/sizeof(*definitions);i++)
            {
                char *candidate=strdup(definitions[i].key),*dot=strrchr(candidate,'.'); *dot=0;
                char *wild=strchr(candidate,'*');
                if (wild) { *wild=0; if (!strncmp(section,candidate,strlen(candidate)) && section[strlen(candidate)]) known=1; }
                else if (!strcmp(section,candidate)) known=1;
                free(candidate);
            }
            if (!known) config_error(file,number,"Unknown section",section);
            continue;
        }
        char *equal=strchr(text,'=');
        if (!equal || !*section) config_error(file,number,"Expected section and key=value",text);
        *equal=0; char *key; asprintf(&key,"%s.%s",section,trim(text));
        for (int i=0;i<count;i++) if (!strcmp(seen[i],key)) config_error(file,number,"Duplicate property",key);
        seen=realloc(seen,(count+1)*sizeof(*seen)); seen[count++]=strdup(key);
        char *value=trim(equal+1),*decoded=NULL;
        const ui_definition_t *property=definition(key);
        if (*value=='"' && (!property || property->type!=UI_LIST))
        {
            size_t length=strlen(value); decoded=calloc(length,1); int n=0,closed=0;
            for (size_t i=1;i<length;i++)
            {
                if (value[i]=='"') { if (i!=length-1) config_error(file,number,"Unexpected text after quote",key); closed=1; break; }
                if (value[i]=='\\' && i+1<length && (value[i+1]=='\\' || value[i+1]=='"')) i++;
                decoded[n++]=value[i];
            }
            if (!closed) config_error(file,number,"Unclosed quote",key);
            value=decoded;
        }
        char *path=NULL;
        if (property && *value && (property->type==UI_IMAGE || (property->type==UI_ICON && strchr(value,'/'))) && *value!='/')
        {
            char *parent=strdup(file); asprintf(&path,"%s/%s",dirname(parent),value); free(parent); value=path;
        }
        ui_set(key,value,file,number); free(path); free(decoded); free(key);
    }
    free(line); free(section); ui_list_free(seen,count); fclose(input);
}

// List named sections in deterministic order independent of INI section order.
char **ui_sections(const char *prefix, int *count)
{
    char **result=NULL; *count=0; size_t length=strlen(prefix);
    for (ui_setting_t *item=settings;item;item=item->next)
    {
        if (strncmp(item->key,prefix,length) || item->key[length]!='.') continue;
        char *id=strdup(item->key+length+1),*dot=strrchr(id,'.'); if (!dot) { free(id); continue; } *dot=0;
        int exists=0;
        for (int i=0;i<*count;i++) if (!strcmp(result[i],id)) exists=1;
        if (exists) { free(id); continue; }
        result=realloc(result,(*count+1)*sizeof(*result)); result[(*count)++]=id;
    }
    for (int i=0;i<*count;i++) for (int j=i+1;j<*count;j++) if (strcmp(result[i],result[j])>0) { char *swap=result[i]; result[i]=result[j]; result[j]=swap; }
    return result;
}

// Resolve a property of one named group, action or custom entry.
const char *ui_field(const char *kind, const char *id, const char *field)
{
    char *key; asprintf(&key,"%s.%s.%s",kind,id,field);
    const char *value=ui_value(key); free(key); return value;
}

// Retain the originating filename and line for cross-property validation errors.
static void property_error(const char *key, const char *message)
{
    for (ui_setting_t *item=settings;item;item=item->next)
        if (!strcmp(item->key,key)) config_error(item->file,item->line,message,key);
    const char *field=strrchr(key,'.');
    if (field) for (ui_setting_t *item=settings;item;item=item->next)
        if (!strncmp(item->key,key,field-key) && item->key[field-key]=='.') config_error(item->file,item->line,message,key);
    config_error("INI",0,message,key);
}

// Validate references and dimensions after all command-line overrides are applied.
void ui_validate(void)
{
    const char *orders[]={"groups.order","session.items"},*kinds[]={"group","action"};
    for (int k=0;k<2;k++)
    {
        int count,total; char **ids=ui_list(ui_value(orders[k]),&count),**defined=ui_sections(kinds[k],&total);
        for (int i=0;i<count;i++)
        {
            int found=0; for (int j=0;j<total;j++) if (!strcmp(ids[i],defined[j])) found=1;
            if (!found) property_error(orders[k],"Reference to missing section");
            if (k==1 && !*ui_field("action",ids[i],"command")) property_error(orders[k],"Missing action command");
        }
        ui_list_free(ids,count); ui_list_free(defined,total);
    }
    if (ui_number("scrollbar.hit_width")<ui_number("scrollbar.width")) property_error("scrollbar.hit_width","Hit area must cover visible width");
    const char *positive[]={"grid.tile_min_width","grid.tile_min_height","sidebar.item_height","search.height"};
    for (int i=0;i<4;i++) if (!ui_number(positive[i])) property_error(positive[i],"Dimension must be positive");
    int count; char **ids=ui_sections("entry",&count);
    for (int i=0;i<count;i++)
    {
        char *key; asprintf(&key,"entry.%s.command",ids[i]);
        if (!*ui_field("entry",ids[i],"label") || !*ui_field("entry",ids[i],"command")) property_error(key,"Custom entries require label and command");
        if (strlen(ui_field("entry",ids[i],"command"))>=sizeof(((node_t *)0)->cmd)) property_error(key,"Custom entry command exceeds 511 bytes");
        free(key);
    }
    ui_list_free(ids,count);
    ids=ui_list(ui_value("sidebar.order"),&count);
    for (int i=0;i<count;i++)
        if (strcmp(ids[i],"all") && strcmp(ids[i],"favorites") && strcmp(ids[i],"groups") && strcmp(ids[i],"other")) property_error("sidebar.order","Unknown navigation item");
    ui_list_free(ids,count);
}
