#include "ui.h"

int starts_with(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? 0 : strncmp(pre, str, lenpre) == 0;
}

void run_internal_command(char * cmd_orig) {
    int will_quit = 0;
    char * cmd = malloc(strlen(cmd_orig)+1);
    memcpy(cmd, cmd_orig, strlen(cmd_orig)+1);
    char *p = strtok( cmd, " ");
    if (strcmp(p, "scroll") == 0) {
        p = strtok( NULL, " ");
        if (strcmp(p, "top") == 0){
            set_scroll_level(0);
        } else if (strcmp(p, "bottom") == 0){
            set_scroll_level(entries_count);
        } else {
            if (p[0] == '+' || p[0] == '-')
                set_scroll_level(scrolled_past + atoi(p));
            else
                set_scroll_level(atoi(p));
        }
    } else if (strcmp(p, "hover") == 0) {
        if (hoverset != KEYBOARD) hovered_entry = 0;
        p = strtok( NULL, " ");
        int new_hover;
        if (strcmp(p, "start") == 0){
            new_hover = 1;
        } else if (strcmp(p, "end") == 0){
            new_hover = entries_count;
        } else {
            if (p[0] == '+' || p[0] == '-')
                new_hover = hovered_entry + atoi(p);
            else
                new_hover = atoi(p);
        }
        hover_entry(new_hover);
    } else if (strcmp(p, "exec") == 0) {
        int old_oo = output_only;
        output_only = 0;
        run_command(strtok(NULL, "\""));
        output_only = old_oo;
    } else if (strcmp(p, "print") == 0){
        printf("%s\n", strtok(NULL, "\""));
    } else if (strcmp(p, "quit") == 0) {
        will_quit = 1;
    }
    if (p != NULL) {
        p = strtok(NULL, "");
        if (p != NULL) {
            if (p[0] == ':') {
                run_internal_command(&p[1]);
            } else {
                fprintf(stderr, "Internal command \"%s\" supplied with extraneous parameters \"%s\"\n", cmd_orig, p);
            }
        }
    }
    free(cmd);
    if (will_quit) {
        cleanup();
        exit(INTERNALCMD);
    }
}

void run_command(char * cmd_orig)
{
    char *cmd;
    char *array[100] = {0};

    cmd = strdup(cmd_orig);
    if (!cmd) {
        fprintf(stderr, "Out of memory!\n");
        exit(ALLOCERROR);
    }

    if (cmd_orig[0]!=':' || starts_with(":recur",cmd_orig)) cache_cancel();

    int isrecur = starts_with(":recur ", cmd_orig) || (strcmp(":recur", cmd_orig) == 0);
    if(isrecur) {
        // split arguments into pieces
        int i = 0;
        // If we recur (ie. start xlunch again) run the command that was used to run xlunch
        array[0] = program_name;

        // Blindly consume the first token which should be "recur"
        char *p = strtok (cmd, " ");
        p = strtok (NULL, " ");
        while (p != NULL)
        {
            array[++i]=p;
            p = strtok (NULL, " ");
            if (i>=99) break;
        }
    } else {
        if (cmd_orig[0] == ':') {
            run_internal_command((char *)(cmd_orig + sizeof(char)));
            free(cmd); return;
        }
        if(output_only){
            fprintf(stdout, "%s\n", cmd_orig); fflush(stdout);
            if(!dont_quit){
                cleanup();
                exit(OKAY);
            } else {
                reset_prompt();
                free(cmd); return;
            }
        }
        array[0] = "/bin/sh";
        array[1] = "-c";
        array[2] = cmd; // Keep the command alive after cleanup frees its entry.
        array[3] = NULL;
    }

    restack();

    if (dont_quit)
    {
        pid_t pid=fork();

        switch (pid) {
        case 0:   /* Child process */
            if (!multiple_instances) close(lock);
            fprintf(focus_mode ? stderr : stdout,"Forking command: %s\n",cmd);
            break;

        case -1:  /* Error */
            perror("fork");
            /*FALLTHROUGH*/

        default:  /* Parent */
            reset_prompt();
            if (cmd != cmd_orig)
                free(cmd);
            return;
        }
    }
    else
    {
        cleanup();
        fprintf(focus_mode ? stderr : stdout,"Running command: %s\n",cmd);
    }

    execvp(array[0],array);
    fprintf(stderr,"Error running '%s -c \"%s\"': %s\n", array[0], array[2],
	strerror(errno));
    exit(EXTERNALERROR);
}
