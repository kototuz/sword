#ifndef SWORD_H
#define SWORD_H

#include <kovsh.h>
#include <stdio.h>

#define REPOSITORIES_PATH "resources/repositories"

typedef enum {
    KNOWLEDGE_LEVEL_OK,
    KNOWLEDGE_LEVEL_NORMAL,
    KNOWLEDGE_LEVEL_HARD
} KnowledgeLevel;

typedef struct {
    char *text;
    char *translation;
} FlashCard;

typedef struct {
    FlashCard *cards;
    size_t cards_len;
} FlashCardRepo;

typedef int Errno;

extern const CommandSet menu_command_set;
extern const CommandSet exam_command_set;

typedef struct {
    unsigned read   : 1;
    unsigned append : 1;
} RepoOpenMode;

// UTILS
Errno strc_from_strv(StrView strv, char **result);

Errno repo_new(StrView name);
Errno repo_del(StrView name);
Errno repo_open(StrView name, RepoOpenMode mode);
void  repo_close();

void repo_ser_fc(FlashCard);
bool repo_deser_fc(FlashCard *);


#endif
