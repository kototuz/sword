#include "sword.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int menu_command_help(ArgValueCopy *args);
static int menu_command_repo_list(ArgValueCopy *args);
static int menu_command_repo_new(ArgValueCopy *args);
static int menu_command_repo_del(ArgValueCopy *args);
static int menu_command_cards_list(ArgValueCopy *args);
static int menu_command_cards_add(ArgValueCopy *args);

static Errno repo_path_str(StrView repo_name, char **result);
static Errno create_repo(StrView repo_name);
static Errno delete_repo(StrView repo_name);
static Errno open_repo(StrView repo_name, const char *mode, FILE **result);

const CommandSet menu_command_set = {
    .len = 3,
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
        },
        {
            .name = STRV_LIT("card"),
            .desc = "List cards from specific repo",
            .fn = menu_command_cards_list,
            .args_len = 1,
            .args = (Arg[]){
                {
                    .name = STRV_LIT("repo"),
                    .usage = "Repository",
                    .type_inst.type_tag = KSH_VALUE_TYPE_TAG_STR,
                }
            },
            .subcommands.len = 1,
            .subcommands.items = (Command[]){
                {
                    .name = STRV_LIT("add"),
                    .desc = "Add a new card to a specific repo",
                    .fn = menu_command_cards_add,
                    .args_len = 3,
                    .args = (Arg[]){
                        {
                            .name = STRV_LIT("repo"),
                            .type_inst.type_tag = KSH_VALUE_TYPE_TAG_STR
                        },
                        {
                            .name = STRV_LIT("text"),
                            .type_inst.type_tag = KSH_VALUE_TYPE_TAG_STR
                        },
                        {
                            .name = STRV_LIT("transcript"),
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

// TODO: the command shows also ".", ".."
static int menu_command_repo_list(ArgValueCopy *args)
{
    (void) args;

    struct dirent *de;

    DIR *dir = opendir(REPOSITORIES_PATH);
    if (!dir) {
        perror("ERROR: could not list repositories");
        return errno;
    }
    
    while ((de = readdir(dir)) != NULL) {
        puts(de->d_name);
    }

    closedir(dir);
    return 0;
}

// TODO: create the `resources/repositories` folder if it doesn't exist
static int menu_command_repo_new(ArgValueCopy *args)
{
    Errno err = create_repo(args[0].value.data.as_str);
    if (err != 0)
        fprintf(stderr, "ERROR: could not create a new repo: %s\n",
                strerror(err));

    return err;
}

static int menu_command_repo_del(ArgValueCopy *args)
{
    Errno err = delete_repo(args[0].value.data.as_str);
    if (err != 0)
        fprintf(stderr, "ERROR: could not delete the repo: %s\n",
                strerror(err));

    return err;
}

static int menu_command_cards_list(ArgValueCopy *args)
{
    FILE *repo;
    Errno err = open_repo(args[0].value.data.as_str, "r", &repo);
    if (err != 0) {
        fprintf(stderr, "ERROR: could not list cards: %s\n",
                strerror(err));
        return err;
    }

    char card_buf[100];
    while (fgets(card_buf, 100, repo) != NULL)
        puts(card_buf);

    fclose(repo);
    return 0;
}

static int menu_command_cards_add(ArgValueCopy *args)
{
    FILE *repo;
    Errno err = open_repo(args[0].value.data.as_str, "w", &repo);
    if (err != 0) {
        fprintf(stderr, "ERROR: could not add a new card: %s\n",
                strerror(err));
        return err;
    }

    fprintf(repo, STRV_FMT"="STRV_FMT"\n",
            STRV_ARG(args[1].value.data.as_str),
            STRV_ARG(args[2].value.data.as_str));

    fclose(repo);
    return 0;
}



static Errno repo_path_str(StrView repo_name, char **result)
{
    char *repo_path = (char *) malloc(sizeof(REPOSITORIES_PATH) + repo_name.len);
    if (!repo_path) return errno;

    memcpy(repo_path, REPOSITORIES_PATH, sizeof(REPOSITORIES_PATH));
    repo_path[sizeof(REPOSITORIES_PATH)-1] = '/';
    strncat(repo_path, repo_name.items, repo_name.len);

    printf("INFO: %s\n", repo_path);
    
    *result = repo_path;
    return 0;
}

static Errno create_repo(StrView repo_name)
{
    char *new_repo_path;
    Errno err = repo_path_str(repo_name, &new_repo_path);
    if (err != 0) return err;

    FILE *new_repo = fopen(new_repo_path, "w");
    if (!new_repo) {
        free(new_repo_path);
        return errno;
    }

    free(new_repo_path);
    fclose(new_repo);
    return 0;
}

static Errno delete_repo(StrView repo_name)
{
    char *repo_path;
    Errno err = repo_path_str(repo_name, &repo_path);
    if (err != 0) return err;

    if (remove(repo_path) != 0) return errno;

    return 0;
}

static Errno open_repo(StrView repo_name, const char *mode, FILE **result)
{
    char *repo_path;
    Errno err = repo_path_str(repo_name, &repo_path);
    if (err != 0) return err;

    FILE *repo = fopen(repo_path, mode);
    if (!repo) {
        free(repo_path);
        return errno;
    }

    *result = repo;

    free(repo_path);
    return 0;
}
