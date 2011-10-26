/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik Starfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/* Online Social Editting Module, 
 * (c) 1996,97 Erwin S. Andreasen <erwin@pip.dknet.dk>
 * See the file "License" for important licensing information
 */

/* This version contains minor modifications to support ROM 2.4b4. */

#if defined(macintosh)
#include <types.h>
#else
#include <sys/types.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ground0.h"
#include "db.h"

#define SOCIAL_FILE "../boot/social.txt"

struct social_type *social_table;       /* and social table */

void
load_social (FILE * fp, struct social_type *social)
{
    social->name = fread_string(fp);
    social->char_no_arg = fread_string(fp);
    social->others_no_arg = fread_string(fp);
    social->char_found = fread_string(fp);
    social->others_found = fread_string(fp);
    social->vict_found = fread_string(fp);
    social->char_auto = fread_string(fp);
    social->others_auto = fread_string(fp);
}

void
load_social_table ()
{
    FILE *fp;
    int i;

    fp = fopen(SOCIAL_FILE, "r");

    if ( !fp )
    {
        logmesg("Could not open " SOCIAL_FILE " for reading.");
        exit(1);
    }

    fscanf(fp, "%d\n", &maxSocial);

    /* IMPORTANT to use malloc so we can realloc later on */

    social_table = malloc(sizeof(struct social_type) * (maxSocial + 1));

    for ( i = 0; i < maxSocial; i++ )
        load_social(fp, &social_table[i]);

    social_table[maxSocial].name = NULL;
    fclose(fp);

}


#define CHK(_str) ((_str) ? (_str) : "")

void
save_social (const struct social_type *s, FILE * fp)
{
    /* get rid of (null) */
    fprintf(fp, "%s~\n%s~\n%s~\n%s~\n%s~\n%s~\n%s~\n%s~\n\n",
            CHK(s->name), CHK(s->char_no_arg), CHK(s->others_no_arg),
            CHK(s->char_found), CHK(s->others_found),
            CHK(s->vict_found), CHK(s->char_auto), CHK(s->others_auto));
}


void
save_social_table ()
{
    FILE *fp;
    int i;

    fp = fopen(SOCIAL_FILE, "w");

    if ( !fp )
    {
        logmesg("Could not open " SOCIAL_FILE " for writing.");
        return;
    }

    fprintf(fp, "%d\n", maxSocial);

    for ( i = 0; i < maxSocial; i++ )
        save_social(&social_table[i], fp);

    fclose(fp);
}


/* Find a social based on name */
int
social_lookup (const char *name)
{
    int i;

    /* Exact match? */
    for ( i = 0; i < maxSocial; i++ )
        if ( !str_cmp(name, social_table[i].name) )
            return i;

    /* Find an abbreviation... */
    for ( i = 0; i < maxSocial; i++ )
        if ( !str_prefix(name, social_table[i].name) )
            return i;

    return -1;
}

/*
 * Social editting command
 */

#ifndef CONST_SOCIAL
void
do_sedit (CHAR_DATA * ch, char *argument)
{
    char cmd[MAX_INPUT_LENGTH], social[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    int iSocial;

    smash_tilde(argument);

    argument = one_argument(argument, cmd);
    argument = one_argument(argument, social);

    if ( !cmd[0] )
    {
        send_to_char("Huh? Type HELP SEDIT to see syntax.\n\r", ch);
        return;
    }

    if ( !social[0] )
    {
        send_to_char("What social do you want to operate on?\n\r", ch);
        return;
    }

    iSocial = social_lookup(social);

    if ( str_cmp(cmd, "new") && (iSocial == -1) )
    {
        send_to_char("No such social exists.\n\r", ch);
        return;
    }

    if ( !str_cmp(cmd, "delete") )
    {                           /* Remove a social */
        int i, j;
        struct social_type *new_table =
            malloc(sizeof(struct social_type) * maxSocial);

        if ( !new_table )
        {
            send_to_char
                ("Memory allocation failed. Brace for impact...\n\r", ch);
            return;
        }

        /* Copy all elements of old table into new table, except the deleted social */
        for ( i = 0, j = 0; i < maxSocial + 1; i++ )
            if ( i != iSocial )
            {                   /* copy, increase only if copied */
                new_table[j] = social_table[i];
                j++;
            }

        free(social_table);
        social_table = new_table;

        maxSocial--;            /* Important :() */

        send_to_char("That social is history now.\n\r", ch);
    }

    else if ( !str_cmp(cmd, "new") )
    {                           /* Create a new social */
        struct social_type *new_table;

        if ( iSocial != -1 )
        {
            send_to_char("A social with that name already exists\n\r", ch);
            return;
        }

        /* reallocate the table */
        /* Note that the table contains maxSocial socials PLUS one empty spot! */

        maxSocial++;
        new_table =
            realloc(social_table,
                    sizeof(struct social_type) * (maxSocial + 1));

        if ( !new_table )
        {                       /* realloc failed */
            send_to_char("Memory allocation failed. Brace for impact.\n\r",
                         ch);
            return;
        }

        social_table = new_table;

        social_table[maxSocial - 1].name = str_dup(social);
        social_table[maxSocial - 1].char_no_arg = NULL;
        social_table[maxSocial - 1].others_no_arg = NULL;
        social_table[maxSocial - 1].char_found = NULL;
        social_table[maxSocial - 1].others_found = NULL;
        social_table[maxSocial - 1].vict_found = NULL;
        social_table[maxSocial - 1].char_auto = NULL;
        social_table[maxSocial - 1].others_auto = NULL;

        /* Terminating empty string. */
        social_table[maxSocial].name = NULL;

        send_to_char("New social added.\n\r", ch);

    }

    else if ( !str_cmp(cmd, "show") )
    {                           /* Show a certain social */
        sprintf(buf, "Social: %s\n\r"
                "(cnoarg) No argument given, character sees:\n\r"
                "%s\n\r\n\r"
                "(onoarg) No argument given, others see:\n\r"
                "%s\n\r\n\r"
                "(cfound) Target found, character sees:\n\r"
                "%s\n\r\n\r"
                "(ofound) Target found, others see:\n\r"
                "%s\n\r\n\r"
                "(vfound) Target found, victim sees:\n\r"
                "%s\n\r\n\r"
                "(cself) Target is character himself:\n\r"
                "%s\n\r\n\r"
                "(oself) Target is character himself, others see:\n\r"
                "%s\n\r",
                CHK(social_table[iSocial].name),
                CHK(social_table[iSocial].char_no_arg),
                CHK(social_table[iSocial].others_no_arg),
                CHK(social_table[iSocial].char_found),
                CHK(social_table[iSocial].others_found),
                CHK(social_table[iSocial].vict_found),
                CHK(social_table[iSocial].char_auto),
                CHK(social_table[iSocial].others_auto));

        send_to_char(buf, ch);
        return;                 /* return right away, do not save the table */
    }

    else if ( !str_cmp(cmd, "cnoarg") )
    {                           /* Set that argument */
        if ( social_table[iSocial].char_no_arg )
        {
            free_string(social_table[iSocial].char_no_arg);
            social_table[iSocial].char_no_arg = NULL;
        }

        if ( *argument )
        {
            social_table[iSocial].char_no_arg = str_dup(argument);
            printf_to_char(ch, "New message is now:\n\r%s\n\r", argument);
        }
        else
            send_to_char
                ("Character will now see nothing when this social is used without arguments.\n\r",
                 ch);
    }

    else if ( !str_cmp(cmd, "onoarg") )
    {
        if ( social_table[iSocial].others_no_arg )
        {
            free_string(social_table[iSocial].others_no_arg);
            social_table[iSocial].others_no_arg = NULL;
        }

        if ( *argument )
        {
            social_table[iSocial].others_no_arg = str_dup(argument);
            printf_to_char(ch, "New message is now:\n\r%s\n\r", argument);
        }
        else
            send_to_char
                ("Others will now see nothing when this social is used without arguments.\n\r",
                 ch);
    }

    else if ( !str_cmp(cmd, "cfound") )
    {
        if ( social_table[iSocial].char_found )
        {
            free_string(social_table[iSocial].char_found);
            social_table[iSocial].char_found = NULL;
        }

        if ( *argument )
        {
            social_table[iSocial].char_found = str_dup(argument);
            printf_to_char(ch, "New message is now:\n\r%s\n\r", argument);
        }
        else
            send_to_char
                ("The character will now see nothing when a target is found.\n\r",
                 ch);
    }

    else if ( !str_cmp(cmd, "ofound") )
    {
        if ( social_table[iSocial].others_found )
        {
            free_string(social_table[iSocial].others_found);
            social_table[iSocial].others_found = NULL;
        }

        if ( *argument )
        {
            social_table[iSocial].others_found = str_dup(argument);
            printf_to_char(ch, "New message is now:\n\r%s\n\r", argument);
        }
        else
            send_to_char
                ("Others will now see nothing when a target is found.\n\r",
                 ch);
    }

    else if ( !str_cmp(cmd, "vfound") )
    {
        if ( social_table[iSocial].vict_found )
        {
            free_string(social_table[iSocial].vict_found);
            social_table[iSocial].vict_found = NULL;
        }

        if ( *argument )
        {
            social_table[iSocial].vict_found = str_dup(argument);
            printf_to_char(ch, "New message is now:\n\r%s\n\r", argument);
        }
        else
            send_to_char
                ("Victim will now see nothing when a target is found.\n\r",
                 ch);
    }

    else if ( !str_cmp(cmd, "cself") )
    {
        if ( social_table[iSocial].char_auto )
        {
            free_string(social_table[iSocial].char_auto);
            social_table[iSocial].char_auto = NULL;
        }

        if ( *argument )
        {
            social_table[iSocial].char_auto = str_dup(argument);
            printf_to_char(ch, "New message is now:\n\r%s\n\r", argument);
        }
        else
            send_to_char
                ("Character will now see nothing when targetting self.\n\r",
                 ch);
    }

    else if ( !str_cmp(cmd, "oself") )
    {
        if ( social_table[iSocial].others_auto )
        {
            free_string(social_table[iSocial].others_auto);
            social_table[iSocial].others_auto = NULL;
        }

        if ( *argument )
        {
            social_table[iSocial].others_auto = str_dup(argument);
            printf_to_char(ch, "New message is now:\n\r%s\n\r", argument);
        }
        else
            send_to_char
                ("Others will now see nothing when character targets self.\n\r",
                 ch);
    }

    else
    {
        send_to_char("Huh? Try HELP SEDIT.\n\r", ch);
        return;
    }

    /* We have done something. update social table */

    save_social_table();
}
#endif                          /* CONST_SOCIAL */
