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

#define EMPTY_REPO "0 0\n"



typedef int Err;

typedef struct {
    StrView label;
    StrView transcript;
} FlashCard;

typedef struct {
    FlashCard *cards;
    size_t cards_count;
} FlashCardGroup;


static void *alloc(size_t size);

static FILE *open_repo(StrView repo_name, const char *mode);
static char *get_repo_path(StrView repo_name);

static void repo_load(StrView repo_name);
static void repo_store();

static bool exam_fc_simple(FlashCard fc);
static void render_fc_tui();
static bool exam_fc_tui(FlashCard fc);



static struct {
    StrView name;
    cvector(FlashCard) cards;
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
    {
        size_t size = cvector_size(REPO.cards);
        for (size_t i = 0; i < size; i++) {
            if (strv_eq(l, REPO.cards[i].label)) {
                fprintf(stderr,
                        "ERROR: card with label `"STRV_FMT"` already exists\n",
                        STRV_ARG(l));
                return 1;
            }
        }

        REPO.textbuf_size += l.len + t.len;
        FlashCard new_fc = { l, t };
        cvector_push_back(REPO.cards, new_fc);
    }
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
    {
        size_t size = cvector_size(REPO.cards);
        for (size_t i = 0; i != size; i++) {
            if (strv_eq(l, REPO.cards[i].label)) {
                cvector_erase(REPO.cards, i);
                REPO.textbuf_size -= REPO.cards[i].label.len
                                   + REPO.cards[i].transcript.len;
                repo_store();
                return 0;
            }
        }

        fprintf(stderr,
                "ERROR: could not find label `"STRV_FMT"` in repo `"STRV_FMT"`\n",
                STRV_ARG(l),
                STRV_ARG(r));
    }
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

    bool (*exam_fc_fn)(FlashCard);
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
        cvector(FlashCard) repeat = {0};
        size_t size;
        size_t i;

        size = cvector_size(REPO.cards);
        for (i = 0; i < size; i++) {
            if (!exam_fc_fn(REPO.cards[i])) {
                cvector_push_back(repeat, REPO.cards[i]);
            }
        }

        while (cvector_size(repeat)) {
            for (i = 0; i < cvector_size(repeat);) {
                if (exam_fc_fn(repeat[i])) {
                    cvector_erase(repeat, i);
                } else i++;
            }
        }

        cvector_free(repeat);
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



static bool exam_fc_simple(FlashCard fc)
{
    printf(STRV_FMT, STRV_ARG(fc.label));
    fgetc(stdin);
    printf(STRV_FMT, STRV_ARG(fc.transcript));

    puts("\n--------------------------");
    while (true) {
        fflush(stdin);
        char buf[6];
        printf("[hard/ok]: ");
        fgets(buf, sizeof(buf), stdin);
        puts("--------------------------");
        if      (strcmp(buf, "hard\n") == 0) return false;
        else if (strcmp(buf, "ok\n") == 0)   return true;
    }
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
static bool exam_fc_tui(FlashCard fc)
{
    clear();
    box(stdscr, 0, 0);

    mvprintw(1, (getmaxx(stdscr) - fc.label.len)*0.5,
             STRV_FMT, STRV_ARG(fc.label));

    ITEM *menu_items[] = {
        new_item("hard", ""),
        new_item(" OK", ""),
        new_item("show", ""),
        NULL
    };

    MENU *menu = new_menu(&menu_items[2]);
    set_menu_sub(menu, derwin(stdscr, 0, 0, getmaxy(stdscr)-1, (getmaxx(stdscr)-4)*0.5));
    set_menu_mark(menu, "");
    post_menu(menu);

    while (getch() != 10) {}

    mvprintw(2, (getmaxx(stdscr) - fc.transcript.len)*0.5,
             STRV_FMT, STRV_ARG(fc.transcript));

    free_item(menu_items[2]);
    menu_items[2] = NULL;
    unpost_menu(menu);
    set_menu_sub(menu, derwin(stdscr, 0, 0, getmaxy(stdscr)-1, (getmaxx(stdscr)-9)*0.5));
    set_menu_items(menu, menu_items);
    set_menu_format(menu, 1, 2);
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
    if      (cur_item == menu_items[0]) return false;
    else if (cur_item == menu_items[1]) return true;
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

static void *alloc(size_t size)
{
    void *result = malloc(size);
    if (!result) {
        perror("ERROR: could not allocate memory");
        exit(1);
    }

    return result;
}

static void repo_store()
{
    FILE *repo_file = open_repo(REPO.name, "w");

    fprintf(repo_file,
            "%zu %zu\n",
            REPO.textbuf_size,
            cvector_size(REPO.cards));

    FlashCard *it = cvector_begin(REPO.cards);
    FlashCard *end = cvector_end(REPO.cards);
    for (; it != end; it++) {
        fprintf(repo_file,
                STRV_FMT"="STRV_FMT"\n",
                STRV_ARG(it->label),
                STRV_ARG(it->transcript));
    }

    cvector_free(REPO.cards);

    fclose(repo_file);
    if (REPO.textbuf) free(REPO.textbuf);
}

static void repo_load(StrView repo_name)
{
    FILE *repo_file = open_repo(repo_name, "r");
    REPO.name = repo_name;

    // repo info
    size_t cards_count;
    fscanf(repo_file, "%zu %zu\n",
           &REPO.textbuf_size,
           &cards_count);

    if (!REPO.textbuf_size) {
        fclose(repo_file);
        return;
    }

    REPO.textbuf = (char *) alloc(REPO.textbuf_size);
    cvector_init(REPO.cards, cards_count, NULL);

    int symbol;
    char *buf = REPO.textbuf;
    for (size_t i = 0; i < cards_count; i++) {
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

        cvector_push_back(REPO.cards, new_fc);
    }

    fclose(repo_file);
}


// TODO: random cards
// TODO: card limit per exam
// TODO: maybe remove the `card` subcommand
// TODO: rewrite tests in C
// TODO: we shouldn't use `scanf()` for numbers
