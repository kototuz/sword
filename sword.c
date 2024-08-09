#include "kovsh.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define REPOS_DIR "./repos/"



static char *get_repo_path(StrView name)
{
    char *result = (char *) malloc(name.len + sizeof(REPOS_DIR));
    if (!result) return NULL;
    strcpy(result, REPOS_DIR);
    memcpy(&result[sizeof(REPOS_DIR)-1], name.items, name.len);
    return result;
}


// TODO: check if the card already exists
int new_card(KshParser *parser)
{
    StrView l; // label 
    StrView t; // transcript
    StrView r; // target repo name

    ksh_parse_args(parser, &(KshArgs){
        .params = KSH_PARAMS(
            KSH_PARAM(l, "flashcard label"),
            KSH_PARAM(t, "flaschard transcript"),
            KSH_PARAM(r, "target repo")
        )
    }); 

    char *repo_path = get_repo_path(r);
    if (!repo_path) {
        fputs("ERROR: could not allocate memory\n", stderr);
        exit(1);
    }

    FILE *repo_file = fopen(repo_path, "r");
    if (!repo_file) {
        fprintf(stderr, "ERROR: could not open repo %s\n", repo_path);
        free(repo_path);
        exit(1);
    }
    fclose(repo_file);

    repo_file = fopen(repo_path, "a");
    fprintf(repo_file, STRV_FMT"="STRV_FMT"\n", STRV_ARG(l), STRV_ARG(t));

    free(repo_path);
    fclose(repo_file);
    return 0;
}

// TODO: check if the repo already exists
int new_repo(KshParser *parser)
{
    StrView n; // name

    ksh_parse_args(parser, &(KshArgs){
        .params = KSH_PARAMS(KSH_PARAM(n, "new repo name"))
    });

    char *new_repo_path = get_repo_path(n);
    if (!new_repo_path) {
        fputs("ERROR: could not allocate memory\n", stderr);
        exit(1);
    }

    FILE *new_repo = fopen(new_repo_path, "w");
    if (!new_repo) {
        fprintf(stderr, "ERROR: could not create new repo %s\n", new_repo_path);
        free(new_repo_path);
        exit(1);
    }

    free(new_repo_path);
    fclose(new_repo);
    return 0;
}

int new(KshParser *parser)
{
    ksh_parse_args(parser, &(KshArgs){
        .subcmds = KSH_SUBCMDS(
            KSH_SUBCMD(new_repo, "repo", "create new repo"),
            KSH_SUBCMD(new_card, "card", "create new card")
        )
    });

    return 0;
}

int root(KshParser *parser)
{
    ksh_parse_args(parser, &(KshArgs){
        .subcmds = KSH_SUBCMDS(
            KSH_SUBCMD(new, "new", "create new (card, repo)")
        )
    });

    return 0;
}

int main(int argc, char **argv)
{
    KshParser parser;
    ksh_init_from_cargs(&parser, argc, argv);
    ksh_parse(&parser, root);
}
