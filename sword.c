#include "kovsh.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define REPOS_DIR "./repos.d/"



typedef FILE *Repo;



// TODO: open_repo instead
static char *get_repo_path(StrView name)
{
    char *result = (char *) malloc(name.len + sizeof(REPOS_DIR));
    if (!result) return NULL;
    strcpy(result, REPOS_DIR);
    memcpy(&result[sizeof(REPOS_DIR)-1], name.items, name.len);
    return result;
}

static Repo open_repo(StrView repo_name, const char *mode)
{
    char *path = get_repo_path(repo_name);
    if (!path) return NULL;
    Repo repo = fopen(path, mode);
    if (!repo) return NULL;
    free(path);
    return repo;
}

static void deleteline(StrView repo_name, size_t ln)
{
    Repo f = open_repo(repo_name, "r");
    if (!f) { return; }

    Repo copy = fopen("copy", "w");
    if (!copy) { return; }

    int s;
    size_t i = 0;
    while ((s = fgetc(f)) != EOF) {
        if (s == '\n') i++;
        if (i == ln) continue;
    }

    fclose(f);
    fclose(copy);

    char *path = get_repo_path(repo_name);
    if (!path) return;

    remove(path);
    rename("copy", path);

    free(path);
}

static bool get_card_line_number(StrView repo_name, StrView label, size_t *result)
{
    Repo repo = open_repo(repo_name, "r");
    if (!repo) return false;

    size_t bufsize = label.len+2;
    char *buf = (char *) malloc(bufsize);
    if (!buf) {
        fclose(repo);
        return false;
    }


    int s = '\n';
    size_t ln = 0;
    do {
        if (s == '\n') {
            ln++;
            if (!fgets(buf, bufsize, repo)) {
                fclose(repo);
                free(buf);
                return false;
            }

            if (strv_eq(label, strv_new(buf, bufsize-2))) {
                *result = ln-1;
                fclose(repo);
                free(buf);
                return true;
            }
        }
    } while ((s = fgetc(repo)) != EOF);

    free(buf);
    fclose(repo);
    return false;
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

    Repo repo = open_repo(r, "r");
    if (!repo) {
        fprintf(stderr, "ERROR: could not open repo "STRV_FMT"\n", STRV_ARG(r));
        exit(1);
    }
    fclose(repo);

    repo = open_repo(r, "a");
    fprintf(repo, STRV_FMT"="STRV_FMT"\n", STRV_ARG(l), STRV_ARG(t));

    fclose(repo);
    return 0;
}

// TODO: check if the repo already exists
int new_repo(KshParser *parser)
{
    StrView n; // name

    ksh_parse_args(parser, &(KshArgs){
        .params = KSH_PARAMS(KSH_PARAM(n, "new repo name"))
    });

    Repo new_repo = open_repo(n, "w");
    if (!new_repo) {
        fprintf(stderr, "ERROR: could not create new repo "STRV_FMT"\n", STRV_ARG(n));
        exit(1);
    }

    fclose(new_repo);
    return 0;
}

int del_card(KshParser *parser)
{
    StrView r; // repo
    StrView l; // label

    ksh_parse_args(parser, &(KshArgs){
        .params = KSH_PARAMS(
            KSH_PARAM(r, "repo"),
            KSH_PARAM(l, "flashcard label")
        )
    });

    size_t ln;
    if (!get_card_line_number(r, l, &ln)) {
        fprintf(stderr, "ERROR: could not find label "STRV_FMT" in repo "STRV_FMT"\n",
                STRV_ARG(l), STRV_ARG(r));
        exit(1);
    }

    deleteline(r, ln);

    return 0;
}

int del_repo(KshParser *parser)
{
    StrView n; // name

    ksh_parse_args(parser, &(KshArgs){
        .params = KSH_PARAMS(KSH_PARAM(n, "repo name"))
    });

    char *repo_path = get_repo_path(n);
    if (!repo_path) {
        fputs("ERROR: could not allocate memory\n", stderr);
        exit(1);
    }

    if (remove(repo_path) < 0) {
        fprintf(stderr, "ERROR: could not delete repo %s\n", repo_path);
        free(repo_path);
        exit(1);
    }

    free(repo_path);
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

int del(KshParser *parser)
{
    ksh_parse_args(parser, &(KshArgs){
        .subcmds = KSH_SUBCMDS(
            KSH_SUBCMD(del_repo, "repo", "delete repo"),
            KSH_SUBCMD(del_card, "card", "delete card")
        )
    });

    return 0;
}

int root(KshParser *parser)
{
    ksh_parse_args(parser, &(KshArgs){
        .subcmds = KSH_SUBCMDS(
            KSH_SUBCMD(new, "new", "create new (card, repo)"),
            KSH_SUBCMD(del, "del", "delete")
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



// TODO: better error message
// TODO: exit(1) -> return 1
