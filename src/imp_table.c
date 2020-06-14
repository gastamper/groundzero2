#include "ground0.h"

/* This was "hard coded for security reasons" by previous maintainers at some point. */

/* Subsequently moved from comm.c to here so that a .gitignore could be written for
 * this file, making it harder to accidentally push your own login info back to Git. */

/* Format is {"Login name", Trust Level, "Character name", "login name's password", ""}
 * For example: {"Greg", 10, "Snoopy", "password", ""} to login as Greg and play
 * the character named Snoopy.  Note that "password" applies to Greg and not Snoopy, which
 * must have its own separate password. Trust levels are shown in src/ground0.h */

const struct god_type imp_table[] = {
    {"Test", 10, "Test", "test", ""}
};
