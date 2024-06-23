#include "sword.h"
#include <errno.h>

Errno strc_from_strv(StrView strv, char **result)
{
    char *buf = (char *) malloc(strv.len);
    if (!buf) return errno;

    memcpy(buf, strv.items, strv.len);

    *result = buf;
    return 0;
}
