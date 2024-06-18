#include <kovsh.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sword.h"

static int strc_from_args(int argc, const char *argv[], char **result);

int main(int argc, const char *argv[])
{
    char *prompt;
    int err = strc_from_args(argc, argv, &prompt);
    if (err != 0) return err;

    ksh_use_builtin_commands();
    ksh_use_command_set(menu_command_set);

    KshErr error;
    CommandCall cmd_call;
    ksh_init();

    StrView prompt_strv = strv_from_str(prompt);
    error = ksh_parse(prompt_strv, &cmd_call);
    if (error != KSH_ERR_OK) {
        fprintf(stderr, "ERROR: %s\n", ksh_err_str(error));
        return error;
    }

    error = ksh_cmd_call_execute(cmd_call);
    if (error != KSH_ERR_OK) {
        fprintf(stderr, "ERROR: %s\n", ksh_err_str(error));
        return error;
    }

    ksh_deinit();
    return 0;
}



static int strc_from_args(int argc, const char *argv[], char **result)
{
    size_t strc_len = 0;
    size_t strc_cursor = 0;

    for (int i = 1; i < argc; i++) {
        strc_len += strlen(argv[i]) + 1;
    }

    char *strc_buf = (char *) malloc(strc_len);
    if (!strc_buf) {
        fputs("ERROR: could not allocate memory\n", stderr);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        strcpy(&strc_buf[strc_cursor], argv[i]);
        strc_cursor += strlen(argv[i]) + 1;
        strc_buf[strc_cursor-1] = ' ';
    }

    *result = strc_buf;
    return 0;
}
