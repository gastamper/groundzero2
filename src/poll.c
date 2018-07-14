/*
 * A very simple polling system.  Only one poll can be active at any given
 * time and you can't re-open closed polls.  Not a big loss.
 */

#include "ground0.h"


static struct poll_data *poll_active = NULL;
static struct poll_data *poll_list = NULL;


/*
 * Memory Management Functions
 *
 * These are pretty straightforward because we don't bother with recyclying
 * destructed polls (the theory being that polls are not deleted and created
 * on-the-fly enough to justify the overhead of maintaining the recycle list
 * structure).
 */

struct poll_data *
poll_ctor ()
{
    struct poll_data *pnew = NULL;
    pnew = alloc_mem(sizeof(struct poll_data));
    memset((char *) pnew, 0, sizeof(struct poll_data));
    return pnew;
}


void
poll_dtor (struct poll_data *pdel)
{
    if ( pdel->question )
        free_string(pdel->question);
    if ( pdel->author )
        free_string(pdel->author);

    if ( pdel->options )
    {
        int x = pdel->numOptions;

        while ( x-- > 0 )
            free_string(pdel->options[x].text);
        free_mem(pdel->options,
                 pdel->numOptions * sizeof(struct poll_opt_data));
    }

    if ( pdel == poll_list )
        poll_list = poll_list->next;
    else
    {
        struct poll_data *tmp = poll_list;

        while ( tmp && tmp->next != pdel )
            tmp = tmp->next;
        if ( tmp )
            tmp->next = pdel->next;
    }

    free_mem(pdel, sizeof(struct poll_data));
}


/*
 * Database Routines
 *
 * Store and recover polls from the poll database.  Polls are stored in a
 * simple text file format for quick loading.  It's not very human-readable
 * but they're not meant to be hand-edited.
 */

void
load_polls (void)
{
    struct poll_data *pnew = NULL;
    FILE *fp;
    int x;

    if ( !(fp = fopen(POLL_FILE, "r")) )
    {
        logerr(POLL_FILE);
        return;
    }

    while ( !feof(fp) )
    {
        pnew = poll_ctor();
        fread_to_eol(fp);
        pnew->question = fread_string(fp);
        pnew->author = fread_string(fp);
        pnew->voters = fread_number(fp);
        pnew->timestamp = fread_number(fp);
        pnew->numOptions = fread_number(fp);

        pnew->options =
            alloc_mem(pnew->numOptions * sizeof(struct poll_opt_data));
        for ( x = 0; x < pnew->numOptions; x++ )
        {
            pnew->options[x].tally = fread_number(fp);
            pnew->options[x].text = fread_string(fp);
        }

        if ( fread_number(fp) )
            poll_active = pnew;

        pnew->next = poll_list;
        poll_list = pnew;
    }

    fclose(fp);
}


void
save_polls (void)
{
    struct poll_data *piter;
    FILE *fp;
    int x;

    if ( !(fp = fopen(POLL_FILE, "w")) )
    {
        logerr(POLL_FILE);
        return;
    }

    for ( piter = poll_list; piter; piter = piter->next )
    {
        fprintf(fp, "\n%s~\n%s~\n%d %lu %d\n",
                piter->question, piter->author, piter->voters,
                piter->timestamp, piter->numOptions);
        for ( x = 0; x < piter->numOptions; x++ )
            fprintf(fp, "%d %s~\n", piter->options[x].tally,
                    piter->options[x].text);
        fprintf(fp, "%d", poll_active == piter);
    }

    fclose(fp);
}


/*
 * Displaying a Poll
 *
 * We'll display a poll.  If it's active, we'll only show the statistics for
 * it if 'ch' has already voted on it.
 */

void
display_poll (struct char_data *ch, struct poll_data *poll, bool show)
{
    char buf[MAX_STRING_LENGTH];
    char timestr[16];
    int x, k;

    strftime(timestr, 16, "%d %b %Y", localtime(&poll->timestamp));
    k = sprintf(buf,
                "&m%s&n\r\n"
                "&cAuthor&X: &n%s\r\n"
                "&cDate  &X: &n%s\r\n"
                "&cStatus&X: &n%s\r\n",
                poll->question, poll->author, timestr,
                (poll_active == poll
                 ? (ch->pcdata->voted_on != poll->timestamp
                    ? "Active.  You may vote on this poll!"
                    : "Active.  You have already voted on this poll.")
                 : "Inactive.  This poll is archived for review only."));

    if (!show && poll_active == poll &&
        ch->pcdata->voted_on != poll->timestamp)
    {
        k += sprintf(buf + k, "\r\n");
        for ( x = 0; x < poll->numOptions; x++ )
            k += sprintf(buf + k, "%d&X. &W%s&n\r\n", x + 1,
                         poll->options[x].text);
        k += sprintf(buf + k,
                     "\r\nUse &cPOLL VOTE #&n to vote for option number #.\r\n");
    }
    else
    {
        const int MAX_PAD = 61;
        int longest = 32;
        int sl = 0;

        for ( x = 0; sl < MAX_PAD && x < poll->numOptions; x++ )
        {
            sl = strlen(poll->options[x].text) + 1;
            if ( sl > longest )
                longest = UMIN(MAX_PAD, sl);
        }

        k += sprintf(buf + k, "&cTotal Participants&X: &n%d\r\n\r\n",
                     poll->voters);
        for ( x = 0; x < poll->numOptions; x++ )
            k += sprintf(buf + k, "&W%s%*c&n%3d%% &X(&n%d votes&X)&n\r\n",
                         poll->options[x].text,
                         longest - (int)strlen(poll->options[x].text), ' ',
                         (poll->options[x].tally * 100) / poll->voters,
                         poll->options[x].tally);
    }

    send_to_char(buf, ch);
}


/*
 * User Interface
 *
 * Here are the commands to actually vote.  The command is 'poll' and it has
 * several subcommands:
 *
 *   POLL                     Show the active poll.
 *   POLL SHOW                Show the active poll.
 *   POLL VOTE x              Vote for poll option number X.
 *   POLL LIST                List all previous polls.
 *   POLL SHOW x              Show poll number X.
 *   POLL NEW "x" "o1"..."oN" Make a new poll, X, with options o1...oN. (Adm)
 *   POLL DEL x               Delete poll number X. (Adm)
 *   POLL ACTIVE x            Set poll number X to active. (Adm)
 */

void
do_poll (struct char_data *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    struct poll_data *psel;
    int x = -1;

    if ( IS_NPC(ch) )
    {
        send_to_char("Fuck off.\r\n", ch);
        return;
    }

    argument = one_argument(argument, arg);

    if ( !*arg || !str_prefix(arg, "show") )
    {
        one_argument(argument, arg);

        if ( !*arg )
        {
            x = 666;
            psel = poll_active;
        }
        else if ( !isdigit(*arg) )
        {
            send_to_char("That's not the number of a poll.\r\n", ch);
            return;
        }
        else
        {
            x = atoi(arg) - 1;
            for ( psel = poll_list; psel && x > 0; psel = psel->next, x-- ) ;
        }

        if ( !psel || x < 0 )
        {
            printf_to_char(ch, "There's no %s.\r\n",
                           (!*arg ? "active poll" :
                            "poll by that number"));
            return;
        }

        display_poll(ch, psel, FALSE);
        return;
    }

    if ( !str_prefix(arg, "check") )
    {
        if ( poll_active && poll_active->timestamp > ch->pcdata->voted_on )
            send_to_char
                ("There's a new poll up!  Type &WPOLL&n to see it.\r\n",
                 ch);
        return;
    }

    if ( !str_prefix(arg, "vote") )
    {
        if ( IS_SET(ch->act, PLR_NOVOTE) )
        {
            send_to_char("You're not allowed to vote.\r\n", ch);
            return;
        }
        else if ( RANK(ch) != RANK_JOSH && RANK(ch) < RANK_NOVICE )
        {
            send_to_char("You must be ranked at least a novice to vote.\r\n", ch);
            return;
        }

        one_argument(argument, arg);
        x = atoi(arg);

        if ( !poll_active )
        {
            send_to_char("There's no active poll to vote on.\r\n", ch);
            return;
        }
        else if ( poll_active->timestamp == ch->pcdata->voted_on )
        {
            send_to_char("You've already voted on that poll!\r\n", ch);
            return;
        }
        else if ( x <= 0 || x > poll_active->numOptions )
        {
            send_to_char("That's not an option on the active poll.\r\n",
                         ch);
            return;
        }

        poll_active->options[x - 1].tally++;
        poll_active->voters++;
        ch->pcdata->voted_on = poll_active->timestamp;
        do_poll(ch, "show");
        logmesg("POLL: %s voted on '%s'.", ch->names,
                   poll_active->question);
        return;
    }

    if ( !str_prefix(arg, "new") && ch->trust >= 10 )
    {                           /* FIXME: Hard-coded trust */
        char tmpb[MAX_INPUT_LENGTH];
        char *tmp;

        argument = one_argument_pcase(argument, arg);
        tmp = argument;
        x = 0;

        if ( !*arg )
        {
            send_to_char
                ("Usage: poll new \"topic\" \"option1\" ... \"optionN\"\r\n"
                 "Note : The quotes are REQUIRED.\r\n", ch);
            return;
        }

        while ( *tmp )
        {
            tmp = one_argument(tmp, tmpb);
            x++;
        }

        if ( x < 2 )
        {
            send_to_char
                ("Usage: poll new\"topic\" \"option1\" ... \"optionN\"\r\n"
                 "Note : The quotes are REQUIRED.\r\n"
                 "&RError&X: &nYou need at least two options!\r\n", ch);
            return;
        }

        psel = poll_ctor();
        psel->question = str_dup(arg);
        psel->numOptions = x;
        psel->options = alloc_mem(x * sizeof(struct poll_opt_data));

        for ( x = 0; x < psel->numOptions; x++ )
        {
            argument = one_argument_pcase(argument, arg);
            psel->options[x].text = str_dup(arg);
            psel->options[x].tally = 0;
        }

        psel->voters = 0;
        psel->timestamp = time(0);
        psel->author = str_dup(ch->names);

        psel->next = poll_list;
        poll_list = psel;
        poll_active = psel;

        send_to_char("Ok.\r\n", ch);
        do_gecho(NULL, "There's a new poll!  Type &cPOLL&n to see it!");
        save_polls();
        return;
    }

    if ( !str_prefix(arg, "delete") && ch->trust >= 10 )
    {                           /* FIXME: Hard-coded trust. */
        one_argument(argument, arg);

        if ( !*arg || !isdigit(*arg) )
        {
            send_to_char("Usage: poll delete <x>\r\n", ch);
            return;
        }

        x = atoi(arg) - 1;
        for ( psel = poll_list; psel && x > 0; psel = psel->next, x-- ) ;

        if ( !psel || x < 0 )
        {
            send_to_char("That's not a valid poll number.\r\n", ch);
            return;
        }

        if ( poll_active == psel )
        {
            poll_active = NULL;
            send_to_char("Deactivated and deleted.\r\n", ch);
        }
        else
            send_to_char("Deleted.\r\n", ch);

        poll_dtor(psel);
        save_polls();
        return;
    }

    if ( !str_prefix(arg, "active") && ch->trust >= 10 )
    {                           /* FIXME: Hard-coded trust. */
        one_argument(argument, arg);

        if ( !*arg || !isdigit(*arg) )
        {
            send_to_char("Usage: poll active <x>\r\n", ch);
            return;
        }

        x = atoi(arg) - 1;
        for ( psel = poll_list; psel && x > 0; psel = psel->next, x-- ) ;

        if ( !psel )
        {
            send_to_char("That's not a valid poll number.\r\n", ch);
            return;
        }
        else if ( x == -1 )
        {
            send_to_char("Ok.  There's now no active poll.\r\n", ch);
            poll_active = NULL;
            return;
        }

        poll_active = psel;
        send_to_char("Ok.\r\n", ch);
        save_polls();
        return;
    }

    if ( !str_prefix(arg, "list") )
    {
        struct poll_data *piter;

        for ( x = 1, piter = poll_list; piter; piter = piter->next, x++ )
        {
            strftime(arg, MAX_INPUT_LENGTH, "%d %b %Y",
                     localtime(&piter->timestamp));
            printf_to_char(ch, "&X[&c%2d&X / &m%s&X] &n%s%s&n\r\n", x, arg,
                           piter->question,
                           (poll_active == piter ? "     &W[&RN&W]" : ""));
        }

        send_to_char
            ("\r\nUse &cPOLL SHOW #&n to review a poll, where # is a poll's number.\r\n",
             ch);
        return;
    }

    send_to_char("Usage: poll [subcommand [args...]]\r\n"
                 "Subcommands:\r\n"
                 "    POLL                  With no arguments, show active poll.\r\n"
                 "    POLL SHOW             With no arguments, show active poll.\r\n"
                 "    POLL VOTE x           Vote for option number X on active poll.\r\n"
                 "    POLL SHOW x           Show poll number X.\r\n"
                 "    POLL LIST             List all polls.\r\n", ch);

    if ( ch->trust >= 10 )
    {                           /* FIXME: Hard-coded trust. */
        send_to_char
            ("    POLL NEW \"x\" \"o\"...   Make a new poll, X, with options o...\r\n"
             "    POLL DEL x            Delete poll number X.\r\n"
             "    POLL ACTIVE x         Set poll number X to active.\r\n"
             "Note:\r\n"
             "  Re-activating an old poll is not recommended unless you're certain\r\n"
             "  no-one who voted for it has voted for a different poll since then!\r\n"
             "  Failure to ensure this can allow certain people to double-vote!\r\n",
             ch);
    }
}
