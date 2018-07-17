
 /* this is a listing of all the commands and command related data */

/* for command types */
#define ML  MAX_TRUST           /* implementor */
#define L1  MAX_TRUST - 1       /* creator */
#define L2  MAX_TRUST - 2       /* supreme being */
#define L3  MAX_TRUST - 3       /* deity */
#define L4  MAX_TRUST - 4       /* god */
#define L5  MAX_TRUST - 5       /* immortal */
#define L6  MAX_TRUST - 6       /* demigod */
#define L7  MAX_TRUST - 7       /* angel */
#define L8  MAX_TRUST - 8       /* avatar */
#define IM  1                   /* angel */
#define HE  1                   /* hero */


/*
 * Structure for a command in the command lookup table.
 */
struct cmd_type
{
    char *const name;
    DO_FUN *do_fun;
    sh_int position;
    sh_int level;
    sh_int log;
    bool show;
    bool canForce;
    bool dub;
    bool resetLagFlag;
};

/* the command table itself */
extern const struct cmd_type cmd_table[];

/*
 * Command functions.
 * Defined in act_*.c (mostly).
 */
DECLARE_DO_FUN(do_addlag);
DECLARE_DO_FUN(do_allow);
DECLARE_DO_FUN(do_apass);
DECLARE_DO_FUN(do_at);
DECLARE_DO_FUN(do_autoexit);
DECLARE_DO_FUN(do_autoloot);
DECLARE_DO_FUN(do_autosac);
DECLARE_DO_FUN(do_badpop);
DECLARE_DO_FUN(do_bamfin);
DECLARE_DO_FUN(do_bamfout);
DECLARE_DO_FUN(do_ban);
DECLARE_DO_FUN(do_basepurge);
DECLARE_DO_FUN(do_boo);
DECLARE_DO_FUN(do_boom);
DECLARE_DO_FUN(do_boot);
DECLARE_DO_FUN(do_bounty);
DECLARE_DO_FUN(do_bringon);
DECLARE_DO_FUN(do_bury);
DECLARE_DO_FUN(do_bug);
DECLARE_DO_FUN(do_characters);
DECLARE_DO_FUN(do_clone);
DECLARE_DO_FUN(do_close);
DECLARE_DO_FUN(do_color);
DECLARE_DO_FUN(do_count);
DECLARE_DO_FUN(do_combine);
DECLARE_DO_FUN(do_commands);
DECLARE_DO_FUN(do_commbadge);
DECLARE_DO_FUN(do_credits);
DECLARE_DO_FUN(do_defec);
DECLARE_DO_FUN(do_defect);
DECLARE_DO_FUN(do_delet);
DECLARE_DO_FUN(do_delete);
DECLARE_DO_FUN(do_deny);
DECLARE_DO_FUN(do_depress);
DECLARE_DO_FUN(do_destroy);
DECLARE_DO_FUN(do_disable);
DECLARE_DO_FUN(do_disconnect);
DECLARE_DO_FUN(do_donate);
DECLARE_DO_FUN(do_done);
DECLARE_DO_FUN(do_down);
DECLARE_DO_FUN(do_duel);
DECLARE_DO_FUN(do_drag);
DECLARE_DO_FUN(do_drive);
DECLARE_DO_FUN(do_drop);
DECLARE_DO_FUN(do_dump);
DECLARE_DO_FUN(do_east);
DECLARE_DO_FUN(do_explode);
DECLARE_DO_FUN(do_gecho);
DECLARE_DO_FUN(do_gemote);
DECLARE_DO_FUN(do_gocial);
DECLARE_DO_FUN(do_emote);
DECLARE_DO_FUN(do_enable);
DECLARE_DO_FUN(do_enter);
DECLARE_DO_FUN(do_equipment);
DECLARE_DO_FUN(do_examine);
DECLARE_DO_FUN(do_exits);
DECLARE_DO_FUN(do_expan);
DECLARE_DO_FUN(do_expand);
DECLARE_DO_FUN(do_follow);
DECLARE_DO_FUN(do_for);
DECLARE_DO_FUN(do_force);
DECLARE_DO_FUN(do_freeze);
DECLARE_DO_FUN(do_get);
DECLARE_DO_FUN(do_give);
DECLARE_DO_FUN(do_goto);
DECLARE_DO_FUN(do_grab);
DECLARE_DO_FUN(do_help);
DECLARE_DO_FUN(do_hide);
DECLARE_DO_FUN(do_holylight);
DECLARE_DO_FUN(do_idea);
DECLARE_DO_FUN(do_immtalk);
DECLARE_DO_FUN(do_imptalk);
DECLARE_DO_FUN(do_imotd);
DECLARE_DO_FUN(do_induct);
DECLARE_DO_FUN(do_inventory);
DECLARE_DO_FUN(do_invis);
DECLARE_DO_FUN(do_join);
DECLARE_DO_FUN(do_kill);
DECLARE_DO_FUN(do_kills);
DECLARE_DO_FUN(do_kill_message);
DECLARE_DO_FUN(do_leave);
DECLARE_DO_FUN(do_load);
DECLARE_DO_FUN(do_lock);
DECLARE_DO_FUN(do_log);
DECLARE_DO_FUN(do_look);
DECLARE_DO_FUN(do_lose_link);
DECLARE_DO_FUN(do_man);
DECLARE_DO_FUN(do_memory);
DECLARE_DO_FUN(do_mfind);
DECLARE_DO_FUN(do_mload);
DECLARE_DO_FUN(do_mset);
DECLARE_DO_FUN(do_motd);
DECLARE_DO_FUN(do_newlock);
DECLARE_DO_FUN(do_nochannels);
DECLARE_DO_FUN(do_noemote);
DECLARE_DO_FUN(do_noleader);
DECLARE_DO_FUN(do_nonote);
DECLARE_DO_FUN(do_north);
DECLARE_DO_FUN(do_noshout);
DECLARE_DO_FUN(do_note);
DECLARE_DO_FUN(do_notell);
DECLARE_DO_FUN(do_open);
DECLARE_DO_FUN(do_passwor);
DECLARE_DO_FUN(do_password);
DECLARE_DO_FUN(do_peace);
DECLARE_DO_FUN(do_pecho);
DECLARE_DO_FUN(do_penalize);
DECLARE_DO_FUN(do_ping);
DECLARE_DO_FUN(do_ptell);
DECLARE_DO_FUN(do_pull);
DECLARE_DO_FUN(do_purge);
DECLARE_DO_FUN(do_push);
DECLARE_DO_FUN(do_put);
DECLARE_DO_FUN(do_qui);
DECLARE_DO_FUN(do_quiet);
DECLARE_DO_FUN(do_quit);
DECLARE_DO_FUN(do_rank);
DECLARE_DO_FUN(do_reboot);
DECLARE_DO_FUN(do_repop);
DECLARE_DO_FUN(do_lecho);
DECLARE_DO_FUN(do_remove);
DECLARE_DO_FUN(do_rename);
DECLARE_DO_FUN(do_reply);
DECLARE_DO_FUN(do_report);
DECLARE_DO_FUN(do_resig);
DECLARE_DO_FUN(do_resign);
DECLARE_DO_FUN(do_rest);
DECLARE_DO_FUN(do_restore);
DECLARE_DO_FUN(do_save);
DECLARE_DO_FUN(do_save_all);
DECLARE_DO_FUN(do_saveban);
DECLARE_DO_FUN(do_savehelps);
DECLARE_DO_FUN(do_say);
DECLARE_DO_FUN(do_scan);
DECLARE_DO_FUN(do_score);
DECLARE_DO_FUN(do_second);
DECLARE_DO_FUN(do_sedit);
DECLARE_DO_FUN(do_set);
DECLARE_DO_FUN(do_shout);
DECLARE_DO_FUN(do_shutdow);
DECLARE_DO_FUN(do_shutdown);
DECLARE_DO_FUN(do_sit);
DECLARE_DO_FUN(do_skills);
DECLARE_DO_FUN(do_sla);
DECLARE_DO_FUN(do_slay);
DECLARE_DO_FUN(do_sleep);
DECLARE_DO_FUN(do_snoop);
DECLARE_DO_FUN(do_south);
DECLARE_DO_FUN(do_socials);
DECLARE_DO_FUN(do_sockets);
DECLARE_DO_FUN(do_stand);
DECLARE_DO_FUN(do_startup);
DECLARE_DO_FUN(do_stat);
DECLARE_DO_FUN(do_status);
DECLARE_DO_FUN(do_story);
DECLARE_DO_FUN(do_string);
DECLARE_DO_FUN(do_team);
DECLARE_DO_FUN(do_teamstats);
DECLARE_DO_FUN(do_teamtalk);
DECLARE_DO_FUN(do_teleport);
DECLARE_DO_FUN(do_tell);
DECLARE_DO_FUN(do_tick);
DECLARE_DO_FUN(do_time);
DECLARE_DO_FUN(do_title);
DECLARE_DO_FUN(do_top);
DECLARE_DO_FUN(do_toss);
DECLARE_DO_FUN(do_track);
DECLARE_DO_FUN(do_traitor);
DECLARE_DO_FUN(do_transfer);
DECLARE_DO_FUN(do_trust);
DECLARE_DO_FUN(do_typo);
DECLARE_DO_FUN(do_undeny);
DECLARE_DO_FUN(do_unload);
DECLARE_DO_FUN(do_unlock);
DECLARE_DO_FUN(do_untraitor);
DECLARE_DO_FUN(do_use);
DECLARE_DO_FUN(do_up);
DECLARE_DO_FUN(do_vnum);
DECLARE_DO_FUN(do_volunteer);
DECLARE_DO_FUN(do_wake);
DECLARE_DO_FUN(do_wear);
DECLARE_DO_FUN(do_west);
DECLARE_DO_FUN(do_where);
DECLARE_DO_FUN(do_who);
DECLARE_DO_FUN(do_whois);
DECLARE_DO_FUN(do_wizhelp);
DECLARE_DO_FUN(do_wizlock);
DECLARE_DO_FUN(do_wizlist);
DECLARE_DO_FUN(do_wiznet);
DECLARE_DO_FUN(do_addbot);
