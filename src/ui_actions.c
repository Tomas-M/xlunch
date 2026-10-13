#include "ui.h"

// Return a selection without invoking a backend, preserving the stdout protocol.
static void output_selection(const char *value)
{
    cache_cancel();
    printf("%s\n",value); fflush(stdout);
    if (!dont_quit) { cleanup(); exit(OKAY); }
    reset_prompt();
    if (focus_mode) focus_refresh();
}

// Execute generic actions or return their explicit caller-specific output value.
void ui_action(const char *command, const char *output)
{
    if (output_only) output_selection(*output ? output : command);
    else run_command((char *)command);
}

// Launch helpers with separate arguments, preserving quoting and the stdout contract.
static void launch(const char *launcher, const char *arguments, const char *value, const char *program)
{
    cache_cancel();
    int count; char **items=ui_list(arguments,&count);
    char **argv=calloc(count+4,sizeof(*argv)); argv[0]=strdup(launcher);
    for (int i=0;i<count;i++) argv[i+1]=items[i];
    free(items); argv[++count]=strdup(value);
    if (program) argv[++count]=strdup(program);
    count++;
    restack();
    if (dont_quit)
    {
        pid_t child=fork();
        if (child<0) { perror("fork"); ui_list_free(argv,count); return; }
        if (child) { ui_list_free(argv,count); reset_prompt(); if (focus_mode) focus_refresh(); return; }
        if (!multiple_instances) close(lock);
    }
    else cleanup();
    execvp(argv[0],argv);
    fprintf(stderr,"Could not run launcher %s: %s\n",argv[0],strerror(errno)); exit(EXTERNALERROR);
}

// Activate legacy/custom commands or invoke the configured desktop helper with argv.
void ui_activate(node_t *entry)
{
    if (!entry) return;
    if (!entry->id) { run_command(entry->cmd); return; }
    if (!entry->desktop_file) { ui_action(entry->cmd,ui_field("entry",entry->id+7,"output")); return; }
    if (output_only) { output_selection(entry->desktop_file); return; }
    launch(ui_value("desktop.launcher"),ui_value("desktop.launcher_args"),entry->desktop_file,NULL);
}

// Resolve a real executable, including empty PATH components and absolute paths.
char *ui_executable(const char *name)
{
    if (!name || !*name) return NULL;
    const char *environment=getenv("PATH");
    char *paths=strdup(environment ? environment : ""),*position=paths,*part,*result=NULL;
    while ((part=strsep(&position,":")))
    {
        char *file; struct stat info;
        if (strchr(name,'/')) file=strdup(name);
        else asprintf(&file,"%s/%s",*part ? part : ".",name);
        if (!stat(file,&info) && S_ISREG(info.st_mode) && !access(file,X_OK)) result=realpath(file,NULL);
        free(file); if (result || strchr(name,'/')) break;
    }
    free(paths); return result;
}

// Read the first quoted or escaped word without evaluating shell expansions.
static char *command_word(const char *command)
{
    if (!command) return NULL;
    while (isspace((unsigned char)*command)) command++;
    char *word=malloc(strlen(command)+1),quote=0; int length=0;
    for (const char *p=command;*p;p++)
    {
        if (!quote && (isspace((unsigned char)*p) || strchr(";&|<>()",*p))) break;
        if (*p==quote) { quote=0; continue; }
        if (!quote && (*p=='\'' || *p=='"')) { quote=*p; continue; }
        if (*p=='\\' && p[1] && quote!='\'') p++;
        word[length++]=*p;
    }
    word[length]=0;
    if (quote || !length) { free(word); return NULL; }
    return word;
}

// Prefer an application's exact executable; otherwise run a configured command helper.
int ui_command(const char *command)
{
    if (!ui_bool("search.allow_commands") || !*command) return 0;
    const char *prefix=ui_value("search.command_prefix");
    if (*prefix) { if (!starts_with(prefix,command)) return 0; command+=strlen(prefix); }
    char *word=command_word(command),*program=ui_executable(word); free(word);
    if (!*prefix && !program) return 0;
    if (!*prefix) for (node_t *entry=entries;entry;entry=entry->next)
    {
        word=command_word(entry->desktop_exec ? entry->desktop_exec : entry->cmd);
        char *candidate=ui_executable(word); free(word);
        int matches=candidate && !strcmp(candidate,program); free(candidate);
        if (matches) { free(program); ui_activate(entry); return 1; }
    }
    if (output_only) output_selection(command);
    else if (*ui_value("search.command_launcher"))
        launch(ui_value("search.command_launcher"),ui_value("search.command_launcher_args"),command,program ? program : "");
    else ui_action(command,"");
    free(program); return 1;
}
