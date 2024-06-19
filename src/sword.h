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
    const char *text;
    const char *translation;
} FlashCard;

typedef struct {
    FlashCard *cards;
    size_t cards_len;
} FlashCardRepo;

typedef int Errno;

extern const CommandSet menu_command_set;
extern const CommandSet exam_command_set;

#endif
