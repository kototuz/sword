#include "kovsh/kovsh.h"

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

#define LEVELS_COUNT 10
#define CARD_LIMIT   30

#define FC_BUF_SIZE  (REPO.fc_groups[LEVELS_COUNT-1].end_idx)
#define FC_BUF_BEGIN (REPO.fc_groups[0].beg_idx)


typedef int Err;

typedef enum {
    FC_LEVEL_MOVE_NONE,
    FC_LEVEL_MOVE_DOWN,
    FC_LEVEL_MOVE_UP
} FCLevelMove;

typedef struct {
    StrView label;
    StrView transcript;
    FCLevelMove lvl_move;
} FlashCard;

typedef struct {
    size_t beg_idx;
    size_t end_idx;
} FCGroup;

typedef enum {
    FC_GROUP_KIND_EXAM_BEGIN = 0,
    FC_GROUP_KIND_EXAM_END   = 9,
    FC_GROUP_KIND_UNEXAM,
} FCGroupKind;



static void *alloc(size_t size);
static size_t cstr_wc_len(const char *cstr, size_t len);

static FILE *open_repo(StrView repo_name, const char *mode);
static char *get_repo_path(StrView repo_name);

static void repo_load(StrView repo_name);
static void repo_store();

static bool exam_fc_simple(FlashCard fc, size_t remains, size_t repetition);
static void render_fc_tui();
static bool exam_fc_tui(FlashCard fc, size_t remains, size_t repetition);


static struct {
    StrView name;
    FlashCard *fc_buf;
    FCGroup fc_groups[LEVELS_COUNT];
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
        for (size_t i = FC_BUF_BEGIN; i < FC_BUF_SIZE; i++) {
            if (strv_eq(l, REPO.fc_buf[i].label)) {
                fprintf(stderr,
                        "ERROR: card with label `"STRV_FMT"` already exists\n",
                        STRV_ARG(l));
                return 1;
            }
        }

        REPO.textbuf_size += l.len + t.len;
        REPO.fc_buf[0] = (FlashCard){ l, t, FC_LEVEL_MOVE_NONE };
        REPO.fc_groups[0].beg_idx = 0;
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
        for (FCGroupKind i = 0; i < LEVELS_COUNT; i++) {
            FCGroup group = REPO.fc_groups[i];
            for (size_t j = group.beg_idx; j < group.end_idx; j++) {
                if (strv_eq(l, REPO.fc_buf[j].label)) {
                    REPO.textbuf_size -= REPO.fc_buf[j].label.len + REPO.fc_buf[j].transcript.len;
                    REPO.fc_buf[j] = REPO.fc_buf[group.end_idx-1];
                    REPO.fc_groups[i].end_idx--;
                    repo_store();
                    return 0;
                }
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
    fputs("0\n", new_repo);
    for (int i = 0; i < LEVELS_COUNT; i++) {
        fputs("0\n", new_repo);
    }
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

    bool (*exam_fc_fn)(FlashCard, size_t, size_t);
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
        size_t remains;
        size_t *repetition, repetition_count = 0;
        size_t i;

        { // first exam
            remains = (FC_BUF_SIZE > 30 ? 30 : FC_BUF_SIZE) - 2;
            repetition = (size_t *) calloc(remains+1, sizeof(size_t));
            for (int lvl = 0; lvl < LEVELS_COUNT; lvl++) {
                for (i = REPO.fc_groups[lvl].beg_idx; i < REPO.fc_groups[lvl].end_idx; i++) {
                    if (!exam_fc_fn(REPO.fc_buf[i], remains, repetition_count)) {
                        repetition[repetition_count++] = i;
                        REPO.fc_buf[i].lvl_move = FC_LEVEL_MOVE_DOWN;
                    } else if (lvl != LEVELS_COUNT-1) {
                        REPO.fc_buf[i].lvl_move = FC_LEVEL_MOVE_UP;
                    }

                    remains--;
                }
            }
        }

        { // repeating the hard words
            while (repetition_count) {
                for (i = 0; i < repetition_count;) {
                    if (exam_fc_fn(REPO.fc_buf[repetition[i]], 0, repetition_count-1)) {
                        repetition[i] = repetition[--repetition_count];
                    } else i++;
                }
            }
            free(repetition);
        }

        { // rearrange words depending on `lvl_move`
            FlashCard tmp;
            for (int lvl = 0; lvl < LEVELS_COUNT; lvl++) {
                for (i = REPO.fc_groups[lvl].beg_idx; i < REPO.fc_groups[lvl].end_idx;) {
                    switch (REPO.fc_buf[i].lvl_move) {
                        case FC_LEVEL_MOVE_NONE:
                            break;
                        case FC_LEVEL_MOVE_DOWN:
                            if (lvl == 0) break;
                            if (i != REPO.fc_groups[lvl].beg_idx) {
                                tmp = REPO.fc_buf[i];
                                tmp.lvl_move = FC_LEVEL_MOVE_NONE;
                                REPO.fc_buf[i] = REPO.fc_buf[REPO.fc_groups[lvl].beg_idx];
                                REPO.fc_buf[REPO.fc_groups[lvl].beg_idx] = tmp;
                            } else {
                                REPO.fc_buf[i].lvl_move = FC_LEVEL_MOVE_NONE;
                            }
                            REPO.fc_groups[lvl-1].end_idx++;
                            REPO.fc_groups[lvl].beg_idx++;
                            continue;
                        case FC_LEVEL_MOVE_UP:
                            if (lvl == LEVELS_COUNT-1) break;
                            if (i != REPO.fc_groups[lvl].end_idx-1) {
                                tmp = REPO.fc_buf[i];
                                tmp.lvl_move = FC_LEVEL_MOVE_NONE;
                                REPO.fc_buf[i] = REPO.fc_buf[REPO.fc_groups[lvl].end_idx-1];
                                REPO.fc_buf[REPO.fc_groups[lvl].end_idx-1] = tmp;
                            } else {
                                REPO.fc_buf[i].lvl_move = FC_LEVEL_MOVE_NONE;
                            }
                            REPO.fc_groups[lvl+1].beg_idx--;
                            REPO.fc_groups[lvl].end_idx--;
                            continue;
                    }
                    i++;
                }
            }
        }
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
        ),
        .help = "manage repos"
    });

    return 0;
}

int manage_cards(KshParser *parser)
{
    ksh_parse_args(parser, &(KshArgs){
        .subcmds = KSH_SUBCMDS(
            KSH_SUBCMD(card_new, "new", "create new card"),
            KSH_SUBCMD(card_del, "del", "delete card"),
        ),
        .help = "manage cards"
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



static bool exam_fc_simple(FlashCard fc, size_t remains, size_t repetition)
{
    printf("Remains: %zu; Repetition: %zu\n", remains, repetition);
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
static bool exam_fc_tui(FlashCard fc, size_t remains, size_t repetition)
{
    clear();
    box(stdscr, 0, 0);

    mvprintw(1, (getmaxx(stdscr) - cstr_wc_len(fc.label.items, fc.label.len))*0.5,
             STRV_FMT, STRV_ARG(fc.label));

    mvprintw(getmaxy(stdscr)-1, 2, "Remains: %zu Repetition: %zu",
             remains, repetition);

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

    mvprintw(2, (getmaxx(stdscr) - cstr_wc_len(fc.transcript.items, fc.transcript.len))*0.5,
             STRV_FMT, STRV_ARG(fc.transcript));

    free_item(menu_items[2]);
    menu_items[2] = NULL;
    unpost_menu(menu);
    set_menu_sub(menu, derwin(stdscr, 0, 0, getmaxy(stdscr)-1, (getmaxx(stdscr)-9)*0.5));
    set_menu_items(menu, menu_items);
    set_menu_format(menu, 1, 2);
    set_current_item(menu, menu_items[1]);
    box(stdscr, 0, 0);
    mvprintw(getmaxy(stdscr)-1, 2, "Remains: %zu Repetition: %zu",
             remains, repetition);
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

size_t cstr_wc_len(const char *cstr, size_t len)
{
    size_t result = 0;
    for (size_t i = 0; i < len; result++) {
        i +=  mblen(&cstr[i], MB_CUR_MAX);
    }

    return result;
}

static void repo_store()
{
    FILE *repo_file = open_repo(REPO.name, "w");

    fprintf(repo_file, "%zu\n", REPO.textbuf_size);

    size_t i;
    for (i = 0; i < LEVELS_COUNT; i++) {
        fprintf(repo_file, "%zu\n",
                REPO.fc_groups[i].end_idx - REPO.fc_groups[i].beg_idx);
    }

    for (int lvl = 0; lvl < LEVELS_COUNT; lvl++) {
        for (size_t i = REPO.fc_groups[lvl].beg_idx; i < REPO.fc_groups[lvl].end_idx; i++) {
            fprintf(repo_file, STRV_FMT"="STRV_FMT"\n",
                    STRV_ARG(REPO.fc_buf[i].label),
                    STRV_ARG(REPO.fc_buf[i].transcript));
        }
    }

    free(REPO.fc_buf);
    if (REPO.textbuf) free(REPO.textbuf);
    fclose(repo_file);
}

static void repo_load(StrView repo_name)
{
    FILE *repo_file = open_repo(repo_name, "r");
    REPO.name = repo_name;

    // repo info
    fscanf(repo_file, "%zu\n", &REPO.textbuf_size);

    if (REPO.textbuf_size) REPO.textbuf = (char *) alloc(REPO.textbuf_size);

    REPO.fc_groups[0].beg_idx++;  // leave place for new card
    size_t size, fc_buf_size = 1; //                           
    for (FCGroupKind i = 0; i < LEVELS_COUNT; i++) {
        REPO.fc_groups[i].beg_idx = fc_buf_size;
        fscanf(repo_file, "%zu\n", &size);
        fc_buf_size += size;
        REPO.fc_groups[i].end_idx = fc_buf_size;
    }

    REPO.fc_buf = (FlashCard *) calloc(fc_buf_size + 1, sizeof(FlashCard)); // leave place for new card

    int symbol;
    char *buf = REPO.textbuf;
    for (size_t i = FC_BUF_BEGIN; i < fc_buf_size; i++) {
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

        REPO.fc_buf[i] = new_fc;
    }

    fclose(repo_file);
}


// TODO: card rating
// TODO: maybe remove the `card` subcommand
