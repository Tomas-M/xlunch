#include "ui.h"
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>

int rebuild_cache;
static pid_t worker;
static char *cache_file,*temporary_file;

// Hash effective settings and environment without inspecting every source file at startup.
uint64_t cache_hash(uint64_t hash, const char *value)
{
    for (const unsigned char *p=(const unsigned char *)(value ? value : "");*p;p++) hash=(hash^*p)*1099511628211ULL;
    return hash*1099511628211ULL;
}

// Keep independent catalogs for different locales, themes and desktop environments.
static void cache_path(void)
{
    if (cache_file) return;
    const char *home=getenv("HOME");
    if (!home || !*home) return;
    uint64_t hash=ui_cache_config(14695981039346656037ULL);
    const char *variables[]={"HOME","XDG_DATA_HOME","XDG_DATA_DIRS","XDG_CURRENT_DESKTOP","PATH"};
    for (int i=0;i<5;i++) hash=cache_hash(hash,getenv(variables[i]));
    hash=cache_hash(hash,setlocale(LC_MESSAGES,NULL)); hash=cache_hash(hash,setlocale(LC_COLLATE,NULL));
    color_t tint=ui_color("icons.color",ui_color("colors.text",text_color));
    char dimensions[128]; uint32_t endian=1;
    snprintf(dimensions,sizeof(dimensions),"%d:%g:%d:%d:%d:%d:%d:%d:%d:%d",icon_size,ui_scale(),focus_mode,hide_missing,reverse,tint.r,tint.g,tint.b,tint.a,*(unsigned char *)&endian);
    hash=cache_hash(hash,dimensions);
    const char *base=getenv("XDG_CACHE_HOME"); char *default_base=NULL;
    if (!base || *base!='/') { asprintf(&default_base,"%s/.cache",home); base=default_base; }
    mkdir(base,0700);
    char *directory; asprintf(&directory,"%s/xlunch",base); mkdir(directory,0700); free(default_base);
    asprintf(&cache_file,"%s/%016llx",directory,(unsigned long long)hash); free(directory);
}

// Bound all reads to the validated file and checksum all bytes written.
void cache_bytes(cache_io_t *io, void *data, size_t size)
{
    if (!io->valid) return;
    if (!io->file)
    {
        if (size>(size_t)(io->end-io->position)) { io->valid=0; return; }
        memcpy(data,io->position,size); io->position+=size; return;
    }
    if (fwrite(data,1,size,io->file)!=size) { io->valid=0; return; }
    for (unsigned char *p=data;size;size--,p++) io->hash=(io->hash^*p)*16777619U;
}

// Encode record counts and dimensions in the cache's native-endian context.
uint32_t cache_number(cache_io_t *io, uint32_t value)
{
    cache_bytes(io,&value,sizeof(value)); return value;
}

// Preserve NULL metadata separately from empty strings and reject oversized records.
char *cache_text(cache_io_t *io, const char *value)
{
    uint32_t length=cache_number(io,value ? strlen(value) : UINT32_MAX);
    if (!io->valid || length==UINT32_MAX) return NULL;
    if (length>1048576) { io->valid=0; return NULL; }
    char *result=io->file ? (char *)value : calloc(length+1,1);
    cache_bytes(io,result,length); return result;
}

// Store only desktop metadata; interaction state and configured entries are recreated.
static node_t *cache_catalog(cache_io_t *io)
{
    uint32_t count=0;
    if (io->file) for (node_t *entry=entries;entry;entry=entry->next) count++;
    count=cache_number(io,count);
    if (count>100000) io->valid=0;
    node_t *catalog=NULL,**tail=&catalog,*source=entries;
    for (uint32_t i=0;i<count && io->valid;i++)
    {
        node_t *entry=io->file ? source : calloc(1,sizeof(*entry));
        char *fields[]={entry->title,entry->icon,entry->id,entry->name,entry->categories,entry->description,entry->keywords,entry->desktop_file,entry->desktop_exec,entry->executable};
        for (int k=0;k<10;k++) fields[k]=cache_text(io,io->file ? fields[k] : NULL);
        if (io->file) { source=source->next; continue; }
        if (!fields[0] || !fields[1] || !fields[2] || !fields[3] || !fields[7]) io->valid=0;
        snprintf(entry->title,sizeof(entry->title),"%s",fields[0] ? fields[0] : "");
        snprintf(entry->icon,sizeof(entry->icon),"%s",fields[1] ? fields[1] : "");
        free(fields[0]); free(fields[1]);
        entry->id=fields[2]; entry->name=fields[3]; entry->categories=fields[4]; entry->description=fields[5];
        entry->keywords=fields[6]; entry->desktop_file=fields[7]; entry->desktop_exec=fields[8]; entry->text_cell_width=-1;
        entry->executable=fields[9];
        *tail=entry; tail=&entry->next;
    }
    return catalog;
}

// Validate the whole snapshot before replacing any live catalog or icon pixels.
int cache_load(int refresh)
{
    cache_path(); if (!cache_file) return 0;
    FILE *file=fopen(cache_file,"rb"); if (!file) return 0;
    struct stat info;
    if (fstat(fileno(file),&info) || info.st_size<16 || info.st_size>256*1024*1024) { fclose(file); return 0; }
    size_t size=info.st_size; unsigned char *data=malloc(size);
    int valid=fread(data,1,size,file)==size; fclose(file);
    if (!valid) { free(data); return 0; }
    uint32_t hash=2166136261U,expected; memcpy(&expected,data+size-4,4);
    for (size_t i=0;i<size-4 && valid;i++) hash=(hash^data[i])*16777619U;
    if (!valid || hash!=expected || memcmp(data,"XLCACHE2",8)) { free(data); return 0; }
    cache_io_t io={.position=data+8,.end=data+size-4,.valid=1};
    node_t *catalog=cache_catalog(&io);
    if (io.valid) ui_icons_cache(&io);
    valid=io.valid && io.position==io.end;
    free(data);
    if (!valid)
    {
        while (catalog) { node_t *next=catalog->next; free_entry(catalog); catalog=next; }
        ui_icons_reset(); return 0;
    }
    if (refresh && focus_mode) focus_catalog_changed(catalog);
    else { clear_entries(); entries=catalog; }
    return 1;
}

// Publish complete snapshots atomically, retaining the last good cache on failure.
int cache_rebuild(void)
{
    cache_path(); if (!cache_file) return 0;
    clear_entries(); ui_icons_reset();
    // A forked refresh must not reuse decoded source images inherited from the old snapshot.
    int image_cache=imlib_get_cache_size(); imlib_set_cache_size(0); imlib_set_cache_size(image_cache);
    ui_load_entries(); ui_warm_icons();
    asprintf(&temporary_file,"%s.%ld.tmp",cache_file,(long)getpid());
    FILE *file=fopen(temporary_file,"wb"); if (!file) { perror(temporary_file); return 0; }
    cache_io_t io={.file=file,.valid=1,.hash=2166136261U};
    cache_bytes(&io,"XLCACHE2",8); cache_catalog(&io); ui_icons_cache(&io);
    if (fwrite(&io.hash,1,4,file)!=4) io.valid=0;
    if (fclose(file)) io.valid=0;
    if (io.valid && rename(temporary_file,cache_file)) io.valid=0;
    unlink(temporary_file); free(temporary_file); temporary_file=NULL;
    return io.valid;
}

// Remove incomplete output when the launcher cancels background work.
static void cache_interrupted(int signal_number)
{
    (void)signal_number;
    if (temporary_file) unlink(temporary_file);
    _exit(1);
}

// Rebuild in a low-priority process after the first frame, without sharing the X connection.
void cache_start(void)
{
    if (strcmp(ui_value("entries.source"),"desktop")) return;
    cache_path(); if (!cache_file) return;
    int channel[2]; if (pipe(channel)) return;
    worker=fork();
    if (worker<0) { close(channel[0]); close(channel[1]); worker=0; return; }
    if (!worker)
    {
        close(channel[0]); close(ConnectionNumber(disp));
        if (!multiple_instances) close(lock);
        signal(SIGTERM,cache_interrupted); signal(SIGPIPE,cache_interrupted);
        setpriority(PRIO_PROCESS,0,19);
        unsigned char result=cache_rebuild();
        write(channel[1],&result,1); close(channel[1]); _exit(0);
    }
    close(channel[1]); eventfds[2]=(struct pollfd){.fd=channel[0],.events=POLLIN};
    fcntl(channel[0],F_SETFD,FD_CLOEXEC);
}

// Consume a completed refresh only between input events and drawing frames.
void cache_update(void)
{
    if (!worker || !eventfds[2].revents) return;
    unsigned char result=0; read(eventfds[2].fd,&result,1);
    close(eventfds[2].fd); eventfds[2].fd=-1;
    waitpid(worker,NULL,0); worker=0;
    double position=scrolled_past;
    if (!result || !cache_load(1)) return;
    if (!focus_mode) { ui_custom_entries(); filter_entries(); }
    arrange_positions(); set_scroll_level(position); arrange_positions();
    updates=imlib_update_append_rect(updates,0,0,screen_width,screen_height);
}

// Stop refresh before launching a selection or closing the launcher.
void cache_cancel(void)
{
    if (!worker) return;
    kill(worker,SIGTERM); waitpid(worker,NULL,0); worker=0;
    close(eventfds[2].fd); eventfds[2].fd=-1;
}
