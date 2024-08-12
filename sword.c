#include "kovsh.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#define REPOS_DIR "./repos.d/"



typedef int Err;
typedef FILE *Repo;



static void *alloc(size_t size);

static size_t file_read_until_delim_alloc(FILE *f, int until, char **result);
static size_t file_read_until_delim(FILE *f, int until, char *buf, size_t bufsize);

static Err remove_repo_line(StrView file_name, size_t ln);
static Repo open_repo(StrView repo_name, const char *mode);
static char *get_repo_path(StrView name);



// TODO: check if the card already exists
int card_new(KshParser *parser)
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
    fclose(repo);
    repo = open_repo(r, "a");

    fprintf(repo, STRV_FMT"="STRV_FMT"\n", STRV_ARG(l), STRV_ARG(t));

    fclose(repo);
    return 0;
}

int card_del(KshParser *parser)
{
    StrView r; // repo
    StrView l; // label

    ksh_parse_args(parser, &(KshArgs){
        .params = KSH_PARAMS(
            KSH_PARAM(r, "repo"),
            KSH_PARAM(l, "flashcard label")
        )
    });

    Repo repo = open_repo(r, "r");

    size_t bufsize = l.len+2;
    char *buf = (char *) alloc(bufsize);

    memset(buf, '\0', bufsize);

    for (size_t line_num = 0;
         fgets(buf, bufsize, repo);
         line_num++)
    {
        if (buf[bufsize-2] == '=') {
            if (strv_eq(l, strv_new(buf, bufsize-2))) {
                fclose(repo);
                remove_repo_line(r, line_num);
                return 0;
            }
        }

        while (fgetc(repo) != '\n');
    }

    fprintf(stderr,
            "ERROR: could not find label `"STRV_FMT"` in repo `"STRV_FMT"`\n",
            STRV_ARG(l),
            STRV_ARG(r));

    return 1;
}

int repo_new(KshParser *parser)
{
    StrView n; // name

    ksh_parse_args(parser, &(KshArgs){
        .params = KSH_PARAMS(KSH_PARAM(n, "new repo name"))
    });

    Repo new_repo = open_repo(n, "w");
    fclose(new_repo);

    return 0;
}

int repo_del(KshParser *parser)
{
    StrView n; // name

    ksh_parse_args(parser, &(KshArgs){
        .params = KSH_PARAMS(KSH_PARAM(n, "repo name"))
    });

    char *repo_path = get_repo_path(n);
    if (remove(repo_path) < 0) {
        fprintf(stderr, "ERROR: could not delete repo `"STRV_FMT"`: %s\n",
                STRV_ARG(n),
                strerror(errno));
        free(repo_path);
        return 1;
    }

    free(repo_path);
    return 0;
}

int repo_list(KshParser *parser)
{
    (void) parser;

    DIR *d = opendir(REPOS_DIR);
    if (!d) {
        fprintf(stderr, "ERROR: could not open the repos directory: %s\n",
                strerror(errno));
        return 1;
    }

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (strncmp(dir->d_name, ".", 1) == 0) continue;
        puts(dir->d_name);
    }

    return 0;
}

int repo_dump(KshParser *parser)
{
    StrView n; // repo name

    ksh_parse_args(parser, &(KshArgs){
        .params = KSH_PARAMS(KSH_PARAM(n, "repo name"))
    });

    Repo repo = open_repo(n, "r");

    char buf[100];
    while (fgets(buf, sizeof(buf), repo)) {
        printf(buf);
    }

    return 0;
}

int repo_exam(KshParser *parser)
{
    StrView n; // repo name

    ksh_parse_args(parser, &(KshArgs){
        .params = KSH_PARAMS(KSH_PARAM(n, "repo name"))
    });

    Repo repo = open_repo(n, "r");

    int s;
    while ((s = fgetc(repo)) != EOF) {
        while (s != '=') {
            fputc(s, stdout);
            s = fgetc(repo);
        }

        fgetc(stdin);
        puts("=");

        while ((s = fgetc(repo)) != '\n' && s != EOF) {
            fputc(s, stdout);
        }

        puts("\n--------------------");
    }

    return 0;
}

int manage_repos(KshParser *parser)
{
    ksh_parse_args(parser, &(KshArgs){
        .subcmds = KSH_SUBCMDS(
            KSH_SUBCMD(repo_new, "new", "create new repo"),
            KSH_SUBCMD(repo_del, "del", "delete repo"),
            KSH_SUBCMD(repo_list, "list", "list all repositories"),
            KSH_SUBCMD(repo_dump, "dump", "dump repo to stdout"),
            KSH_SUBCMD(repo_exam, "exam", "examine repo")
        )
    });

    return 0;
}

int manage_cards(KshParser *parser)
{
    ksh_parse_args(parser, &(KshArgs){
        .subcmds = KSH_SUBCMDS(
            KSH_SUBCMD(card_new, "new", "create new card"),
            KSH_SUBCMD(card_del, "del", "delete card")
        )
    });

    return 0;
}


int root(KshParser *parser)
{
    ksh_parse_args(parser, &(KshArgs){
        .subcmds = KSH_SUBCMDS(
            KSH_SUBCMD(manage_cards, "card", "manage cards"),
            KSH_SUBCMD(manage_repos, "repo", "manage repos"),
        )
    });

    return 0;
}

int main(int argc, char **argv)
{
    KshParser parser;
    ksh_init_from_cargs(&parser, argc, argv);
    ksh_parse(&parser, root);

    return parser.cmd_exit_code;
}




static char *get_repo_path(StrView name)
{
    char *result = (char *) alloc(name.len + sizeof(REPOS_DIR));
    strcpy(result, REPOS_DIR);
    memcpy(&result[sizeof(REPOS_DIR)-1], name.items, name.len);
    result[name.len + sizeof(REPOS_DIR) - 1] = '\0';
    return result;
}

static Repo open_repo(StrView repo_name, const char *mode)
{
    char *path = get_repo_path(repo_name);
    if (!path) return NULL;
    Repo repo = fopen(path, mode);
    if (!repo) {
        fprintf(stderr,
                "ERROR: could not open repo `"STRV_FMT"`: %s\n",
                STRV_ARG(repo_name),
                strerror(errno));
        exit(1);
    }

    free(path);
    return repo;
}

static size_t file_read_until_delim(FILE *f,
                                    int until,
                                    char *buf,
                                    size_t bufsize)
{
    assert(until);

    buf[--bufsize] = '\0';

    int s = 0;
    size_t i = 0;
    for (; (s = fgetc(f)) != until && s != EOF && i < bufsize; i++)
    {
        buf[i] = s;
    }

    return i;
}

static size_t file_read_until_delim_alloc(FILE *f,
                                          int until,
                                          char **result)
{
    assert(until);

    int s;
    size_t length = 0;
    while ((s = fgetc(f)) != until && s != EOF) length++;

    if (length == 0) return 0;

    char *buf = (char *) alloc(length+1);
    buf[length] = '\0';

    fseek(f, -length, SEEK_CUR);

    for (size_t i = 0; i < length; i++) {
        buf[i] = fgetc(f);
    }

    *result = buf;
    return length;
}

static Err remove_repo_line(StrView file_name, size_t ln)
{
    Repo f = open_repo(file_name, "r");
    if (!f) return 1;

    Repo copy = open_repo(STRV_LIT("copy"), "w");
    if (!copy) return 1;

    int s;
    size_t i = 0;
    while ((s = fgetc(f)) != EOF) {
        if (s == '\n') i++;
        if (i == ln) continue;
        fputc(s, copy);
    }

    fclose(f);
    fclose(copy);

    char *path = get_repo_path(file_name);
    if (!path) return 1;
    char *copy_path = get_repo_path(STRV_LIT("copy"));
    if (!copy_path) return 1;

    remove(path);
    rename(copy_path, path);

    free(copy_path);
    free(path);
    return 0;
}

static void *alloc(size_t size)
{
    void *result = malloc(size);
    if (!result) {
        perror("ERROR: could not allocate memory");
        exit(1);
    }

    return result;
}

// TODO: implement repo cursor like a checkpoint
