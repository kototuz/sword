#include "sword.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int menu_command_help(ArgValueCopy *args);
static int menu_command_repo_list(ArgValueCopy *args);
static int menu_command_repo_new(ArgValueCopy *args);
static int menu_command_repo_del(ArgValueCopy *args);

static int make_repo_path_str(const StrView name, char **result);

const CommandSet menu_command_set = {
    .len = 2,
    .items = (Command[]){
        {
            .name = STRV_LIT("help"),
            .desc = "Prints all commands",
            .fn = menu_command_help,
        },
        {
            .name = STRV_LIT("repo"),
            .desc = "Lists all repositories",
            .fn = menu_command_repo_list,
            .subcommands.len = 2,
            .subcommands.items = (Command[]){
                {
                    .name = STRV_LIT("new"),
                    .desc = "Creates new repository",
                    .fn = menu_command_repo_new,
                    .args_len = 1,
                    .args = (Arg[]){
                        {
                            .name = STRV_LIT("name"),
                            .usage = "Repo name",
                            .type_inst.type_tag = KSH_VALUE_TYPE_TAG_STR
                        }
                    }
                },
                {
                    .name = STRV_LIT("del"),
                    .desc = "Deletes specified repository",
                    .fn = menu_command_repo_del,
                    .args_len = 1,
                    .args = (Arg[]){
                        {
                            .name = STRV_LIT("name"),
                            .usage = "Repo name",
                            .type_inst.type_tag = KSH_VALUE_TYPE_TAG_STR
                        }
                    }
                }
            }
        }
    }
};



static int menu_command_help(ArgValueCopy *args)
{
    (void) args;

    puts("->SWORD<-");
    puts("Flashcard programm written in C");
    puts("[comands]:");
    
    for (size_t i = 0; i < menu_command_set.len; i++) {
        printf("\t%s\t%s\n",
               menu_command_set.items[i].name.items,
               menu_command_set.items[i].desc);
    }

    return 0;
}

static int menu_command_repo_list(ArgValueCopy *args)
{
    (void) args;

    struct dirent *de;

    DIR *dir = opendir(REPOSITORIES_PATH);
    if (!dir) {
        fputs("ERROR: could not open the directory\n", stderr);
        return 1;
    }
    
    while ((de = readdir(dir)) != NULL) {
        puts(de->d_name);
    }

    closedir(dir);
    return 0;
}

static int menu_command_repo_new(ArgValueCopy *args)
{
    char *new_repo_path;
    int err = make_repo_path_str(args[0].value.data.as_str, &new_repo_path);
    if (err != 0) return err;

    FILE *new_repo = fopen(new_repo_path, "w");
    if (!new_repo) {
        fputs("ERROR: could not create new repo\n", stderr);
        free(new_repo_path);
        return 1;
    }

    fclose(new_repo);
    return 0;
}

static int menu_command_repo_del(ArgValueCopy *args)
{
    char *repo_path;
    int err = make_repo_path_str(args[0].value.data.as_str, &repo_path);
    if (err != 0) return err;

    if (remove(repo_path) == 0) {
        fputs("ERROR: could not delete the repo\n", stderr);
        free(repo_path);
        return 1;
    }

    free(repo_path);
    return 0;
}

static int make_repo_path_str(const StrView name, char **result)
{
    char *new_repo_path = (char *) malloc(sizeof(REPOSITORIES_PATH) + name.len);
    if (!new_repo_path) return 1;

    memcpy(new_repo_path, REPOSITORIES_PATH, sizeof(REPOSITORIES_PATH));
    new_repo_path[sizeof(REPOSITORIES_PATH)-1] = '/';
    strncat(new_repo_path, name.items, name.len);

    *result = new_repo_path;
    return 0;
}
