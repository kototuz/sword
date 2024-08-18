#include "kovsh/kovsh.h"
#include "cvector.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <stdint.h>

#define REPOS_DIR "./repos.d/"

#define EMPTY_REPO "0 0 0\n0\n0\n0\n"



typedef int Err;

typedef enum {
    MEM_LVL_HARD = 0,
    MEM_LVL_NORM,
    MEM_LVL_GOOD,
    MEM_LVL_COUNT
} MemLvl;

typedef struct {
    StrView label;
    StrView transcript;
} FlashCard;

typedef struct {
    FlashCard *cards;
    size_t cards_count;
} FlashCardGroup;


static void *alloc(size_t size);

static size_t file_read_until_delim_alloc(FILE *f, int until, char **result);
static size_t file_read_until_delim(FILE *f, int until, char *buf, size_t bufsize);

static Err remove_repo_line(StrView file_name, size_t ln);
static FILE *open_repo(StrView repo_name, const char *mode);
static char *get_repo_path(StrView repo_name);

static void repo_del_card_and_store(size_t line_nr);
static void repo_load(StrView repo_name);
static void repo_store();
static void repo_print_info();



static struct {
    StrView name;

    struct {
        size_t num;
        MemLvl group;
    } cursor;

    cvector(FlashCard) groups[MEM_LVL_COUNT];

    size_t textbuf_size;
    char *textbuf;
} REPO = {0};

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

    repo_load(r);
        for (MemLvl i = 0; i < MEM_LVL_COUNT; i++) {
            FlashCard *it = cvector_begin(REPO.groups[i]);
            FlashCard *end = cvector_end(REPO.groups[i]);
            for (; it != end; it++) {
                if (strv_eq(l, it->label)) {
                    fprintf(stderr,
                            "ERROR: card with label `"STRV_FMT"` already exists\n",
                            STRV_ARG(l));
                    return 1;
                }
            }
        }

        REPO.textbuf_size += l.len + t.len;
        FlashCard new_fc = { l, t };
        cvector_push_back(REPO.groups[MEM_LVL_HARD], new_fc);
    repo_store();

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

    repo_load(r);
        for (MemLvl i = 0; i < MEM_LVL_COUNT; i++) {
            for (size_t j = 0; j < cvector_size(REPO.groups[i]); j++) {
                FlashCard fc = REPO.groups[i][j];
                if (strv_eq(l, fc.label)) {
                    cvector_erase(REPO.groups[i], j);
                    REPO.textbuf_size -= fc.label.len + fc.transcript.len;
                    repo_store();
                    return 0;
                }
            }
        }

    fprintf(stderr,
            "ERROR: could not find label `"STRV_FMT"` in repo `"STRV_FMT"`\n",
            STRV_ARG(l),
            STRV_ARG(r));
    repo_store();

    return 1;
}

int repo_new(KshParser *parser)
{
    StrView n; // name

    ksh_parse_args(parser, &(KshArgs){
        .params = KSH_PARAMS(KSH_PARAM(n, "new repo name"))
    });

    FILE* new_repo = open_repo(n, "w");
    fputs(EMPTY_REPO, new_repo);
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

    FILE* repo = open_repo(n, "r");

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

    repo_load(n);
        for (MemLvl i = 0; i < MEM_LVL_COUNT; i++) {
            for (size_t j = 0; j < cvector_size(REPO.groups[i]);) {
                printf(STRV_FMT, STRV_ARG(REPO.groups[i][j].label));
                fgetc(stdin);
                printf(STRV_FMT, STRV_ARG(REPO.groups[i][j].transcript));

                puts("\n--------------------------");
                while (true) {
                    printf("[0|1|2/q]: ");
                    MemLvl cmd = fgetc(stdin);
                    while (fgetc(stdin) != '\n') {}
                    if (cmd == 'q') {
                        REPO.cursor.group = i;
                        REPO.cursor.num = j;
                        repo_store();
                        return 0;
                    } else if ((cmd -= '0') >= MEM_LVL_HARD && cmd <= MEM_LVL_GOOD) {
                        if (cmd == i) { j++; break; }
                        cvector_push_back(REPO.groups[cmd], REPO.groups[i][j]);
                        cvector_erase(REPO.groups[i], j);
                        break;
                    }
                }
                puts("--------------------------");
            }
        }
    REPO.cursor.num = 0;
    repo_store();

    return 0;
}

int manage_repos(KshParser *parser)
{
    ksh_parse_args(parser, &(KshArgs){
        .subcmds = KSH_SUBCMDS(
            KSH_SUBCMD(repo_new, "new", "create new repo"),
            KSH_SUBCMD(repo_del, "del", "delete repo"),
            KSH_SUBCMD(repo_dump, "dump", "dump repo to stdout"),
            KSH_SUBCMD(repo_list, "list", "list all repositories"),
            KSH_SUBCMD(repo_exam, "exam", "examine repo"),
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

static FILE* open_repo(StrView repo_name, const char *mode)
{
    char *path = get_repo_path(repo_name);
    if (!path) return NULL;
    FILE* repo = fopen(path, mode);
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
    FILE* f = open_repo(file_name, "r");
    if (!f) return 1;

    FILE* copy = open_repo(STRV_LIT("copy"), "w");
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

static void repo_del_card_and_store(size_t line_nr)
{
    (void) line_nr;
}

static void repo_print_info()
{
    printf("repo `"STRV_FMT"`:\n", STRV_ARG(REPO.name));
    printf("\tcursor group:  %d\n",  REPO.cursor.group);
    printf("\tcursor number: %zu\n", REPO.cursor.num);
    printf("\ttextbuf_size:  %zu\n", REPO.textbuf_size);

    for (MemLvl i = 0; i < MEM_LVL_COUNT; i++) {
        printf("\tgroup(lvl=%d, count=%zu):\n", i, cvector_size(REPO.groups[i]));
        FlashCard *it = cvector_begin(REPO.groups[i]);
        FlashCard *end = cvector_end(REPO.groups[i]);
        for (; it != end; it++) {
            printf("\t\tlabel="STRV_FMT", transcript="STRV_FMT"\n",
                   STRV_ARG(it->label),
                   STRV_ARG(it->transcript));
        }
    }
}

static void repo_store()
{
    FILE *repo_file = open_repo(REPO.name, "w");

    fprintf(repo_file,
            "%d %zu %zu\n",
            REPO.cursor.group,
            REPO.cursor.num,
            REPO.textbuf_size);

    for (MemLvl i = 0; i < MEM_LVL_COUNT; i++) {
        fprintf(repo_file, "%zu\n", cvector_size(REPO.groups[i]));

        FlashCard *it = cvector_begin(REPO.groups[i]);
        FlashCard *end = cvector_end(REPO.groups[i]);
        for (; it != end; it++) {
            fprintf(repo_file,
                    STRV_FMT"="STRV_FMT"\n",
                    STRV_ARG(it->label),
                    STRV_ARG(it->transcript));
        }

        cvector_free(REPO.groups[i]);
    }

    fclose(repo_file);
    if (REPO.textbuf) free(REPO.textbuf);
}

static void repo_load(StrView repo_name)
{
    FILE *repo_file = open_repo(repo_name, "r");
    REPO.name = repo_name;

    // repo info
    fscanf(repo_file, "%d %zu %zu\n",
           (int *)&REPO.cursor.group,
           &REPO.cursor.num,
           &REPO.textbuf_size);

    if (!REPO.textbuf_size) {
        fclose(repo_file);
        return;
    }

    REPO.textbuf = (char *) alloc(REPO.textbuf_size);

    int symbol;
    size_t cards_count;
    char *buf = REPO.textbuf;
    for (MemLvl i = 0; i < MEM_LVL_COUNT; i++) {
        fscanf(repo_file, "%zu\n", &cards_count);
        cvector_init(REPO.groups[i], cards_count, NULL);
        while (cards_count--) {
            FlashCard new_fc = {0};

            new_fc.label.items = buf;
            while ((symbol = fgetc(repo_file)) != '=') {
                assert(symbol != EOF && "unreachable");
                *buf++ = symbol;
                new_fc.label.len++;
            }

            new_fc.transcript.items = buf;
            while ((symbol = fgetc(repo_file)) != '\n') {
                assert(symbol != EOF && "unreachabel");
                *buf++ = symbol;
                new_fc.transcript.len++;
            }

            cvector_push_back(REPO.groups[i], new_fc);
        }
    }

    repo_print_info();
    fclose(repo_file);
}



// TODO: rewrite tests in C
// TODO: we shouldn't use `scanf()` for numbers
// TODO: lazy flashcard loading
// TODO: level of memorization
// TODO: better ui
