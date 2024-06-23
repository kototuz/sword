#include "sword.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

static int menu_command_help(ArgValueCopy *args);
static int menu_command_repo_list(ArgValueCopy *args);
static int menu_command_repo_new(ArgValueCopy *args);
static int menu_command_repo_del(ArgValueCopy *args);
static int menu_command_cards_list(ArgValueCopy *args);
static int menu_command_cards_add(ArgValueCopy *args);

static Errno repo_path_str(StrView repo_name, char **result);

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

static struct {
    RepoOpenMode open_mode;
    FILE *file;
} current_repo = {0};



Errno repo_new(StrView repo_name)
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

Errno repo_del(StrView repo_name)
{
    char *repo_path;
    Errno err = repo_path_str(repo_name, &repo_path);
    if (err != 0) return err;

    if (remove(repo_path) != 0) return errno;

    return 0;
}

static const char *mode_map[] = {[1] = "r", "a", "a+"};
Errno repo_open(StrView repo_name, RepoOpenMode mode)
{
    assert(!current_repo.file);

    union {
        RepoOpenMode mode;
        int idx;
    } value = {.mode = mode};

    assert(value.idx);

    char *repo_path;
    Errno err = repo_path_str(repo_name, &repo_path);
    if (err != 0) return err;

    FILE *repo_file = fopen(repo_path, mode_map[value.idx]);
    if (!repo_file) {
        free(repo_path);
        return errno;
    }

    current_repo.open_mode = mode;
    current_repo.file = repo_file;

    free(repo_path);
    return 0;
}

void repo_close()
{
    assert(current_repo.file);
    fclose(current_repo.file);
}

void repo_ser_fc(FlashCard fc)
{
    assert(current_repo.file);
    assert(current_repo.open_mode.append);

    fprintf(current_repo.file,
            "%s=%s\n",
            fc.text,
            fc.translation);
}

bool repo_deser_fc(FlashCard *result)
{
    assert(current_repo.file);
    assert(current_repo.open_mode.read);

    int symbol;
    size_t fc_text_len = 0;
    size_t fc_transcript_len = 0;

    symbol = fgetc(current_repo.file);
    if (symbol == EOF) return false;
    else if (symbol != '=') fc_text_len++;

    while (fgetc(current_repo.file) != '=') fc_text_len++;
    while (fgetc(current_repo.file) != '\n') fc_transcript_len++;

    fseek(current_repo.file, -(fc_text_len + fc_transcript_len)-2, SEEK_CUR);

    result->text = (char *) malloc(fc_text_len+1);
    if (!result->text) return false;
    result->translation = (char *) malloc(fc_transcript_len+1);
    if (!result->translation) {
        free(result->text);
        return false;
    }

    result->text[fc_text_len] = '\0';
    result->translation[fc_transcript_len] = '\0';

    fc_text_len = 0;
    fc_transcript_len = 0;

    while ((symbol = fgetc(current_repo.file)) != '=')
        result->text[fc_text_len++] = symbol;
    while ((symbol = fgetc(current_repo.file)) != '\n')
        result->translation[fc_transcript_len++] = symbol;

    return true;
}



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
    Errno err = repo_new(args[0].value.data.as_str);
    if (err != 0)
        fprintf(stderr, "ERROR: could not create a new repo: %s\n",
                strerror(err));

    return err;
}

static int menu_command_repo_del(ArgValueCopy *args)
{
    Errno err = repo_del(args[0].value.data.as_str);
    if (err != 0)
        fprintf(stderr, "ERROR: could not delete the repo: %s\n",
                strerror(err));

    return err;
}

static int menu_command_cards_list(ArgValueCopy *args)
{
    Errno err = repo_open(args[0].value.data.as_str, (RepoOpenMode){.read = 1});
        if (err != 0) {
            fprintf(stderr, "ERROR: could not list cards: %s\n",
                    strerror(err));
            return err;
        }

        FlashCard fc;
        while (repo_deser_fc(&fc))
            printf("%s=%s\n", fc.text, fc.translation);
    repo_close();

    return 0;
}

static int menu_command_cards_add(ArgValueCopy *args)
{
    Errno err = repo_open(args[0].value.data.as_str, (RepoOpenMode){.append = 1});
        if (err != 0) {
            fprintf(stderr, "ERROR: could not add a new card: %s\n",
                    strerror(err));
            return err;
        }

        char *text_strc;
        char *transcript_strc;
        err = strc_from_strv(args[1].value.data.as_str, &text_strc);
        if (err != 0) {
            fprintf(stderr, "ERROR: could not add a new card: %s\n",
                    strerror(err));
            return err;
        }
        err = strc_from_strv(args[2].value.data.as_str, &transcript_strc);
        if (err != 0) {
            fprintf(stderr, "ERROR: could not add a new card: %s\n",
                    strerror(err));
            return err;
        }

        repo_ser_fc((FlashCard){text_strc, transcript_strc});

        free(text_strc);
        free(transcript_strc);
    repo_close();

    return 0;
}



static Errno repo_path_str(StrView repo_name, char **result)
{
    char *repo_path = (char *) malloc(sizeof(REPOSITORIES_PATH) + repo_name.len);
    if (!repo_path) return errno;

    memcpy(repo_path, REPOSITORIES_PATH, sizeof(REPOSITORIES_PATH));
    repo_path[sizeof(REPOSITORIES_PATH)-1] = '/';
    strncat(repo_path, repo_name.items, repo_name.len);

    *result = repo_path;
    return 0;
}

