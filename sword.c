#include "kovsh/kovsh.h"
#include "cvector.h"

#define _XOPEN_SOURCE 700
#include <ncursesw/ncurses.h>
#include <ncursesw/menu.h>
#include <locale.h>


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

static MemLvl exam_fc_simple(FlashCard fc);
static void   render_fc_tui();
static MemLvl exam_fc_tui(FlashCard fc);



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
    bool tui = false;

    ksh_parse_args(parser, &(KshArgs){
        .params = KSH_PARAMS(KSH_PARAM(n, "repo name")),
        .flags = KSH_FLAGS(KSH_FLAG(tui, "Enable tui?"))
    });

    MemLvl (*exam_fc_fn)(FlashCard);
    if (tui) {
        setlocale(LC_ALL, "");
        initscr();
        render_fc_tui();
        exam_fc_fn = exam_fc_tui;
    } else {
        exam_fc_fn = exam_fc_simple;
    }

    repo_load(n);
    {
        for (MemLvl i = 0; i < MEM_LVL_COUNT; i++) {
            for (size_t j = 0; j < cvector_size(REPO.groups[i]);) {
                MemLvl new_lvl = exam_fc_fn(REPO.groups[i][j]);
                if (new_lvl != i) {
                    cvector_push_back(REPO.groups[new_lvl], REPO.groups[i][j]);
                    cvector_erase(REPO.groups[i], j);
                } else j++;
            }
        }

        REPO.cursor.num = 0;
    }
    repo_store();

    if (tui) endwin();
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



static MemLvl exam_fc_simple(FlashCard fc)
{
    printf(STRV_FMT, STRV_ARG(fc.label));
    fgetc(stdin);
    printf(STRV_FMT, STRV_ARG(fc.transcript));

    puts("\n--------------------------");
    while (true) {
        printf("[0|1|2]: ");
        MemLvl mem_lvl = fgetc(stdin) - '0';
        while (fgetc(stdin) != '\n') {}
        if (mem_lvl >= MEM_LVL_HARD && mem_lvl <= MEM_LVL_GOOD) {
            puts("--------------------------");
            return mem_lvl;
        }
    }
    puts("--------------------------");
}

#define FC_WIN_WIDTH  30
#define FC_WIN_HEIGHT 15
static void render_fc_tui(void)
{
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    box(stdscr, 0, 0);
    start_color();
    init_pair(1, COLOR_YELLOW, COLOR_BLACK);
    bkgd(COLOR_PAIR(1));
}

// assumes that ncurses is initialized
static MemLvl exam_fc_tui(FlashCard fc)
{
    clear();
    box(stdscr, 0, 0);

    mvprintw(1, (getmaxx(stdscr) - fc.label.len)*0.5,
             STRV_FMT, STRV_ARG(fc.label));

    ITEM *menu_items[] = {
        new_item("hard", ""),
        new_item("norm", ""),
        new_item("good", ""),
        new_item("show", ""),
        NULL
    };

    MENU *menu = new_menu(&menu_items[3]);
    set_menu_sub(menu, derwin(stdscr, 0, 0, getmaxy(stdscr)-1, (getmaxx(stdscr)-4)*0.5));
    set_menu_mark(menu, "");
    post_menu(menu);

    while (getch() != 10) {}

    mvprintw(2, (getmaxx(stdscr) - fc.transcript.len)*0.5,
             STRV_FMT, STRV_ARG(fc.transcript));

    free_item(menu_items[3]);
    menu_items[3] = NULL;
    unpost_menu(menu);
    set_menu_sub(menu, derwin(stdscr, 0, 0, getmaxy(stdscr)-1, (getmaxx(stdscr)-14)*0.5));
    set_menu_items(menu, menu_items);
    set_menu_format(menu, 1, 3);
    set_current_item(menu, menu_items[1]);
    box(stdscr, 0, 0);
    post_menu(menu);

    int c;
    while ((c = getch()) != 10) {
        switch (c) {
        case KEY_LEFT:  menu_driver(menu, REQ_PREV_ITEM); break;
        case KEY_RIGHT: menu_driver(menu, REQ_NEXT_ITEM); break;
        }
    }

    ITEM *cur_item = current_item(menu);
    if      (cur_item == menu_items[0]) return MEM_LVL_HARD;
    else if (cur_item == menu_items[1]) return MEM_LVL_NORM;
    else if (cur_item == menu_items[2]) return MEM_LVL_GOOD;
    assert(0 && "unreachable");
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

    fclose(repo_file);
}



// TODO: rewrite tests in C
// TODO: we shouldn't use `scanf()` for numbers
// TODO: lazy flashcard loading
// TODO: level of memorization
// TODO: better ui
