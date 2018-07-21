
/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  Ground ZERO improvements copyright pending 1994, 1995 by James Hilke   *
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

#include "ground0.h"
#include "interp.h"
#include "mccp.h"
#include "db.h"


/*
 * A SHOCKINGLY UGLY GLOBAL VARIABLE FOR '!' PARSING.
 * This is really, really awful.  Instead of changing the do_xxx functions to
 * take an extra argument to specify '!' parsing, I've added this global to
 * indicate the line was terminated with a '!', which indicates that the
 * action should be forced (to, for instance, target teammates for kills or
 * drive a tank through a wall).
 */
bool g_forceAction = FALSE;

bool check_social(CHAR_DATA * ch, char *command, char *argument);
char *command_argument(char *, char *);

/*
 * Command logging types.
 */
#define LOG_NORMAL  0
#define LOG_ALWAYS  1
#define LOG_NEVER   2


DECLARE_DO_FUN (do_suicide_message);
DECLARE_DO_FUN (do_gamestats);
DECLARE_DO_FUN (do_outfit);
DECLARE_DO_FUN (do_unpull);
DECLARE_DO_FUN (do_prepare);
DECLARE_DO_FUN (do_setscenario);
DECLARE_DO_FUN (do_clear);
DECLARE_DO_FUN (do_lines);
DECLARE_DO_FUN (do_listenin);
DECLARE_DO_FUN (do_repeat);
DECLARE_DO_FUN (do_novote);
DECLARE_DO_FUN (do_credits);
DECLARE_DO_FUN (do_demosocial);
DECLARE_DO_FUN (do_rules);
DECLARE_DO_FUN (do_changes);
DECLARE_DO_FUN (do_profile);
DECLARE_DO_FUN (do_wield);
DECLARE_DO_FUN (do_secondary);
DECLARE_DO_FUN (do_primary);
DECLARE_DO_FUN (do_as);
DECLARE_DO_FUN (do_roll);
DECLARE_DO_FUN (do_zombies);
DECLARE_DO_FUN (do_statfreeze);
DECLARE_DO_FUN (do_start);
DECLARE_DO_FUN (do_stop);
DECLARE_DO_FUN (do_detach);
DECLARE_DO_FUN (do_attach);
DECLARE_DO_FUN (do_history);
DECLARE_DO_FUN (do_beeps);
DECLARE_DO_FUN (do_makechar);
DECLARE_DO_FUN (do_ttype);
DECLARE_DO_FUN (do_palette);
DECLARE_DO_FUN (do_social);
DECLARE_DO_FUN (do_topology);
DECLARE_DO_FUN (do_compact);
DECLARE_DO_FUN (do_apropos);
DECLARE_DO_FUN (do_dub);
DECLARE_DO_FUN (do_whack);
DECLARE_DO_FUN (do_ansi);
DECLARE_DO_FUN (do_teamwho);

DECLARE_DO_FUN (do_noskulls);
DECLARE_DO_FUN (do_autoscan);
DECLARE_DO_FUN (do_nosocials);

DECLARE_DO_FUN (do_unban);
DECLARE_DO_FUN (do_mortal_ban);
DECLARE_DO_FUN (do_colorset);

DECLARE_DO_FUN (do_fire);
DECLARE_DO_FUN (do_turn);

#if 0
/* Mech stuff -- need to incorporate. */
DECLARE_DO_FUN (do_drive);
DECLARE_DO_FUN (do_charge);
DECLARE_DO_FUN (do_jump);
#endif

/*
 * Log-all switch.
 */
bool fLogAll = FALSE;
void do_rand_error(CHAR_DATA * ch);
void log_base_player(CHAR_DATA * ch, char *str);


const struct wiznet_type wiznet_table[] = {
    {"on", WIZ_ON, IM},
    {"prefix", WIZ_PREFIX, IM},
    {"logins", WIZ_LOGINS, IM},
    {"sites", WIZ_SITES, L4},
    {"secure", WIZ_SECURE, ML},
    {"badpop", WIZ_BADPOP, L2},
    {"bans", WIZ_BANS, L7},
    {"logs", WIZ_PLOG, L1},
    {NULL, 0, 0}
};


#define MAX_ERRORS    1

const char *error_table[MAX_ERRORS] = {
    "Huh?!"
};




/*
 * Command table.
 */
const struct cmd_type cmd_table[] = {
    /*
     * Common movement commands.
     */
    {"north", do_north, POS_STANDING, 0, LOG_NEVER, 1, 1, 0, 1},
    {"east", do_east, POS_STANDING, 0, LOG_NEVER, 1, 1, 0, 1},
    {"south", do_south, POS_STANDING, 0, LOG_NEVER, 1, 1, 0, 1},
    {"west", do_west, POS_STANDING, 0, LOG_NEVER, 1, 1, 0, 1},
    {"up", do_up, POS_STANDING, 0, LOG_NEVER, 1, 1, 0, 1},
    {"down", do_down, POS_STANDING, 0, LOG_NEVER, 1, 1, 0, 1},

    /*
     * Common other commands.
     * Placed here so one and two letter abbreviations work.
     */
    {"load", do_load, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"at", do_at, POS_DEAD, L5, LOG_NORMAL, 1, 0, 0, 1},
    {"pull", do_pull, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"light", do_pull, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"toss", do_toss, POS_STANDING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"throw", do_toss, POS_STANDING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"donate", do_donate, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"enter", do_enter, POS_STANDING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"exits", do_exits, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"get", do_get, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"goto", do_goto, POS_DEAD, L8, LOG_NORMAL, 1, 0, 0, 1},
    {"inventory", do_inventory, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"kill", do_kill, POS_FIGHTING, 0, LOG_NORMAL, 1, 1, 0, 1},
    {"shoot", do_kill, POS_FIGHTING, 0, LOG_NORMAL, 1, 1, 0, 1},
    {"look", do_look, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 0},
    {"leave", do_leave, POS_STANDING, 0, LOG_NORMAL, 1, 0, 0, 0},
    {"tell", do_tell, POS_RESTING, 0, LOG_NORMAL, 1, 0, 1, 1},
    {"teleport", do_teleport, POS_STANDING, 0, LOG_NORMAL, 1, 1, 0, 1},
    {"push", do_push, POS_STANDING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"roll", do_roll, POS_STANDING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"follow", do_follow, POS_STANDING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"commbadge", do_commbadge, POS_SLEEPING, 0, LOG_NORMAL, 1, 0, 1, 1},
    {"bury", do_bury, POS_STANDING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"rest", do_rest, POS_SLEEPING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"sit", do_sit, POS_SLEEPING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"sockets", do_sockets, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"stand", do_stand, POS_SLEEPING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"wield", do_wield, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"secondary", do_secondary, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"primary", do_primary, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"wizhelp", do_wizhelp, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"commands", do_commands, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"use", do_use, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 0},
    {"wear", do_wear, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"primary", do_wear, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"second", do_wear, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"depress", do_depress, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"team", do_team, POS_DEAD, L1, LOG_NORMAL, 1, 0, 0, 1},
    {"teamtalk", do_teamtalk, POS_DEAD, 0, LOG_NORMAL, 1, 0, 1, 1},
    {"/", do_teamtalk, POS_DEAD, 0, LOG_NORMAL, 1, 0, 1, 1},
    {"stat", do_stat, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"apropos", do_apropos, POS_DEAD, 0, LOG_NEVER, 1, 0, 0, 1},
    {"fire", do_fire, POS_DEAD, 0, LOG_NEVER, 1, 0, 0, 1},
    {"turn", do_turn, POS_DEAD, 0, LOG_NEVER, 1, 0, 0, 1},

    /*
     * Informational commands.
     */
    {"count", do_count, POS_SLEEPING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"credits", do_credits, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"equipment", do_equipment, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"gear", do_equipment, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"examine", do_look, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"help", do_help, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"idea", do_idea, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"motd", do_motd, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"report", do_report, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"scan", do_scan, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"score", do_score, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"socials", do_socials, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"time", do_time, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"typo", do_typo, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"who", do_who, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"whois", do_whois, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"gamestats", do_gamestats, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"wizlist", do_wizlist, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"credits", do_credits, POS_DEAD, 0, LOG_NEVER, 1, 0, 0, 1},
    {"rules", do_rules, POS_DEAD, 0, LOG_NEVER, 1, 0, 0, 1},
    {"changes", do_changes, POS_DEAD, 0, LOG_NEVER, 1, 0, 0, 1},
    {"demosocial", do_demosocial, POS_DEAD, 0, LOG_NEVER, 1, 0, 0, 1},
    {"profile", do_profile, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"history", do_history, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"twho", do_teamwho, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"teamwho", do_teamwho, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},

    /*
     * Configuration commands.
     */
    {"delet", do_delet, POS_DEAD, 0, LOG_NORMAL, 0, 0, 0, 1},
    {"delete", do_delete, POS_DEAD, 0, LOG_ALWAYS, 1, 0, 0, 1},
    {"passwor", do_passwor, POS_DEAD, 0, LOG_NEVER, 1, 0, 0, 1},
    {"password", do_password, POS_DEAD, 0, LOG_NEVER, 1, 0, 0, 1},
    {"title", do_title, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"rank", do_rank, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"kmsg", do_kill_message, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"smsg", do_suicide_message, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"compress", do_compress, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"clear", do_clear, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"cls", do_clear, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"lines", do_lines, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"beeps", do_beeps, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"ttype", do_ttype, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"palette", do_palette, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"compact", do_compact, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"colorset", do_colorset, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"ansi", do_ansi, POS_DEAD, 0, LOG_NORMAL, 0, 0, 0, 1},
    {"noskulls", do_noskulls, POS_DEAD, 0, LOG_NORMAL, 0, 0, 0, 1},
    {"nosocials", do_nosocials, POS_DEAD, 0, LOG_NORMAL, 0, 0, 0, 1},
    {"autoscan", do_autoscan, POS_DEAD, 0, LOG_NORMAL, 0, 0, 0, 1},

    /*
     * Communication commands.
     */
    {"emote", do_emote, POS_RESTING, 0, LOG_NORMAL, 1, 0, 1, 1},
    {",", do_emote, POS_RESTING, 0, LOG_NORMAL, 1, 0, 1, 1},
    {"note", do_note, POS_SLEEPING, 0, LOG_NORMAL, 1, 0, 1, 1},
    {"quiet", do_quiet, POS_SLEEPING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"reply", do_reply, POS_RESTING, 0, LOG_NORMAL, 1, 0, 1, 1},
    {";", do_reply, POS_RESTING, 0, LOG_NORMAL, 1, 0, 1, 1},
    {"say", do_say, POS_RESTING, 0, LOG_NORMAL, 1, 0, 1, 1},
    {"'", do_say, POS_RESTING, 0, LOG_NORMAL, 1, 0, 1, 1},
    {"gocial", do_gocial, POS_RESTING, 0, LOG_NORMAL, 1, 0, 1, 1},
    {"@", do_social, POS_DEAD, 0, LOG_NORMAL, 1, 0, 1, 1},
    {"gemote", do_gemote, POS_DEAD, 0, LOG_NORMAL, 1, 0, 1, 1},

    /*
     * Object manipulation commands.
     */
    {"drop", do_drop, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"give", do_give, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"hold", do_wear, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"remove", do_remove, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"take", do_get, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"unload", do_unload, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"use", do_use, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"wear", do_wear, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"repin", do_unpull, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"deactive", do_unpull, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"unpull", do_unpull, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"activate", do_pull, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"detach", do_detach, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"attach", do_attach, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"combine",do_combine, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},

    /*
     * Vehicle commands.
     */
    {"start", do_start, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"stop", do_stop, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"man", do_man, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},

    /*
     * Miscellaneous commands.
     */
    {"quit", do_quit, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"repop", do_repop, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"save", do_save, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"sleep", do_sleep, POS_SLEEPING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"wake", do_wake, POS_SLEEPING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"where", do_where, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 0},
    {"track", do_track, POS_RESTING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"resig", do_resig, POS_SLEEPING, 0, LOG_NORMAL, 0, 0, 0, 1},
    {"resign", do_resign, POS_SLEEPING, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"induct", do_induct, POS_DEAD, 0, LOG_ALWAYS, 1, 0, 0, 1},
    {"volunteer", do_volunteer, POS_DEAD, 0, LOG_ALWAYS, 1, 0, 0, 1},
    {"showvol", do_showvol, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"boom", do_boom, POS_SLEEPING, 0, LOG_NORMAL, 0, 0, 0, 1},
    {"explode", do_explode, POS_SLEEPING, 0, LOG_NORMAL, 0, 0, 0, 1},
    {"boo", do_boo, POS_DEAD, 0, LOG_NORMAL, 0, 0, 0, 1},
    {"boot", do_boot, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"poll", do_poll, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},


    /*
     * Immortal commands.
     */
    {"addlag", do_addlag, POS_DEAD, ML, LOG_ALWAYS, 0, 0, 0, 1},
    {"dump", do_dump, POS_DEAD, ML, LOG_ALWAYS, 1, 0, 0, 1},
    {"badpop", do_badpop, POS_DEAD, ML, LOG_ALWAYS, 1, 0, 0, 1},
    {"basepurge", do_basepurge, POS_DEAD, ML, LOG_ALWAYS, 1, 0, 0, 1},
    {"bringon", do_bringon, POS_DEAD, L1, LOG_ALWAYS, 1, 0, 0, 1},
    {"trust", do_trust, POS_DEAD, ML, LOG_ALWAYS, 1, 0, 0, 1},
    {"ban", do_ban, POS_DEAD, L5, LOG_ALWAYS, 1, 0, 0, 1},
    {"ban", do_mortal_ban, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},   /* "ban list" */
    {"unban", do_unban, POS_DEAD, L5, LOG_ALWAYS, 1, 0, 0, 1},     /* "ban $1 remove" */
    {"deny", do_deny, POS_DEAD, ML, LOG_ALWAYS, 1, 0, 0, 1},
    {"undeny", do_undeny, POS_DEAD, L3, LOG_ALWAYS, 1, 0, 0, 1},
    {"untraitor", do_untraitor, POS_DEAD, L7, LOG_ALWAYS, 1, 0, 0, 1},
    {"disconnect", do_disconnect, POS_DEAD, L1, LOG_ALWAYS, 1, 0, 0, 1},
    {"doas", do_as, POS_DEAD, L4, LOG_ALWAYS, 1, 0, 0, 1},
    {"freeze", do_freeze, POS_DEAD, L1, LOG_ALWAYS, 1, 0, 0, 1},
    {"reboo", do_shutdow, POS_DEAD, L6, LOG_NORMAL, 0, 0, 0, 1},
    {"reboot", do_reboot, POS_DEAD, L6, LOG_ALWAYS, 1, 0, 0, 1},
    {"sedit", do_sedit, POS_DEAD, L7, LOG_ALWAYS, 1, 0, 0, 1},
    {"set", do_set, POS_DEAD, L2, LOG_ALWAYS, 1, 0, 0, 1},
    {"shutdow", do_shutdow, POS_DEAD, L6, LOG_NORMAL, 0, 0, 0, 1},
    {"shutdown", do_shutdown, POS_DEAD, L6, LOG_ALWAYS, 1, 0, 0, 1},
    {"wizlock", do_wizlock, POS_DEAD, L2, LOG_ALWAYS, 1, 0, 0, 1},
    {"wiznet", do_wiznet, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"for", do_for, POS_DEAD, L2, LOG_ALWAYS, 1, 0, 0, 1},
    {"force", do_force, POS_DEAD, L3, LOG_ALWAYS, 1, 0, 0, 1},
    {"create", do_oload, POS_DEAD, L5, LOG_ALWAYS, 1, 0, 0, 1},
    {"newlock", do_newlock, POS_DEAD, L2, LOG_ALWAYS, 1, 0, 0, 1},
    {"nochannels", do_nochannels, POS_DEAD, L5, LOG_ALWAYS, 1, 0, 0, 1},
    {"noemote", do_noemote, POS_DEAD, L5, LOG_ALWAYS, 1, 0, 0, 1},
    {"noshout", do_noshout, POS_DEAD, L5, LOG_ALWAYS, 1, 0, 0, 1},
    {"notell", do_notell, POS_DEAD, L5, LOG_ALWAYS, 1, 0, 0, 1},
    {"pecho", do_pecho, POS_DEAD, L3, LOG_ALWAYS, 1, 0, 0, 1},
    {"purge", do_purge, POS_DEAD, L5, LOG_ALWAYS, 1, 0, 0, 1},
    {"destroy", do_destroy, POS_DEAD, L4, LOG_ALWAYS, 1, 0, 0, 1},
    {"restore", do_restore, POS_DEAD, L3, LOG_ALWAYS, 1, 0, 0, 1},
    {"slay", do_slay, POS_DEAD, L2, LOG_ALWAYS, 1, 0, 0, 1},
    {"transfer", do_transfer, POS_DEAD, L6, LOG_ALWAYS, 1, 0, 0, 1},
    {"poofin", do_bamfin, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"poofout", do_bamfout, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"ptell", do_ptell, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"gecho", do_gecho, POS_DEAD, L2, LOG_ALWAYS, 1, 0, 0, 1},
    {"grab", do_grab, POS_DEAD, L5, LOG_ALWAYS, 1, 0, 0, 1},
    {"lecho", do_lecho, POS_DEAD, L8, LOG_ALWAYS, 1, 0, 0, 1},
    {"holylight", do_holylight, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"invis", do_invis, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"log", do_log, POS_DEAD, L1, LOG_ALWAYS, 1, 0, 0, 1},
    {"memory", do_memory, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"peace", do_peace, POS_DEAD, L6, LOG_NORMAL, 1, 0, 0, 1},
    {"snoop", do_snoop, POS_DEAD, ML, LOG_ALWAYS, 1, 0, 0, 1},
    {"stat", do_stat, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"string", do_string, POS_DEAD, L6, LOG_ALWAYS, 1, 0, 0, 1},
    {"wizinvis", do_invis, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"vnum", do_vnum, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"clone", do_clone, POS_DEAD, L3, LOG_ALWAYS, 1, 0, 0, 1},
    {"imptalk", do_imptalk, POS_DEAD, ML, LOG_NORMAL, 1, 0, 0, 1},
    {"[", do_imptalk, POS_DEAD, ML, LOG_NORMAL, 1, 0, 0, 1},
    {"immtalk", do_immtalk, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"imotd", do_imotd, POS_DEAD, HE, LOG_NORMAL, 1, 0, 0, 1},
    {":", do_immtalk, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"ld", do_quit, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"nonote", do_nonote, POS_DEAD, L4, LOG_NORMAL, 1, 0, 0, 1},
    {"penalize", do_penalize, POS_DEAD, IM, LOG_ALWAYS, 0, 0, 0, 1},
    {"done", do_done, POS_DEAD, 0, LOG_ALWAYS, 0, 0, 0, 1},
    {"chars", do_characters, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"savehelps", do_savehelps, POS_DEAD, 10, LOG_ALWAYS, 0, 0, 0, 1},
    {"kills", do_kills, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},
    {"top", do_top, POS_DEAD, 0, LOG_NORMAL, 0, 0, 0, 1},
    {"rename", do_rename, POS_DEAD, ML, LOG_ALWAYS, 1, 0, 0, 1},
    {"noleader", do_noleader, POS_DEAD, L2, LOG_ALWAYS, 1, 0, 0, 1},
    {"tick", do_tick, POS_DEAD, ML, LOG_ALWAYS, 1, 0, 0, 1},
    {"teamstats", do_teamstats, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"expand", do_expand, POS_DEAD, ML, LOG_ALWAYS, 1, 1, 0, 1},
    {"ping", do_ping, POS_DEAD, ML, LOG_ALWAYS, 1, 0, 0, 1},
    {"outfit", do_outfit, POS_DEAD, L3, LOG_NORMAL, 1, 0, 0, 1},
    {"setscenario", do_setscenario, POS_DEAD, L1, LOG_ALWAYS, 1, 0, 0, 1},
    {"listen", do_listenin, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"*", do_repeat, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"novote", do_novote, POS_DEAD, L4, LOG_NORMAL, 1, 0, 0, 1},
    {"zombies", do_zombies, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"statfreeze", do_statfreeze, POS_DEAD, ML, LOG_ALWAYS, 1, 0, 0, 1},
    {"makechar", do_makechar, POS_DEAD, L3, LOG_NORMAL, 1, 0, 0, 1},
    {"topology", do_topology, POS_DEAD, IM, LOG_NORMAL, 1, 0, 0, 1},
    {"dub", do_dub, POS_DEAD, L7, LOG_ALWAYS, 1, 0, 0, 1},
    {"whack", do_whack, POS_DEAD, ML, LOG_ALWAYS, 1, 1, 0, 1},
//    {"testscreen", do_testscreen, POS_DEAD, ML, LOG_NORMAL, 1, 0, 0, 1},

    /* ... */
    {"forgive", do_forgive, POS_DEAD, 0, LOG_NORMAL, 1, 0, 0, 1},


    /*
     * End of list.
     */
    {"", 0, POS_DEAD, 0, LOG_NORMAL, 0, 0, 0, 0}
};

/*
 * The main entry point for executing commands.
 * Can be recursively called from 'at', 'order', 'force'.
 */
void
interpret (CHAR_DATA * ch, char *argument)
{
    char command[MAX_INPUT_LENGTH];
    char logline[MAX_INPUT_LENGTH];
    bool temp = FALSE;
    int cmd;
    int trust;
    bool found;

    if ( IS_IMMORTAL(ch) || ch->in_room->level != -1 ||
         ch->desc == NULL || !ch->desc->repeat )
        ch->idle = 0;

    if ( IS_SET(ch->temp_flags, TEMP_BLOCK_ED) )
    {
        if ( ch->pnote->text == NULL || (*argument == '~' && !*(argument + 1)) )
        {
            REMOVE_BIT(ch->temp_flags, TEMP_BLOCK_ED);
            send_to_char("Block added to message.\r\n", ch);
            return;
        }
        else if ( strlen(ch->pnote->text) + strlen(argument) >= MAX_STRING_LENGTH - MAX_INPUT_LENGTH )
        {
            send_to_char("Note too long.  Use &W~&n on a line by itself to end.\r\n", ch);
            return;
        }
        else
        {
            char buf[MAX_STRING_LENGTH];
            strcpy(buf, ch->pnote->text);
            strcat(buf, argument);
            strcat(buf, "\r\n");
            free_string(ch->pnote->text);
            ch->pnote->text = str_dup(buf);
            return;
        }
    }

    /* Strip leading spaces. */
    while ( isspace(*argument) )
        argument++;
    if ( argument[0] == '\0' )
        return;

    /*
     * Implement freeze command.
     */
    if ( !IS_NPC(ch) && IS_SET(ch->act, PLR_FREEZE) )
    {
        send_to_char("You're totally frozen!\n\r", ch);
        return;
    }

    /*
     * Grab the command word.
     * Special parsing so ' can be a command,
     *   also no spaces needed after punctuation.
     */
    strcpy(logline, argument);

    if ( !isalpha(argument[0]) && !isdigit(argument[0]) )
    {
        command[0] = argument[0];
        command[1] = '\0';
        argument++;
        while ( isspace(*argument) )
            argument++;
    }
    else
        argument = command_argument(argument, command);

    found = FALSE;
    trust = get_trust(ch);

    for ( cmd = 0; cmd_table[cmd].name[0] != '\0'; cmd++ )
    {
        if (command[0] == cmd_table[cmd].name[0]
            && !str_prefix(command, cmd_table[cmd].name)
            && cmd_table[cmd].level <= trust)
        {
            found = TRUE;
            break;
        }
    }

    /*
     * Log and snoop.
     */
    if ( cmd_table[cmd].log == LOG_NEVER )
        *logline = '\0';

    if ( *logline && ((!IS_NPC(ch) && IS_SET(ch->act, PLR_LOG) )
                     || fLogAll || cmd_table[cmd].log == LOG_ALWAYS))
    {

        sprintf(log_buf, "Log %s: %s", ch->names, logline);
        wiznet(log_buf, ch, NULL,
               (!IS_NPC(ch) &&
                IS_SET(ch->act, PLR_LOG) ? WIZ_PLOG : WIZ_SECURE), 0,
               get_trust(ch));

        if ( IS_SET(ch->act, PLR_LOG) )
            log_base_player(ch, logline);
        else
            log_base_string(log_buf);

        if ( ch == enforcer )
        {
            char buf[MAX_STRING_LENGTH];

            sprintf(buf, "%%%s", log_buf);
            do_immtalk(NULL, buf);
        }
    }

    if ( *logline && ch->desc != NULL && ch->desc->snoop_by != NULL )
    {
        write_to_buffer(ch->desc->snoop_by, "% ", 2);
        write_to_buffer(ch->desc->snoop_by, logline, 0);
        write_to_buffer(ch->desc->snoop_by, "\r\n", 2);
    }

    if ( !found )
    {
        /* Look for command in social table. */
        if ( !check_social(ch, command, argument) )
            printf_to_char(ch, "%s\r\n",
                           error_table[number_range(0, MAX_ERRORS - 1)]);
        return;
    }

    if ( ch->dubbing && cmd_table[cmd].dub )
        ch = ch->dubbing;

    /*
     * Character not in position for command?
     */
    if ( ch->position < cmd_table[cmd].position )
    {
        switch (ch->position)
        {
            case POS_DEAD:
                send_to_char("Lie still; you are DEAD.\n\r", ch);
                break;

            case POS_MORTAL:
            case POS_INCAP:
                send_to_char("You are hurt far too bad for that.\n\r", ch);
                break;

            case POS_STUNNED:
                send_to_char("You are too stunned to do that.\n\r", ch);
                break;

            case POS_SLEEPING:
                send_to_char("In your dreams, or what?\n\r", ch);
                break;

            case POS_RESTING:
                send_to_char("Nah... You feel too relaxed...\n\r", ch);
                break;

            case POS_SITTING:
                send_to_char("Better stand up first.\n\r", ch);
                break;

            case POS_FIGHTING:
                send_to_char("No way!  You are still fighting!\n\r", ch);
                break;

        }
        return;
    }

    temp = g_forceAction;
    g_forceAction = FALSE;

    if ( cmd_table[cmd].canForce && strlen(argument) > 0 )
    {
        char *last = argument + strlen(argument) - 1;

        if ( *last == '!' )
        {
            *last = '\0';
            g_forceAction = TRUE;
        }
    }

    if ( ch->desc && cmd_table[cmd].resetLagFlag )
        ch->desc->botLagFlag = 0;

    /*
     * Dispatch the command.
     */
    (*cmd_table[cmd].do_fun) (ch, argument);

    if ( ch->desc && ch->desc->showstr_point )
        show_string(ch->desc, "");

    g_forceAction = temp;
    tail_chain();
}


void
do_social (struct char_data *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];

    argument = one_argument(argument, arg);

    if ( !check_social(ch, arg, argument) )
        printf_to_char(ch, "%s\r\n",
                       error_table[number_range(0, MAX_ERRORS - 1)]);
}


bool
check_social (CHAR_DATA * ch, char *command, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    int cmd;
    bool found = FALSE;

    for ( cmd = 0; social_table[cmd].name; cmd++ )
    {
        if (command[0] == social_table[cmd].name[0]
            && !str_prefix(command, social_table[cmd].name))
        {
            found = TRUE;
            break;
        }
    }

    if ( !found )
        return FALSE;

    if ( !IS_NPC(ch) && IS_SET(ch->comm, COMM_NOEMOTE | COMM_NOSOCIALS) )
    {
        send_to_char("You are anti-social!\n\r", ch);
        return TRUE;
    }

    switch (ch->position)
    {
        case POS_DEAD:
            send_to_char("Lie still; you are DEAD.\n\r", ch);
            return TRUE;
        case POS_INCAP:
        case POS_MORTAL:
            send_to_char("You are hurt far too bad for that.\n\r", ch);
            return TRUE;
        case POS_STUNNED:
            send_to_char("You are too stunned to do that.\n\r", ch);
            return TRUE;
        case POS_SLEEPING:
            if ( !str_cmp(social_table[cmd].name, "snore") )
                break;
            send_to_char("In your dreams, or what?\n\r", ch);
            return TRUE;

    }

    if ( ch->dubbing )
        ch = ch->dubbing;

    one_argument(argument, arg);
    victim = NULL;
    if ( arg[0] == '\0' )
    {
        act(social_table[cmd].others_no_arg, ch, NULL, victim, TO_ROOM | TO_NOT_NOSOCIALS);
        act(social_table[cmd].char_no_arg, ch, NULL, victim, TO_CHAR | TO_NOT_NOSOCIALS);
    }
    else if ( (victim = get_char_room(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\n\r", ch);
    }
    else if ( victim == ch )
    {
        act(social_table[cmd].others_auto, ch, NULL, victim, TO_ROOM | TO_NOT_NOSOCIALS);
        act(social_table[cmd].char_auto, ch, NULL, victim, TO_CHAR | TO_NOT_NOSOCIALS);
    }
    else
    {
        act(social_table[cmd].others_found, ch, NULL, victim, TO_NOTVICT | TO_NOT_NOSOCIALS);
        act(social_table[cmd].char_found, ch, NULL, victim, TO_CHAR | TO_NOT_NOSOCIALS);
        act(social_table[cmd].vict_found, ch, NULL, victim, TO_VICT | TO_NOT_NOSOCIALS);

        if ( !IS_NPC(ch) && IS_NPC(victim )
            && IS_AWAKE(victim) && victim->desc == NULL)
        {

            if ( number_range(0, 12) < 9 )
            {
                act(social_table[cmd].others_found, victim, NULL, ch,
                    TO_NOTVICT | TO_NOT_NOSOCIALS);
                act(social_table[cmd].char_found, victim, NULL, ch,
                    TO_CHAR | TO_NOT_NOSOCIALS);
                act(social_table[cmd].vict_found, victim, NULL, ch,
                    TO_VICT | TO_NOT_NOSOCIALS);
            }
            else
            {
                act("$n slaps $N.", victim, NULL, ch, TO_NOTVICT | TO_NOT_NOSOCIALS);
                act("You slap $N.", victim, NULL, ch, TO_CHAR | TO_NOT_NOSOCIALS);
                act("$n slaps you.", victim, NULL, ch, TO_VICT | TO_NOT_NOSOCIALS);
            }
        }
    }

    return (TRUE);
}

/*
 * Return true if an argument is completely numeric.
 */
bool
is_number (char *arg)
{

    if ( *arg == '\0' )
        return FALSE;

    if ( *arg == '+' || *arg == '-' )
        arg++;

    for ( ; *arg != '\0'; arg++ )
    {
        if ( !isdigit(*arg) )
            return FALSE;
    }

    return TRUE;
}



/*
 * Given a string like 14.foo, return 14 and 'foo'
 */
int
number_argument (char *argument, char *arg)
{
    char *pdot;
    int number;

    for ( pdot = argument; *pdot != '\0'; pdot++ )
    {
        if ( *pdot == '.' )
        {
            *pdot = '\0';
            number = atoi(argument);
            *pdot = '.';
            strcpy(arg, pdot + 1);
            return number;
        }
    }

    strcpy(arg, argument);
    return 1;
}



/*
 * Pick off one argument from a string and return the rest.
 * Understands quotes.
 */
int
mult_argument (char *argument, char *arg)
{
    char *pdot;
    int number;

    for ( pdot = argument; *pdot != '\0'; pdot++ )
    {
        if ( *pdot == '*' )
        {
            *pdot = '\0';
            number = atoi(argument);
            *pdot = '*';
            strcpy(arg, pdot + 1);
            return number;
        }
    }

    strcpy(arg, argument);
    return 1;
}


char *
one_word (char *argument, char *arg_first)
{
    while ( isspace(*argument) )
        argument++;

    while ( !isspace(*argument) && *argument != '\0' )
        *(arg_first++) = *(argument++);

    *arg_first = '\0';
    while ( isspace(*argument) )
        argument++;
    return argument;
}


char *
one_argument (char *argument, char *arg_first)
{
    char cEnd;

    while ( isspace(*argument) )
        argument++;

    cEnd = ' ';
    if ( *argument == '\'' || *argument == '"' )
        cEnd = *argument++;

    while ( *argument != '\0' )
    {
        if ( *argument == cEnd || (cEnd == ' ' && isspace(*argument)) )
        {
            argument++;
            break;
        }
        *arg_first = LOWER(*argument);
        arg_first++;
        argument++;
    }
    *arg_first = '\0';

    while ( isspace(*argument) )
        argument++;

    return argument;
}


int
count_arguments (const char *input)
{
    int count = 0;
    char cEnd;

    while ( isspace(*input) )
        input++;

    while ( *input )
    {
        if ( *input == '\'' || *input == '"' )
            cEnd = *input;
        else
            cEnd = ' ';

        while (*input && *input != cEnd &&
               (cEnd != ' ' || !isspace(*input)))
            input++;

        count++;
        while ( isspace(*input) )
            input++;
    }

    return count;
}


char *
command_argument (char *argument, char *arg_first)
{
    while ( isspace(*argument) )
        argument++;

    while ( *argument && !isspace(*argument) && *argument != '!' )
    {
        *(arg_first++) = LOWER(*argument);
        argument++;
    }

    *arg_first = '\0';

    while ( isspace(*argument) )
        argument++;

    return (argument);
}


/* one_argument() except we preserve case. */
char *
one_argument_pcase (char *argument, char *arg_first)
{
    char cEnd;

    while ( isspace(*argument) )
        argument++;

    cEnd = ' ';
    if ( *argument == '\'' || *argument == '"' )
        cEnd = *argument++;

    while ( *argument != '\0' )
    {
        if ( *argument == cEnd || (cEnd == ' ' && isspace(*argument)) )
        {
            argument++;
            break;
        }
        *arg_first = *argument;
        arg_first++;
        argument++;
    }
    *arg_first = '\0';

    while ( isspace(*argument) )
        argument++;

    return argument;
}


void
do_apropos (struct char_data *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];

    one_argument(argument, arg);

    if ( !*arg )
    {
        send_to_char("Usage: apropos <keyword>\r\n", ch);
        return;
    }

    char buf[MAX_STRING_LENGTH];
    int count = 0;
    int k = 0;

    extern int g_noCmdSynopses;
    extern struct cmd_synopses_data *g_cmdSynopses;

    for ( int cmd = 0; cmd < g_noCmdSynopses; cmd++ )
    {
        if ( !str_prefix(arg, g_cmdSynopses[cmd].name ) ||
            strstr(without_colors(g_cmdSynopses[cmd].description), arg))
        {
            k += sprintf(buf + k, "&c%s&X:&n %s\r\n",
                         g_cmdSynopses[cmd].name,
                         g_cmdSynopses[cmd].description);

            if ( k >= MAX_STRING_LENGTH - 128 )
            {
                page_to_char(buf, ch);
                *buf = '\0';
                k = 0;
            }

            count++;
        }
    }

    if ( !count )
    {
        send_to_char("No matches found.\r\n", ch);
        return;
    }

    sprintf(buf + k, "\r\n%d matches found.\r\n", count);
    page_to_char(buf, ch);
}


void
do_commands (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    int col = 0;
    int k = 0;
    int cmd;

    extern int g_noCmdSynopses;
    extern struct cmd_synopses_data *g_cmdSynopses;

    one_argument(argument, arg);
    *buf = '\0';

    if ( !str_cmp(arg, "-table") )
    {
        for ( cmd = 0; cmd_table[cmd].name[0] != '\0'; cmd++ )
            if ( cmd_table[cmd].level <= get_trust(ch ) &&
                cmd_table[cmd].show)
            {
                k += sprintf(buf + k, "%-12s%s", cmd_table[cmd].name,
                             (!(++col % 6) ? "\r\n" : ""));

                if ( k >= MAX_STRING_LENGTH - 128 )
                {
                    page_to_char(buf, ch);
                    *buf = '\0';
                    k = 0;
                }
            }

        if ( (col % 6) )
            strcat(buf, "\r\n");

        page_to_char(buf, ch);
        return;
    }
    else if ( !str_cmp(arg, "-count") )
    {
        for ( cmd = 0; *cmd_table[cmd].name; cmd++ )
            if ( cmd_table[cmd].level <= get_trust(ch ) &&
                cmd_table[cmd].show)
                col++;

        printf_to_char(ch, "Counted %d command%s.", col,
                       (col == 1 ? "" : "s"));
        return;
    }

    for ( cmd = 0; cmd < g_noCmdSynopses; cmd++ )
        if ( !*arg || !str_prefix(arg, g_cmdSynopses[cmd].name) )
        {
            k += sprintf(buf + k, "&c%s&X:&n %s\r\n",
                         g_cmdSynopses[cmd].name,
                         g_cmdSynopses[cmd].description);

            if ( k >= MAX_STRING_LENGTH - 128 )
            {
                page_to_char(buf, ch);
                *buf = '\0';
                k = 0;
            }

            col++;
        }

    sprintf(buf + k, "\r\n%d command%s matched.", col,
            (col == 1 ? "" : "s"));
    page_to_char(buf, ch);
}


void
do_wizhelp (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    int cmd;
    int col;

    col = 0;
    for ( cmd = 0; cmd_table[cmd].name[0] != '\0'; cmd++ )
    {
        if ( cmd_table[cmd].level <= get_trust(ch) && cmd_table[cmd].level )
        {
            sprintf(buf, "%-12s", cmd_table[cmd].name);
            send_to_char(buf, ch);
            if ( ++col % 6 == 0 )
                send_to_char("\n\r", ch);
        }
    }

    if ( col % 6 != 0 )
        send_to_char("\n\r", ch);
}
