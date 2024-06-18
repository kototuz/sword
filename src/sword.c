#include "sword.h"

static int menu_command_help(ArgValueCopy *args);
// TODO:
//static int menu_command_repo_list(ArgValueCopy *args);
//static int menu_command_repo_new(ArgValueCopy *args);
//static int menu_command_repo_del(ArgValueCopy *args);

const CommandSet menu_command_set = {
    .len = 1,
    .items = (Command[]){
        {
            .name = STRV_LIT("help"),
            .desc = "Prints all commands",
            .fn = menu_command_help,
        }
    }
};



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
