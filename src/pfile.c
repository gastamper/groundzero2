/*
 * ============================================================================
 *                              _______ _______ 
 *                             |   _   |   _   |
 *                             |.  |___|___|   |
 *             -= g r o u n d  |.  |   |/  ___/  z e r o   2 =-
 *                             |:  1   |:  1  \ 
 *                             |::.. . |::.. . |
 *                             `-------`-------'
 * ============================================================================
 *
 * Module:      pfile.c
 * Synopsis:    Simple routines for managing player files.
 * Author:      Satori
 * Created:     28 Nov 2002
 */

#include "ground0.h"


char *
pfile_name (const char *dir, const char *name)
{
    static char buf[MAX_STRING_LENGTH + 1];
    static char *ptr = buf;
    static int remaining = MAX_STRING_LENGTH;
    char *result = ptr;
    int len;

    len = snprintf(ptr, remaining, "%sPS%s", dir, capitalize(name));

    if ( len > remaining )
    {
        remaining = MAX_STRING_LENGTH - 1;
        ptr = buf;
        len = snprintf(ptr, remaining, "%sPS%s", dir, capitalize(name));

        if ( len > remaining )
        {
            logmesg("Name too long to generate pfile name: %s.", name);
            return (NULL);
        }
    }

    remaining -= len;
    ptr += len;

    /* Ensure it's null terminated. */
    *ptr++ = '\0';
    remaining--;

    return (result);
}


bool
rename_pfile (const char *from, const char *to)
{
    char *oldfn = pfile_name(PLAYER_DIR, from);
    char *newfn = pfile_name(PLAYER_DIR, to);
    return (oldfn && newfn && !rename(oldfn, newfn));
}


bool
remove_pfile (const char *name)
{
    char *fn = pfile_name(PLAYER_DIR, name);
    return (fn && !unlink(fn));
}


bool
backup_pfile (const char *name)
{
    char *current = pfile_name(PLAYER_DIR, name);
    char *backup = pfile_name(BAK_PLAYER_DIR, name);

    if ( !current || !backup || copy_file(current, backup) == -1 )
    {
        logmesg("Couldn't backup player: %s.", name);
        return false;
    }

    return true;
}


bool
player_exists (const char *name)
{
    char *ptr = pfile_name(PLAYER_DIR, name);
    return (ptr && access(ptr, F_OK) == 0);
}
