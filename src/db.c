
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

#include    "ground0.h"
#include    "db.h"
#include    "memory.h"
#include    "ban.h"

#include    <sys/resource.h>
#include    <sys/stat.h>

char *npc_parse(const char *, struct char_data *, struct char_data *,
                struct obj_data *, const char *);

void set_bases();
void distribute_mobs();
void scatter_objects();
void scatter_obj(OBJ_DATA *);
void randomize_level(LEVEL_DATA *);
void set_eq_numbers();
void find_level_commonality(LEVEL_DATA *, LEVEL_DATA *,
                            int *, int *, int *, int *, int *, int *);
CHAR_DATA *alloc_char();
void get_string_space();
void set_char_defaults(CHAR_DATA *);
int get_next_char(FILE *);

DECLARE_DO_FUN (do_save_all);
DECLARE_DO_FUN (do_wear);
DECLARE_DO_FUN (do_load);
DECLARE_DO_FUN (do_ban);
DECLARE_DO_FUN (do_goto);

#define MAX_MEDIC       1
#define MAX_MARINE      1
#define MAX_
/*
 * Globals.
 */
OBJ_INDEX_DATA *all_objects = NULL;
LEVEL_DATA *the_city;
HELP_DATA *help_first;
HELP_DATA *help_last;

int VNUM_FIRE = -1;
int VNUM_NAPALM = -1;
int VNUM_HQ = -1;
int VNUM_DARK = -1;
int VNUM_GAS = -1;

struct obj_vehicle_data dummy_vehicle;

char *scenario_name = NULL;
extern char scenario_dir[256];
int enforcer_depress = 90;      /* Decrement every minute. */
int round_time_left = -1;

char bug_buf[2 * MAX_INPUT_LENGTH];
CHAR_DATA *char_list;
char log_buf[2 * MAX_INPUT_LENGTH];
NOTE_DATA *note_list = NULL;
OBJ_DATA *object_list;
char *help_greeting1;
char *help_gameover;

TIME_INFO_DATA time_info;

int mobile_count = 0;
int newmobs = 0;
int newobjs = 0;
GOD_TYPE *god_table;

int expansions = 0;
int expand_event = 0;

CHAR_DATA *enforcer;
CHAR_DATA *pill_box;
CHAR_DATA *guardian;
CHAR_DATA *medic;
CHAR_DATA *trooper;
CHAR_DATA *marine;
CHAR_DATA *turret;
OBJ_INDEX_DATA *flag;

/* Semi-locals. */
bool fBootDb;
char strArea[MAX_INPUT_LENGTH];

/* Command help synopses. */
int g_noCmdSynopses = 0;
struct cmd_synopses_data *g_cmdSynopses = NULL;

/* Enforcer parameters. */
bool spamkill_protection = TRUE;
bool time_protection = TRUE;

/* Tired of having the outfit stuff hardcoded. */
int g_numOutfits = 0;
OUTFIT_DATA *g_outfitTable = NULL;


/* For the new handling of teams (per scenario). */
int g_numTeams = 0;
struct team_type *team_table = NULL;
int TEAM_NONE = -1;
int TEAM_TRAITOR = -1;
int TEAM_MOB = -1;
int TEAM_ADMIN = -1;


/* Here we go... */
int max_ammo_types = 0;
char **ammo_types = NULL;


/*
 * Local booting procedures.
 */
void init_mm(void);
void load_area(FILE * fp);
void load_helps(FILE * fp);
void load_obj_header(FILE * fp);
void load_objects(FILE * fp);
void load_notes(void);
void load_whole_game(FILE * fp);
void load_gods(FILE * fp);
void load_mobs(FILE * fp);
void load_teams(FILE * fp);
void load_outfit(FILE * fp);
void load_consts(FILE * fp);
void load_cmd_helps(FILE * fp);
void read_index(const char *, const char *);
void read_area_file(FILE * fp, const char *);

/* RT max open files fix */

void
maxfilelimit ()
{
    struct rlimit r;

    getrlimit(RLIMIT_NOFILE, &r);
    r.rlim_cur = r.rlim_max;
    setrlimit(RLIMIT_NOFILE, &r);
}



void
read_index (const char *dir, const char *index)
{
    char filename[256];
    char line[256];
    struct stat sb;
    FILE *idxfp;
    FILE *fp;

    sprintf(filename, "%s%s", dir, index);

    if ( !(idxfp = fopen(filename, "r")) )
    {
        logerr(filename);
        exit(STATUS_ERROR);
    }

    while ( get_line(idxfp, line, 256) != -1 )
    {
        sprintf(filename, "%s%s", dir, line);

        if ( lstat(filename, &sb) == -1 )
        {
            logerr(filename);
            exit(STATUS_ERROR);
        }
        else if ( S_ISDIR(sb.st_mode) )
        {
            if ( filename[strlen(filename) - 1] != '/' )
                strcat(filename, "/");
            read_index(filename, INDEX_FILE);
            continue;
        }
        else if ( !(fp = fopen(filename, "r")) )
        {
            logerr(filename);
            exit(STATUS_ERROR);
        }

        read_area_file(fp, filename);
        fclose(fp);
    }

    fclose(idxfp);
}


void
read_area_file (FILE * fp, const char *fname)
{
    char *word;

    for ( ;; )
    {
        if ( fread_letter(fp) != '#' )
        {
            logmesg("%s: Missing section marker (#).", fname);
            exit(STATUS_ERROR);
        }

        word = fread_word(fp);

        if ( word[0] == '$' )
            break;
        else
        {
            int err = 0;

            switch (word[0])
            {
                default:
                    err = 1;
                    break;
                case 'O':
                    if ( !str_cmp(word, "OBJ_HEADER") )
                        load_obj_header(fp);
                    else if ( !str_cmp(word, "OBJECTS") ) {
                        logmesg("%s: loading objects", fname);
                        load_objects(fp);
                    } else if ( !str_cmp(word, "OUTFIT") )
                        load_outfit(fp);
                    else
                        err = 1;
                    break;
                case 'A':
                    if ( !str_cmp(word, "AREA") )
                        load_area(fp);
                    else
                        err = 1;
                    break;
                case 'H':
                    if ( !str_cmp(word, "HELPS") )
                        load_helps(fp);
                    else
                        err = 1;
                    break;
                case 'G':
                    if ( !str_cmp(word, "GODS") )
                        load_gods(fp);
                    else
                        err = 1;
                    break;
                case 'B':
                    if ( !str_cmp(word, "BANS") )
                        LoadBans(fp);
                    else
                        err = 1;
                    break;
                case 'M':
                    if ( !str_cmp(word, "MOBS") )
                        load_mobs(fp);
                    else
                        err = 1;
                    break;
                case 'C':
                    if ( !str_cmp(word, "CONSTS") )
                        load_consts(fp);
                    else if ( !str_cmp(word, "CMDHELP") )
                        load_cmd_helps(fp);
                    else
                        err = 1;
                    break;
                case 'T':
                    if ( !str_cmp(word, "TEAMS") )
                        load_teams(fp);
                    else
                        err = 1;
                    break;
            }

            if ( err )
            {
                logmesg("%s: Bad section name '%s'", fname, word);
                exit(STATUS_ERROR);
            }
        }
    }
}


void
boot_db (void)
{
    maxfilelimit();
    get_string_space();
    fBootDb = TRUE;
    srandom(time(0));

    logmesg("Reading boot files.");
    read_index(BOOT_DIR, BOOT_LIST);

    if ( *scenario_dir )
        read_index(scenario_dir, INDEX_FILE);
    else
        read_index(CUR_SCEN_DIR, INDEX_FILE);

    logmesg("Setting top arrays to defaults");
    initialise_top(top_players_kills, NUM_TOP_KILLS);
    initialise_top(top_players_deaths, NUM_TOP_DEATHS);
    initialise_top(top_players_hours, NUM_TOP_HOURS);

    logmesg("Loading top files");
    read_top(TOP_FILE, top_players_kills, NUM_TOP_KILLS);
    read_top(TOP_DEATH_FILE, top_players_deaths, NUM_TOP_DEATHS);
    read_top(TOP_HOURS_FILE, top_players_hours, NUM_TOP_HOURS);

    memset((char *) &dummy_vehicle, 0, sizeof(struct obj_vehicle_data));

    logmesg("Setting eq numbers");
    set_eq_numbers();

    {
        int x = number_range(0, the_city->x_size - 1);
        int y = number_range(0, the_city->y_size - 1);

        ammo_repop[0] = index_room(the_city->rooms_on_level, x, y);
        ammo_repop[2] = index_room(the_city->rooms_on_level,
                                   the_city->x_size - 1 - x,
                                   the_city->y_size - 1 - y);
        ammo_repop[1] =
            index_room(the_city->rooms_on_level, the_city->x_size / 2,
                       the_city->y_size / 2);
        logmesg("ammo repops at: (%d, %d), (%d, %d), (%d, %d)",
                   ammo_repop[0]->x, ammo_repop[0]->y, ammo_repop[1]->x,
                   ammo_repop[1]->y, ammo_repop[2]->x, ammo_repop[2]->y);
    }

    logmesg("Distributing objects . . .");
    scatter_objects();

    logmesg("Setting bases . . .");
    set_bases();

    logmesg("Distributing mobs . . .");
    distribute_mobs();

    logmesg("Reading notes . . .");
    load_notes();

    logmesg("Loading socials . . .");
    load_social_table();

    logmesg("Loading polls . . .");
    load_polls();

    /* Declare booting over. */
    fBootDb = FALSE;
}


void
set_bases ()
{
    OBJ_DATA *obj;
    int count;
    char stat_buf[MAX_INPUT_LENGTH];
    ROOM_DATA *room;

    if ( VNUM_HQ == -1 )
        return;

    for ( count = 0; count < g_numTeams; count++ )
    {
        if ( !team_table[count].active )
            continue;

        obj = create_object(get_obj_index(VNUM_HQ), 0);
        team_table[count].hq = obj;
        obj->name = str_dup("An HQ.");
        sprintf(stat_buf, "The %s&n headquarters entrance",
                team_table[count].who_name);
        obj->short_descr = str_dup(stat_buf);
        sprintf(stat_buf, "The %s&n headquarters entrance is here.",
                team_table[count].who_name);
        obj->description = str_dup(stat_buf);
        obj->interior->name = str_dup("The inside of HQ.");
        obj->team = count;

        room = index_room(the_city->rooms_on_level,
                          (the_city->x_size -
                           1) * team_table[count].x_mult,
                          (the_city->y_size -
                           1) * team_table[count].y_mult);
        obj_to_room(obj, room);

        team_table[count].map_memory =
            calloc(the_city->x_size * the_city->y_size, sizeof(char));

        sprintf(log_buf, "Setting %s hq to %d, %d.",
                team_table[count].name,
                (the_city->x_size - 1) * team_table[count].x_mult,
                (the_city->y_size - 1) * team_table[count].y_mult);
        logmesg(log_buf);
    }
}


void
scatter_objects ()
{
    OBJ_INDEX_DATA *obj_ind;
    OBJ_DATA *obj;
    int count;
    int add_obj;

    for (obj_ind = all_objects; obj_ind->name[0] != '$';
         obj_ind = &(obj_ind[1]))
        if ( obj_ind->number_to_put_in > 0 )
        {
            add_obj = 0;

            if ( fBootDb )
                add_obj = 1;
            else if ( obj_ind->item_type != ITEM_TEAM_VEHICLE )
                add_obj = 1;

            if ( add_obj )
                for ( count = 0; count < obj_ind->number_to_put_in; count++ )
                {
                    obj = create_object(obj_ind, fBootDb);
                    scatter_obj(obj);
                }
        }
}



void
load_consts (FILE * fp)
{
    register char *word = NULL;
    int i;

    word = fread_word(fp);

    if ( str_cmp(word, "MAXIMUMS") )
    {
        logmesg
            ("DB ERROR: Maximums must be the first entry in #CONSTS.");
        exit(EXIT_FAILURE);
    }

    max_ammo_types = fread_number(fp);

    if ( max_ammo_types <= 0 )
    {
        logmesg("DB WARNING: No constants in #CONSTS section.");
        return;
    }

    ammo_types = alloc_perm(max_ammo_types * sizeof(char *));

    for ( ; !feof(fp); )
    {
        word = fread_word(fp);

        if ( !*word )
            continue;
        else if ( *word == '$' )
            break;
        else if ( *word == '*' )
        {
            fread_to_eol(fp);
            continue;
        }

        if ( !str_cmp(word, "AMMO_TYPE") || !str_cmp(word, "AMMOTYPE") )
        {
            i = fread_number(fp);

            if ( i >= max_ammo_types )
            {
                logmesg
                    ("DB ERROR: AmmoType record for %d, which is >= maximum (%d).",
                     i, max_ammo_types);
                exit(EXIT_FAILURE);
            }
            else if ( ammo_types[i] )
            {
                logmesg
                    ("DB WARNING: AmmoType string constant specified for %d more than once.",
                     i);
                free_string(ammo_types[i]);
            }

            ammo_types[i] = fread_string(fp);
        }
        else
        {
            logmesg("DB ERROR: Unrecognized constant type '%s'.", word);
            exit(EXIT_FAILURE);
        }
    }
}


void
load_gods (FILE * fp)
{
    int num_gods, count = 0;

    logmesg("Reading the god list . . .");
    num_gods = fread_number(fp);
    num_gods++;                 /* blank record at the end. */
    god_table = alloc_perm(num_gods * sizeof(GOD_TYPE));
    god_table[count].rl_name = fread_string(fp);
    while ( god_table[count].rl_name[0] != '$' )
    {
        god_table[count].trust = fread_number(fp);
        if ( god_table[count].trust >= MAX_TRUST )
        {
            logmesg
                ("No characters of IMP trust are allowed in the god bootfile.");
            exit(STATUS_ERROR);
        }
        god_table[count].game_names = fread_string(fp);
        god_table[count].password = fread_string(fp);
        god_table[count].room_name = fread_string(fp);
        god_table[count].honorary = fread_number(fp);
        count++;
        if ( count == num_gods )
        {
            logmesg("More gods were listed than were declared");
            exit(STATUS_ERROR);
        }
        god_table[count].rl_name = fread_string(fp);
    }
    god_table[count].rl_name = god_table[count].game_names =
        god_table[count].password = god_table[count].room_name = "";
    god_table[count].trust = 0;
}


char *
TeamSubstitute (const char *orig, int team)
{
    static char buf[MAX_STRING_LENGTH];
    register char *ptr = buf;
    register char *rd;

    while ( *orig )
    {
        if ( *orig == '$' && *(orig + 1) == 'a' )
        {
            orig++;
            *(ptr++) = TCON_TOK;
            *(ptr++) = '{';
            for ( rd = team_table[team].who_name; *rd; rd++ )
                *(ptr++) = *rd;
            *(ptr++) = TCON_TOK;
            *(ptr++) = '}';
        }
        else
            *(ptr++) = *orig;

        orig++;
    }

    *ptr = '\0';
    return buf;
}


/* To elimate repitious code from distribute_mobs(). -Raj */
void
teamed_mob (CHAR_DATA * proto, int team, int realObjs, int times)
{
    CHAR_DATA *instance;
    char buf[MAX_STRING_LENGTH];
    ROOM_DATA *room;
    int x;

    if ( !proto )
        return;

    sprintf(buf, "Distributing mobile %s for %s team.", proto->names,
            team_table[team].name);
    logmesg(buf);

    room = index_room(the_city->rooms_on_level,
                      (the_city->x_size - 1) * team_table[team].x_mult,
                      (the_city->y_size - 1) * team_table[team].y_mult);

    for ( x = 0; x < times; x++ )
    {
        instance = clone_mobile(proto, realObjs);
        sprintf(buf, "%s %s", team_table[team].name, proto->names);
        instance->names = str_dup(buf);
        char *foo = TeamSubstitute(instance->short_descript, team);

        instance->short_descript = str_dup(foo);
        instance->team = team;
        char_from_room(instance);
        char_to_room(instance, room);
    }
}


/* This is so much cleaner thanks to the above. :)  -Raj */
void
distribute_mobs (void)
{
    CHAR_DATA *mob;
    int count;

    for ( mob = char_list; mob; mob = mob->next )
        if ( IS_NPC(mob) )
            do_goto(mob, mob->where_start);

    for ( count = 0; count < g_numTeams; count++ )
    {
        if ( !team_table[count].active )
            continue;

        teamed_mob(guardian, count, TRUE, 1);
        teamed_mob(medic, count, TRUE, MAX_MEDIC);
        teamed_mob(marine, count, TRUE, MAX_MARINE);
    }
}


void
EnqueueNpcEvent (struct cmd_queue *q, const char *cmd)
{
    struct cmd_node *node = calloc(1, sizeof(struct cmd_node));

    node->cmd = cmd;
    node->next = NULL;

    if ( q->head == NULL )
        q->head = q->tail = node;
    else
    {
        q->tail->next = node;
        q->tail = node;
    }
}


void
load_mobs (FILE * fp)
{
    char fword[MAX_INPUT_LENGTH];
    CHAR_DATA *inject_mob;
    char *buf, *ptr;
    int vnum;
    int low, high;

    buf = NULL;
    do
    {
        buf = fread_string_eol(fp);
    }
    while ( buf[0] == ']' );
    buf = fread_string(fp);

    while ( buf[0] != '$' )
    {
        inject_mob = alloc_char();
        clear_char(inject_mob);
        inject_mob->names = str_dup(buf);
        ptr = one_argument(buf, fword);

        if ( !str_cmp(fword, "_droid_") )
        {
            pill_box = inject_mob;
            buf = ptr;
        }
        else if ( !str_cmp(fword, "_enforcer_") )
        {
            enforcer = inject_mob;
            buf = ptr;
        }
        else if ( !str_cmp(fword, "_guardian_") )
        {
            guardian = inject_mob;
            buf = ptr;
        }
        else if ( !str_cmp(fword, "_trooper_") )
        {
            trooper = inject_mob;
            buf = ptr;
        }
        else if ( !str_cmp(fword, "_turret_") )
        {
            turret = inject_mob;
            buf = ptr;
        }
        else if ( !str_cmp(fword, "_medic_") )
        {
            medic = inject_mob;
            buf = ptr;
        }
        else if ( !str_cmp(fword, "_marine_") )
        {
            marine = inject_mob;
            buf = ptr;
        }

        inject_mob->names = str_dup(buf);
        inject_mob->short_descript = fread_string(fp);
        set_char_defaults(inject_mob);
        inject_mob->pcdata = NULL;
        fread_word(fp);

        inject_mob->npc = calloc(1, sizeof(struct npc_data));

        low = fread_number(fp);
        if ( fgetc(fp) != '-' )
            inject_mob->hit = inject_mob->max_hit = low;
        else
        {
            high = fread_number(fp);
            inject_mob->hit = inject_mob->max_hit =
                number_range(low, high);
        }

        fread_word(fp);
        inject_mob->move_delay = fread_number(fp);
        fread_word(fp);
        inject_mob->ld_behavior = fread_number(fp);
        fread_word(fp);
        inject_mob->act = fread_flag(fp);

        buf = fread_word(fp);

        if ( !str_cmp(buf, "TEAM") )
        {
            buf = fread_word(fp);
            inject_mob->team = lookup_team(buf);
            buf = fread_word(fp);
        }
        else
            inject_mob->team = TEAM_MOB;

        if ( !str_cmp(buf, "WORTH") )
        {
            inject_mob->worth = fread_number(fp);
            fread_word(fp);
        }
        else
            inject_mob->worth = 1;

        inject_mob->sex = fread_number(fp);
        fread_word(fp);

        inject_mob->where_start = fread_string(fp);
        char_to_room(inject_mob, safe_area);

        fread_word(fp);
        while ( (vnum = fread_number(fp)) != 0 )
            obj_to_char(create_object(get_obj_index(vnum), 0), inject_mob);

        inject_mob->next = char_list;
        char_list = inject_mob;
        buf = fread_string(fp);

        /* New: */
        while ( 1 )
        {
            ptr = one_argument(buf, fword);

            if ( !str_cmp(fword, "OnCreate") )
                EnqueueNpcEvent(&inject_mob->npc->birth, ptr);
            else if ( !str_cmp(fword, "OnHeartbeat") )
                EnqueueNpcEvent(&inject_mob->npc->heartbeat, ptr);
            else if ( !str_cmp(fword, "OnDepress") )
                EnqueueNpcEvent(&inject_mob->npc->depress, ptr);
            else if ( !str_cmp(fword, "OnEnforcer") )
                EnqueueNpcEvent(&inject_mob->npc->enforcer, ptr);
            else if ( !str_cmp(fword, "OnDeath") )
                EnqueueNpcEvent(&inject_mob->npc->death, ptr);
            else if ( !str_cmp(fword, "OnKill") )
                EnqueueNpcEvent(&inject_mob->npc->kill, ptr);
            else
                break;

            buf = fread_string(fp);
        }

        if ( inject_mob->npc->birth.head )
        {
            struct cmd_node *node = inject_mob->npc->birth.head;
            char *hold;

            while ( node != NULL )
            {
                hold = npc_parse(node->cmd, inject_mob, NULL, NULL, NULL);
                if ( !hold )
                    continue;
                interpret(inject_mob, hold);
                node = node->next;
            }
        }
        else
        {
            do_wear(inject_mob, "all");
            do_load(inject_mob, "");
        }
    }
}


void
load_obj_header (FILE * fp)
{
    int num_objects = -1;
    char word[256];
    char line[256];
    int lineno = 0;
    char arg[256];
    char *ptr;
    int i;

    logmesg("Reading the object header file . . .");

    while ( (i = get_line(fp, line, 256)) != -1 )
    {
        lineno += i;
        ptr = one_argument(line, word);
        one_argument(ptr, arg);

        if ( !str_cmp(word, "vnum_hq") )
            VNUM_HQ = atoi(arg);
        else if ( !str_cmp(word, "vnum_fire") )
            VNUM_FIRE = atoi(arg);
        else if ( !str_cmp(word, "vnum_gas") )
            VNUM_GAS = atoi(arg);
        else if ( !str_cmp(word, "vnum_dark") )
            VNUM_DARK = atoi(arg);
        else if ( !str_cmp(word, "vnum_napalm") )
            VNUM_NAPALM = atoi(arg);
        else if ( !str_cmp(word, "max") )
            num_objects = atoi(arg);
        else if ( !str_cmp(word, "#$") )
            break;
    }

    if ( num_objects == -1 )
    {
        logmesg("No maximum object count in object header!");
        exit(STATUS_ERROR);
    }

    if ( VNUM_HQ == -1 )
        logmesg
            ("Warning: No vnum_hq set in object header -- HQs disabled.");
    else if ( VNUM_NAPALM == -1 )
        logmesg
            ("Warning: No vnum_napalm set in object header -- napes disabled.");
    else if ( VNUM_FIRE == -1 )
        logmesg
            ("Warning: No vnum_fire set in object header -- fires disabled.");
    else if ( VNUM_GAS == -1 )
        logmesg
            ("Warning: No vnum_gas set in object header -- gas disabled.");
    else if ( VNUM_DARK == -1 )
        logmesg
            ("Warning: No vnum_dark set in object header -- darkness disabled.");

    all_objects = alloc_perm(sizeof(OBJ_INDEX_DATA) * num_objects);
    logmesg("Object header has been read.");
}


void
load_damage_numbers (FILE * fp, OBJ_INDEX_DATA * current_obj)
{
    char *buf;
    int count;

    buf = fread_word(fp);

    if ( !strcmp(buf, "NUM_EXPLODE") )
    {
        if (!IS_SET
            (current_obj->extract_flags, EXTRACT_EXPLODE_ON_EXTRACT))
        {
            logmesg("NUM_EXPLODE on non-explosive.");
            exit(STATUS_ERROR);
        }

        current_obj->num_extract_explode = fread_number(fp);
        buf = fread_word(fp);
    }
    else
    {
        current_obj->num_extract_explode = 1;
    }

    if ( strcmp(buf, "DAM_CH") )
    {
        logmesg("got %s instead of DAM_CH (%s %s).", buf, buf,
                   fread_string_eol(fp));
        exit(STATUS_ERROR);
    }

    for ( count = 0; count < 3; count++ )
        current_obj->damage_char[count] = fread_number(fp);

    fread_to_eol(fp);
    buf = fread_word(fp);

    for ( count = 0; count < 3; count++ )
        current_obj->damage_structural[count] = fread_number(fp);
}


void
load_gen_obj_flags (FILE * fp, OBJ_INDEX_DATA * current_obj)
{
    char *buf;
    unsigned int flags;

    buf = fread_word(fp);
    flags = current_obj->general_flags = fread_flag(fp);

    if ( IS_SET(flags, GEN_BURNS_ROOM | GEN_NO_AMMO_NEEDED | GEN_CHOKES_ROOM) )
        load_damage_numbers(fp, current_obj);

    if ( IS_SET(flags, GEN_WALK_DAMAGE) )
    {
        load_damage_numbers(fp, current_obj);
        current_obj->attack_message = fread_string(fp);
        buf = fread_word(fp);
        current_obj->ammo = fread_number(fp);
        buf = fread_word(fp);
        current_obj->rounds_delay = fread_number(fp);
    }

    if ( IS_SET(flags, GEN_NO_AMMO_NEEDED) )
        current_obj->attack_message = fread_string(fp);

    if ( IS_SET(flags, GEN_LEAVE_TRAIL) )
    {
        buf = fread_word(fp);
        current_obj->spawnTrail = fread_number(fp);
        buf = fread_word(fp);
        current_obj->spawnMax = fread_number(fp);
        buf = fread_word(fp);
        current_obj->spawnTimer = fread_number(fp);
    }

    if ( IS_SET(flags, GEN_DEPLOYMENT) )
    {
        buf = fread_word(fp);
        current_obj->arrival_time = fread_number(fp);
        buf = fread_word(fp);
        current_obj->destination = fread_string(fp);
    }
}


void
load_extract_obj_flags (FILE * fp, OBJ_INDEX_DATA * current_obj)
{
    char *buf;
    unsigned int flags;

    buf = fread_word(fp);
    flags = current_obj->extract_flags = fread_flag(fp);

    if ( flags )
        current_obj->explode_desc = fread_string(fp);

    if ( IS_SET(flags, EXTRACT_EXPLODE_ON_EXTRACT) )
        load_damage_numbers(fp, current_obj);

    if ( IS_SET(flags, EXTRACT_BURN_ON_EXTRACT) )
    {
        buf = fread_word(fp);

        if ( !strcmp(buf, "NUM_FIRES") )
        {
            current_obj->num_extract_burn = fread_number(fp);
            buf = fread_word(fp);
        }
        else
        {
            current_obj->num_extract_burn = 1;
        }

        current_obj->burn_time = fread_number(fp);
    }

    if ( IS_SET(flags, EXTRACT_CHOKE_ON_EXTRACT) )
    {
        buf = fread_word(fp);
        current_obj->choke_time = fread_number(fp);
    }
}

void
load_use_obj_flags (FILE * fp, OBJ_INDEX_DATA * current_obj)
{
    char *buf;
    unsigned int flags;

    buf = fread_word(fp);
    if ( strcmp(buf, "USE") )
    {
        logmesg("got %s instead of USE.", buf);
        exit(STATUS_ERROR);
    }
    flags = current_obj->usage_flags = fread_flag(fp);

    if ( IS_SET(flags, USE_HEAL) || IS_SET(flags, USE_TANKKIT) )
        load_damage_numbers(fp, current_obj);

    if ( IS_SET(flags, USE_MAKE_OBJ_ROOM) )
    {
        buf = fread_word(fp);
        current_obj->spawnTrail = fread_number(fp);
        buf = fread_word(fp);
        current_obj->spawnMax = fread_number(fp);
    }

    if ( IS_SET(flags, USE_HEALTH_MONITOR) || IS_SET(flags, USE_BATTLEMAP) )
    {
        buf = fread_word(fp);

        if ( !str_cmp(buf, "MAX_USES") || !str_cmp(buf, "MAXUSES") )
            current_obj->ammo = fread_number(fp);
        else
        {
            logmesg("got %s instead of MaxUses.", buf);
            exit(EXIT_FAILURE);
        }
    }
}

void
load_obj_item_type (FILE * fp, OBJ_INDEX_DATA * current_obj)
{
    char *buf;

    buf = fread_word(fp);
    if ( strcmp(buf, "ITEM_TYPE") )
    {
        logmesg("got %s instead of item_type.", buf);
        exit(STATUS_ERROR);
    }
    current_obj->item_type = fread_number(fp);

    buf = fread_word(fp);
    if ( strcmp(buf, "WEAR") )
    {
        logmesg("got %s instead of wear.", buf);
        exit(STATUS_ERROR);
    }

    current_obj->wear_flags = fread_number(fp);

    switch (current_obj->item_type)
    {
        case ITEM_WEAPON:
            buf = fread_word(fp);

            /* MULT is optional. */
            if ( !str_cmp(buf, "MULT") )
            {
                buf = fread_word(fp);
                current_obj->dam_mult = strtod(buf, NULL);
                buf = fread_word(fp);
            }
            else
            {
                current_obj->dam_mult = 1.0;
            }

            current_obj->rounds_per_second = fread_number(fp);
            buf = fread_word(fp);

            current_obj->rounds_delay = 0;

            if ( current_obj->rounds_per_second < 0 )
            {
                current_obj->rounds_delay =
                    -current_obj->rounds_per_second;
                current_obj->rounds_per_second = 1;
            }

            if ( !str_cmp(buf, "DELAY") )
            {
                current_obj->rounds_delay = fread_number(fp);
                buf = fread_word(fp);
            }

            current_obj->ammo_type = fread_number(fp);
            buf = fread_word(fp);

            if ( !str_cmp(buf, "scope_type") )
            {
                current_obj->scope_type = fread_number(fp);
                buf = fread_word(fp);
            }
            else
            {
                current_obj->scope_type = -1;
            }

            current_obj->range = fread_number(fp);
            break;
        case ITEM_ARMOR:
            buf = fread_word(fp);
            current_obj->armor = fread_number(fp);
            break;
        case ITEM_EXPLOSIVE:
            buf = fread_word(fp);
            current_obj->range = fread_number(fp);
            break;
        case ITEM_AMMO:
            buf = fread_word(fp);
            current_obj->ammo_type = fread_number(fp);
            buf = fread_word(fp);
            current_obj->ammo = fread_number(fp);
            load_damage_numbers(fp, current_obj);
            break;
        case ITEM_SCOPE:
            buf = fread_word(fp);
            current_obj->scope_type = fread_number(fp);
            buf = fread_word(fp);
            current_obj->range = fread_number(fp);
            break;
        case ITEM_TEAM_VEHICLE:
            current_obj->vehicle = alloc_perm(sizeof(struct obj_vehicle_data));
            buf = fread_word(fp);

            if ( !str_cmp(buf, "capacity") )
            {
                current_obj->vehicle->capacity = fread_number(fp);
                buf = fread_word(fp);
            }
            else
                current_obj->vehicle->capacity = 0;

            if ( !str_cmp(buf, "interior") )
            {
                current_obj->vehicle->interior_desc = fread_string(fp);
                buf = fread_word(fp);
            }
            else
                current_obj->vehicle->interior_desc =
                    str_dup("Inside a vehicle.");

            if ( !str_cmp(buf, "startActor") )
            {
                current_obj->vehicle->start_message_dp = fread_string(fp);
                buf = fread_word(fp);
            }

            if ( !str_cmp(buf, "startInterior") )
            {
                current_obj->vehicle->start_message_do = fread_string(fp);
                buf = fread_word(fp);
            }

            if ( !str_cmp(buf, "startExterior") )
            {
                current_obj->vehicle->start_message_io = fread_string(fp);
                buf = fread_word(fp);
            }

            if ( !str_cmp(buf, "stopActor") )
            {
                current_obj->vehicle->stop_message_dp = fread_string(fp);
                buf = fread_word(fp);
            }

            if ( !str_cmp(buf, "stopInterior") )
            {
                current_obj->vehicle->stop_message_do = fread_string(fp);
                buf = fread_word(fp);
            }

            if ( !str_cmp(buf, "stopExterior") )
            {
                current_obj->vehicle->stop_message_io = fread_string(fp);
                buf = fread_word(fp);
            }

            current_obj->vehicle->r_positions =
            current_obj->vehicle->positions = fread_flag(fp);
            buf = fread_word(fp);

            if ( !str_cmp(buf, "reqPositions") )
            {
                current_obj->vehicle->r_positions = fread_flag(fp);
                buf = fread_word(fp);
            }

            if ( !str_cmp(buf, "turretDistance") )
            {
                current_obj->vehicle->turret_low = fread_number(fp);
                current_obj->vehicle->turret_high = fread_number(fp);
                buf = fread_word(fp);
            }

            current_obj->vehicle->tomob = fread_string(fp);
            break;
        case ITEM_TEAM_ENTRANCE:
        case ITEM_MISC:
            break;
        default:
            logmesg("Illegal item type %d found.",
                       current_obj->item_type);
            exit(STATUS_ERROR);
    }
}

int
get_next_char (FILE * fp)
{
    int ch = ' ';

    while ( isspace(ch) )
        ch = fgetc(fp);

    return ch;
}

int
get_count_num (int item_type)
{
    switch (item_type)
    {
        case ITEM_WEAPON:
            return COUNT_WEAPONS;
        case ITEM_ARMOR:
            return COUNT_ARMOR;
        case ITEM_EXPLOSIVE:
            return COUNT_EXPLOSIVES;
        case ITEM_AMMO:
            return COUNT_AMMO;
    }

    return COUNT_MISC;
}

int
counts_match (int *counter, int *counter2)
{
    int count;

    for ( count = 0; count < NUM_COUNTS; count++ )
        if ( counter[count] != counter2[count] )
            return 0;
    return 1;
}

void
set_eq_numbers ()
{
    static char *objectives_names[NUM_COUNTS] =
        { "weapons", "armor", "exp", "ammo", "misc" };
    int current_vnum;
    int counters[NUM_COUNTS];
    int objectives[NUM_COUNTS];
    int count;
    OBJ_INDEX_DATA *current_obj;
    int top_vnum;
    int count_num;

    for ( count = 0; count < NUM_COUNTS; count++ )
    {
        counters[count] = 0;
        objectives[count] = 0;
    }

    /* first we find our objective counts and set the current to the number
       that are guaranteed to be in it. */
    for (current_vnum = 1; all_objects[current_vnum - 1].name[0] != '$';
         current_vnum++)
    {
        current_obj = all_objects + current_vnum - 1;
        count_num = get_count_num(current_obj->item_type);
        if ( current_obj->prob_in > 0 )
        {
            objectives[count_num]++;
            if ( current_obj->prob_in == 100 )
                counters[count_num]++;
        }
    }
    top_vnum = current_vnum;

    /* we want 3/4 of them for everything but ammo/guns */
    for ( count = 0; count < NUM_COUNTS; count++ )
    {
        if ( count == COUNT_WEAPONS || count == COUNT_AMMO )
            objectives[count] = (objectives[count] * 87) / 100;
        else
            objectives[count] = (objectives[count] * 3) / 4;
        logmesg("Objective load for %s is %d.", objectives_names[count],
                   objectives[count]);
    }

    /* now make them match */
    while ( !counts_match(counters, objectives) )
    {
        current_obj = all_objects + number_range(0, top_vnum - 1);
        if (current_obj->prob_in != 100 &&
            number_range(1, 100) < current_obj->prob_in)
        {
            int count_num = get_count_num(current_obj->item_type);

            if ( counters[count_num] < objectives[count_num] )
            {
                counters[count_num]++;
                current_obj->prob_in = 100;
            }
        }
    }

    /* everything that isn't 100% now should not go in */
    for (current_vnum = 1; all_objects[current_vnum - 1].name[0] != '$';
         current_vnum++)
    {
        current_obj = all_objects + current_vnum - 1;
        if ( current_obj->prob_in != 100 )
        {
            current_obj->prob_in = 0;
            current_obj->number_to_put_in = 0;
        }
    }
}


/*
 * Snarf some objects
 */
void
load_objects (FILE * fp)
{
    char *buf;
    OBJ_INDEX_DATA *current_obj = NULL;
    int low, high;
    int a_char;
    static int current_vnum = 1;

    if ( !all_objects )
    {
        logmesg("Attempt to load objects before header.");
        exit(STATUS_ERROR);
    }

    a_char = get_next_char(fp);

    for ( ; a_char != '$'; current_vnum++ )
    {
        ungetc(a_char, fp);
        current_obj = all_objects + current_vnum - 1;
        current_obj->vnum = current_vnum;

        load_obj_item_type(fp, current_obj);
        load_gen_obj_flags(fp, current_obj);
        load_extract_obj_flags(fp, current_obj);
        load_use_obj_flags(fp, current_obj);

        current_obj->name = fread_string(fp);
        current_obj->short_descr = fread_string(fp);
        current_obj->description = fread_string(fp);

        buf = fread_word(fp);
        if ( (current_obj->prob_in = fread_number(fp)) > 0 )
        {
            low = fread_number(fp);

            if ( fgetc(fp) != '-' )
            {
                logmesg("'-' expected.");
                exit(STATUS_ERROR);
            }

            high = fread_number(fp);
            current_obj->number_to_put_in = number_range(low, high);
        }
        else
            current_obj->number_to_put_in = 0;

        buf = fread_word(fp);
        current_obj->hp_char = fread_number(fp);
        current_obj->hp_struct = fread_number(fp);

        a_char = get_next_char(fp);
    }

    top_obj_index = current_obj->vnum + 1;
    current_obj = all_objects + current_vnum - 1;
    current_obj->name = str_dup("$");
}


void
clear_level (ROOM_DATA * rooms_on_level, int x_size, int y_size,
            LEVEL_DATA * the_level)
{
    static ROOM_DATA room_zero;
    int current_x, current_y;

    for ( current_x = 0; current_x < x_size; current_x++ )
        for ( current_y = 0; current_y < y_size; current_y++ )
            rooms_on_level[current_x + x_size * current_y] = room_zero;
}

void
set_descs_level (ROOM_DATA * rooms_on_level, int x_size, int y_size,
                LEVEL_DATA * the_level)
{
    ROOM_DATA *temp_room;
    int current_x, current_y;
    char *the_desc[9] = {
        "An Abandoned House",
        "A Nuclear Weapons Warehouse",
        "A Dark Alley",
        "A Barren Field",
        "An Empty Street",
        "A Ransacked Office Building",
        "A Narrow Passageway",
        "A Drug Lab",
        "A Hospital",
    };

    for ( current_x = 0; current_x < x_size; current_x++ )
        for ( current_y = 0; current_y < y_size; current_y++ )
        {
            temp_room = &(rooms_on_level[current_x + x_size * current_y]);
            temp_room->level = the_level->level_number;
            temp_room->this_level = the_level;
            temp_room->people = NULL;
            temp_room->interior_of = NULL;
            temp_room->contents = NULL;
            temp_room->mine = NULL;
            temp_room->room_flags = 0;

            if ( current_x < x_size / 3 )
            {
                if ( current_y < y_size / 3 )
                    temp_room->name = the_desc[0];
                else if ( current_y >= (y_size * 2) / 3 )
                    temp_room->name = the_desc[1];
                else
                    temp_room->name = the_desc[2];
            }
            else if ( current_x >= (x_size * 2) / 3 )
            {
                if ( current_y < y_size / 3 )
                    temp_room->name = the_desc[3];
                else if ( current_y >= (y_size * 2) / 3 )
                    temp_room->name = the_desc[4];
                else
                    temp_room->name = the_desc[5];
            }
            else
            {
                if ( current_y < y_size / 3 )
                    temp_room->name = the_desc[6];
                else if ( current_y >= (y_size * 2) / 3 )
                    temp_room->name = the_desc[7];
                else
                    temp_room->name = the_desc[8];
            }

            temp_room->x = current_x;
            temp_room->y = current_y;
            temp_room->description = temp_room->name;
            temp_room->mine = NULL;
        }
}

void
set_walls_level (ROOM_DATA * rooms_on_level, int x_size, int y_size)
{
    int current_x, current_y;
    ROOM_DATA *temp_room;
    int room_killer;

    for ( current_x = 0; current_x < x_size; current_x++ )
        for ( current_y = 0; current_y < y_size; current_y++ )
        {
            temp_room = index_room(rooms_on_level, current_x, current_y);
            for ( room_killer = 0; room_killer < 6; room_killer++ )
            {
                temp_room->exit[room_killer] = 0;
                if ( (!current_x && (room_killer == DIR_WEST) ) ||
                    (!current_y && (room_killer == DIR_SOUTH)) ||
                    ((current_x == x_size - 1) &&
                     (room_killer == DIR_EAST)) ||
                    ((current_y == y_size - 1) &&
                     (room_killer == DIR_NORTH)) ||
                    (room_killer == DIR_DOWN) || (room_killer == DIR_UP))
                {
                    SET_BIT(temp_room->exit[room_killer], EX_ISWALL);
                    SET_BIT(temp_room->exit[room_killer],
                            EX_ISNOBREAKWALL);
                }
            }
        }
}


ROOM_DATA *
create_room (const char *desc)
{
    ROOM_DATA *room;
    int i;

    room = alloc_perm(sizeof(ROOM_DATA));
    memset((char *) room, 0, sizeof(ROOM_DATA));

    room->name = str_dup(desc);
    room->description = room->name;
    room->this_level = the_city;
    room->x = room->y = room->level = -1;

    for ( i = 0; i < 6; i++ )
        SET_BIT(room->exit[i], EX_ISWALL | EX_ISNOBREAKWALL);

    return room;
}


/*
 * Snarf the whole city
 */
void
load_area (FILE * fp)
{
    LEVEL_DATA *level = NULL;
    int num_levels = 0;
    char *word;
    int x_size;
    int y_size;
    int i;

    the_city = alloc_perm(sizeof(LEVEL_DATA));
    the_city->level_up = NULL;
    the_city->level_down = NULL;

    fprintf(stderr, "\n");
    logmesg("Loading scenario and map.");

    while ( 1 )
    {
        word = fread_word(fp);

        if ( !str_cmp(word, "TIME") )
        {
            enforcer_depress = fread_number(fp);
            if ( !enforcer_depress )
                time_protection = FALSE;
        }
        else if ( !str_cmp(word, "SCENARIO") )
        {
            scenario_name = fread_string(fp);
        }
        else if ( !str_cmp(word, "L") )
        {
            num_levels = fread_number(fp);

            for ( i = 0; i < num_levels; i++ )
            {
                LEVEL_DATA *new_level;

                new_level = alloc_perm(sizeof(LEVEL_DATA));

                if ( level )
                {
                    new_level->level_up = level;
                    level->level_down = new_level;
                }
                else
                    the_city = new_level;

                level = new_level;

                word = fread_word(fp);
                x_size = fread_number(fp);
                y_size = fread_number(fp);

                level->num_levels = num_levels;
                level->level_number = i;

                level->x_size = x_size;
                level->y_size = y_size;
                level->reference_x = number_range(0, x_size - 1);
                level->reference_y = number_range(0, y_size - 1);
                level->level_down = NULL;

                level->rooms_on_level =
                    alloc_mem(x_size * y_size * sizeof(ROOM_DATA));

                clear_level(level->rooms_on_level, x_size, y_size, level);
                set_descs_level(level->rooms_on_level, x_size, y_size,
                                level);
                set_walls_level(level->rooms_on_level, x_size, y_size);
            }
        }
        else if ( !str_cmp(word, "$") )
            break;
        else
        {
            logmesg("DB ERROR: Received unexpected token %s.", word);
            exit(1);
        }
    }

    if ( !scenario_name )
        scenario_name = str_dup("GZ2 Default Scenario");
    logmesg("Name of scenario: %s.", scenario_name);
    logmesg("Round Length    : %d minutes.", enforcer_depress);
    logmesg("Number of Levels: %d", num_levels);

    for ( level = the_city; level; level = level->level_down )
        logmesg("Dimensions of L%d: (%d, %d)", level->level_number,
                   level->x_size, level->y_size);

    fprintf(stderr, "\n");
    logmesg("Making safe room and god rooms . . .");

    safe_area =
        create_room
        ("Type 'leave' to leave the safe room and enter the game.");
    explosive_area = create_room("Explosives Depot.");
    god_general_area = create_room("The General God Room");
    satori_area = create_room("Satori's Zen Garden");
    store_area = create_room("Mob Storage Area");
    graveyard_area = create_room("The Graveyard");
    deploy_area = create_room("Supply Deployment Area");

    for ( level = the_city; level; level = level->level_down )
        randomize_level(level);
}

/* returns info on the common area where 2 levels correspond */
void
find_level_commonality (LEVEL_DATA * first_level,
                       LEVEL_DATA * second_level, int *x_size,
                       int *y_size, int *lev1_lower_x,
                       int *lev1_lower_y, int *lev2_lower_x,
                       int *lev2_lower_y)
{
    int x_dist_first, x_dist_second, y_dist_first, y_dist_second;

    /* use x/y level referrences to determine the amount of area the level
       has with its up and down neighbors and then calculate the number of up
       and down exits there should be */
    if ((*lev1_lower_x =
         first_level->reference_x - second_level->reference_x) >= 0)
        /* left limit of second level has corresponding square on first level */
        *lev2_lower_x = 0;
    else
    {
        *lev1_lower_x = 0;
        *lev2_lower_x =
            second_level->reference_x - first_level->reference_x;
    }

    if ((*lev1_lower_y =
         first_level->reference_y - second_level->reference_y) >= 0)
        /* bottom limit of second level has corresponding square on first level */
        *lev2_lower_y = 0;
    else
    {
        *lev1_lower_y = 0;
        *lev2_lower_y =
            second_level->reference_y - first_level->reference_y;
    }

    x_dist_first = (first_level->x_size - 1) - first_level->reference_x;
    x_dist_second = (second_level->x_size - 1) - second_level->reference_x;
    y_dist_first = (first_level->y_size - 1) - first_level->reference_y;
    y_dist_second = (second_level->y_size - 1) - second_level->reference_y;
    *x_size = (first_level->reference_x - *lev1_lower_x) +
        ((x_dist_first > x_dist_second) ? x_dist_second : x_dist_first);
    *y_size = (first_level->reference_y - *lev1_lower_y) +
        ((y_dist_first > y_dist_second) ? y_dist_second : y_dist_first);
}

void
mark_connected (ROOM_DATA * a_room)
{
    int count;

    SET_BIT(a_room->room_flags, ROOM_CONNECTED);
    for ( count = 0; count < 4; count++ )
        if ( !IS_SET(a_room->exit[count], EX_ISWALL ) &&
            !IS_SET((get_to_room(a_room, count))->room_flags,
                    ROOM_CONNECTED))
            mark_connected(get_to_room(a_room, count));
}

void
connect_level (LEVEL_DATA * a_level)
{
    int x, y, topx, topy, dir;
    ROOM_DATA *curr_room, *to_room;
    const sh_int rev_dir[] = {
        2, 3, 0, 1, 5, 4
    };

    mark_connected(a_level->rooms_on_level);
    topx = a_level->x_size - 2;
    topy = a_level->y_size - 2;
    for ( x = 1; x <= topx; x++ )
        for ( y = 1; y <= topy; y++ )
            if (!IS_SET((curr_room = to_room =
                         index_room(a_level->rooms_on_level, x,
                                    y))->room_flags, ROOM_CONNECTED))
            {
                do
                {
                    curr_room = to_room;
                    dir = number_range(0, 3);
                    to_room = get_to_room(curr_room, dir);
                    REMOVE_BIT(curr_room->exit[dir], EX_ISWALL);
                    REMOVE_BIT(curr_room->exit[dir], EX_ISNOBREAKWALL);
                    REMOVE_BIT(to_room->exit[rev_dir[dir]], EX_ISWALL);
                    REMOVE_BIT(to_room->exit[rev_dir[dir]],
                               EX_ISNOBREAKWALL);
                }
                while ( !IS_SET(to_room->room_flags, ROOM_CONNECTED) );
                mark_connected(curr_room);
            }
    /* just to make sure */
    logmesg("Disconnected:");
    for ( x = 1; x <= topx; x++ )
        for ( y = 1; y <= topy; y++ )
            if (!IS_SET
                ((index_room(a_level->rooms_on_level, x, y))->room_flags,
                 ROOM_CONNECTED))
            {
                sprintf(log_buf, "%d, %d", x, y);
                logmesg(log_buf);
            }
}

void
randomize_level (LEVEL_DATA * a_level)
{
    const sh_int rev_dir[] = {
        2, 3, 0, 1, 5, 4
    };

    ROOM_DATA *temp_room;
    sh_int count, x, y, topx, topy, the_exit;
    sh_int num_up_exits = 0, num_down_exits = 0;
    int x_area, y_area, lev_x_lower, lev_y_lower, down_x_lower,
        down_y_lower;
    int up_x_lower, up_y_lower;

    logmesg("Randomizing cardinal direction exits.");
    topx = a_level->x_size - 2;
    topy = a_level->y_size - 2;
    for ( count = 0; count < (topx * topy * 3) / 4; count++ )
    {
        x = number_range(1, topx);
        y = number_range(1, topy);
        the_exit = number_range(0, 3);
        temp_room = index_room(a_level->rooms_on_level, x, y);
        SET_BIT(temp_room->exit[the_exit], EX_ISWALL);
        temp_room = get_to_room(temp_room, the_exit);
        SET_BIT(temp_room->exit[rev_dir[the_exit]], EX_ISWALL);
    }

    logmesg("Guaranteeing level connectivity.");
    connect_level(a_level);

    logmesg("Randomizing vertical exits.");

    if ( a_level->level_down )
    {
        find_level_commonality(a_level, a_level->level_down, &x_area,
                               &y_area, &lev_x_lower, &lev_y_lower,
                               &down_x_lower, &down_y_lower);
        num_down_exits = x_area * y_area / (5 * 5);
        if ( num_down_exits < 1 )
            num_down_exits = 1;
        logmesg("There will be %d down exits on level %d in the range "
                   "(%d - %d, %d - %d)", num_down_exits,
                   a_level->level_number, lev_x_lower,
                   lev_x_lower + x_area, lev_y_lower,
                   lev_y_lower + y_area);
    }
    else
        logmesg("no down exits are possible on this level.");

    for ( count = 0; count < num_down_exits; count++ )
    {
        do
        {
            temp_room = index_room(a_level->rooms_on_level,
                                   x =
                                   number_range(lev_x_lower,
                                                lev_x_lower + x_area), y =
                                   number_range(lev_y_lower,
                                                lev_y_lower + y_area));
        }
        while ( !IS_SET(temp_room->exit[DIR_DOWN], EX_ISWALL) );
        logmesg("down added to %d, %d, %d", temp_room->x, temp_room->y,
                   temp_room->level);
        temp_room->exit[DIR_DOWN] = 0;
    }

    if ( a_level->level_up )
    {
        find_level_commonality(a_level, a_level->level_up, &x_area,
                               &y_area, &lev_x_lower, &lev_y_lower,
                               &up_x_lower, &up_y_lower);
        num_up_exits = x_area * y_area / (5 * 5);
        if ( num_up_exits < 1 )
            num_up_exits = 1;
        logmesg("There will be %d up exits on level %d in the range "
                   "(%d - %d, %d - %d)", num_up_exits,
                   a_level->level_number, lev_x_lower,
                   lev_x_lower + x_area, lev_y_lower,
                   lev_y_lower + y_area);
    }
    else
        logmesg("no up exits are possible on this level.");

    for ( count = 0; count < num_up_exits; count++ )
    {
        do
        {
            temp_room = index_room(a_level->rooms_on_level,
                                   x =
                                   number_range(lev_x_lower,
                                                lev_x_lower + x_area), y =
                                   number_range(lev_y_lower,
                                                lev_y_lower + y_area));
        }
        while ( !IS_SET(temp_room->exit[DIR_UP], EX_ISWALL) );
        logmesg("up added to %d, %d, %d", temp_room->x, temp_room->y,
                   temp_room->level);
        temp_room->exit[DIR_UP] = 0;
    }
}

/*
 * Snarf a help section.
 */
void
load_helps (FILE * fp)
{
    HELP_DATA *pHelp;

    logmesg("Loading helps.");
    for ( ;; )
    {
        pHelp = alloc_perm(sizeof(*pHelp));
        pHelp->level = fread_number(fp);
        pHelp->keyword = fread_string(fp);
        if ( pHelp->keyword[0] == '$' )
            break;
        pHelp->text = fread_string(fp);

        if ( !str_cmp(pHelp->keyword, "greeting1") )
            help_greeting1 = pHelp->text;
        if ( !str_cmp(pHelp->keyword, "\"game over\"") )
            help_gameover = pHelp->text;

        if ( help_first == NULL )
            help_first = pHelp;
        if ( help_last != NULL )
            help_last->next = pHelp;

        help_last = pHelp;
        pHelp->next = NULL;
        top_help++;
    }
    logmesg("Helps have been read.");
    return;
}


void
load_outfit (FILE * fp)
{
    char line[256], *ptr;
    long ourPosition;
    char buf[256];
    int idx;

    g_numOutfits = 0;
    ourPosition = ftell(fp);

    while ( fgets(line, 256, fp) && strcmp(line, "$\n") )
    {
        ptr = line;
        while ( isspace(*ptr) )
            ptr++;
        if ( *ptr && *ptr != '*' )
            g_numOutfits++;
    }

    if ( strcmp(line, "$\n") )
    {
        logmesg("Unexpected EOF while loading outfits");
        exit(STATUS_ERROR);
    }

    fseek(fp, ourPosition, SEEK_SET);
    g_outfitTable = alloc_perm(sizeof(OUTFIT_DATA) * g_numOutfits);

    for ( idx = 0; !feof(fp); )
    {
        ptr = fgets(line, 256, fp);
        *(line + strlen(line) - 1) = '\0';

        ptr = one_argument(ptr, buf);

        if ( !*buf || *buf == '*' )
            continue;
        else if ( *buf == '$' )
            break;

        g_outfitTable[idx].team = lookup_team(buf);
        ptr = one_argument(ptr, buf);
        g_outfitTable[idx].vnum = atoi(buf);
        ptr = one_argument(ptr, buf);
        g_outfitTable[idx].quantity = atoi(buf);
        ptr = one_argument(ptr, buf);
        g_outfitTable[idx].leader_only = !str_prefix(buf, "leader");

        idx++;
    }
}


void
load_teams (FILE * fp)
{
    long ourPosition;
    char line[256];
    char buf[256];
    char *ptr;
    int i;

    /* Initialise number of team records and file position. */
    g_numTeams = 0;
    ourPosition = ftell(fp);

    /* Count the number of entries in the section. */
    while ( fgets(line, 256, fp) && *line != '$' )
    {
        ptr = line;
        while ( isspace(*ptr) )
            ptr++;
        if ( *ptr != '*' && *ptr )
            g_numTeams++;
    }

    if ( *line != '$' )
    {
        logmesg("Unexpected EOF while loading teams.");
        exit(STATUS_ERROR);
    }

    /* Rewind the file to ourPosition and allocate the team table. */
    fseek(fp, ourPosition, SEEK_SET);
    team_table = alloc_perm(sizeof(struct team_type) * g_numTeams);

    for ( i = 0; !feof(fp); )
    {
        ptr = fgets(line, 256, fp);
        *(line + strlen(line) - 1) = '\0';
        ptr = one_argument(ptr, buf);

        if ( !*buf || *buf == '*' )
            continue;
        else if ( *buf == '$' )
            break;

        team_table[i].score = 0;
        team_table[i].players = 0;
        team_table[i].independent = FALSE;
        team_table[i].active = FALSE;
        team_table[i].teamleader = NULL;
        team_table[i].hq = NULL;
        team_table[i].map_memory = NULL;

        team_table[i].name = str_dup(buf);
        ptr = one_argument_pcase(ptr, buf);
        team_table[i].who_name = str_dup(buf);
        ptr = one_argument_pcase(ptr, buf);
        team_table[i].namecolor = str_dup(buf);
        ptr = one_argument(ptr, buf);
        team_table[i].x_mult = atoi(buf);
        ptr = one_argument(ptr, buf);
        team_table[i].y_mult = atoi(buf);

        one_argument(ptr, buf);

        if ( *buf )
        {
            team_table[i].active = !str_prefix(buf, "active");
            team_table[i].independent = !str_prefix(buf, "independent");
        }

        if ( !str_cmp(team_table[i].name, "solo") )
            TEAM_NONE = i;
        else if ( !str_cmp(team_table[i].name, "traitor") )
            TEAM_TRAITOR = i;
        else if ( !str_cmp(team_table[i].name, "mob") )
            TEAM_MOB = i;
        else if ( !str_cmp(team_table[i].name, "admin") )
            TEAM_ADMIN = i;

        i++;
    }

    if ( TEAM_MOB == -1 )
    {
        if ( TEAM_NONE != -1 )
            TEAM_MOB = TEAM_NONE;
        else if ( TEAM_ADMIN != -1 )
            TEAM_MOB = TEAM_ADMIN;
        else
        {
            logmesg("No mobile, solo, or admin teams specified.");
            exit(1);
        }
    }

    if ( TEAM_ADMIN == -1 )
    {
        if ( TEAM_NONE != -1 )
            TEAM_ADMIN = TEAM_NONE;
        else if ( TEAM_MOB != -1 )
            TEAM_ADMIN = TEAM_MOB;
        else
        {
            logmesg("No mobile, solo, or admin teams specified.");
            exit(1);
        }
    }

    if ( TEAM_TRAITOR == -1 )
    {
        logmesg("No traitor team specified.");
        exit(1);
    }
}


/*
 * Snarf notes file.
 */
void
load_notes (void)
{
    FILE *fp;
    NOTE_DATA *pnotelast;
    char *ptr;

    if ( (fp = fopen(NOTE_FILE, "r")) == NULL )
        return;

    pnotelast = NULL;

    for ( ;; )
    {
        NOTE_DATA *pnote;
        char letter;

        do
        {
            letter = getc(fp);
            if ( feof(fp) )
            {
                fclose(fp);
                return;
            }
        }
        while ( isspace(letter) );
        ungetc(letter, fp);

        pnote = alloc_perm(sizeof(*pnote));

        if ( str_cmp(fread_word(fp), "sender") )
            break;
        pnote->sender = fread_string(fp);

        if ( str_cmp(fread_word(fp), "date") )
            break;
        pnote->date = fread_string(fp);

        if ( str_cmp(fread_word(fp), "stamp") )
            break;
        pnote->date_stamp = fread_number(fp);

        ptr = fread_word(fp);

        if ( !str_cmp(ptr, "flags") )
        {
            pnote->flags = fread_number(fp);
            ptr = fread_word(fp);
        }
        else
            pnote->flags = 0L;

        if ( str_cmp(ptr, "to") )
            break;
        pnote->to_list = fread_string(fp);

        if ( str_cmp(fread_word(fp), "subject") )
            break;
        pnote->subject = fread_string(fp);

        if ( str_cmp(fread_word(fp), "text") )
            break;
        pnote->text = fread_string(fp);
        pnote->next = NULL;

        if ( pnote->date_stamp < current_time - (7 * 24 * 60 * 60 )       /* 1 week */
            && !IS_SET(pnote->flags, PNOTE_NODEL))
        {
            free_string(pnote->text);
            free_string(pnote->subject);
            free_string(pnote->to_list);
            free_string(pnote->date);
            free_string(pnote->sender);
            pnote->flags = 0L;
            pnote->next = note_free;
            note_free = pnote;
            pnote = NULL;
            continue;
        }

        if ( note_list == NULL )
            note_list = pnote;
        else
            pnotelast->next = pnote;

        pnotelast = pnote;
    }

    strcpy(strArea, NOTE_FILE);
    fp = fp;
    logmesg("Load_notes: bad key word.");
    exit(STATUS_ERROR);
    return;
}


/*
 * Create an instance of an object.
 */
OBJ_DATA *
create_object (OBJ_INDEX_DATA * pObjIndex, int level)
{
    static OBJ_DATA obj_zero;
    static ROOM_DATA room_zero;
    OBJ_DATA *obj;
    sh_int count;

    if ( pObjIndex == NULL )
    {
        logmesg("Create_object: NULL pObjIndex.");
        exit(STATUS_ERROR);
    }

    if ( obj_free == NULL )
    {
        obj = alloc_perm(sizeof(*obj));
    }
    else
    {
        obj = obj_free;
        obj_free = obj_free->next;
    }

    *obj = obj_zero;
    obj->pIndexData = pObjIndex;
    obj->in_room = NULL;

    obj->wear_loc = -1;

    obj->name = pObjIndex->name;
    obj->short_descr = pObjIndex->short_descr;
    obj->description = pObjIndex->description;
    obj->explode_desc = pObjIndex->explode_desc;
    obj->extract_flags = pObjIndex->extract_flags;
    obj->general_flags = pObjIndex->general_flags;
    obj->usage_flags = pObjIndex->usage_flags;
    obj->wear_flags = pObjIndex->wear_flags;
    obj->weight = pObjIndex->weight;
    obj->item_type = pObjIndex->item_type;
    obj->num_extract_explode = pObjIndex->num_extract_explode;
    obj->num_extract_burn = pObjIndex->num_extract_burn;
    obj->cooked_timer = EXP_SECONDS;

    obj->destination = NULL;
    obj->arrival_time = -1;

    if ( IS_SET(obj->general_flags, GEN_DEPLOYMENT) && level )
    {
        obj->arrival_time = pObjIndex->arrival_time;
        obj->destination = find_location(NULL, pObjIndex->destination);
        obj_to_room(obj, deploy_area);
    }

    if ( obj->item_type == ITEM_TEAM_VEHICLE )
    {
        obj->vehicle = alloc_mem(sizeof(struct obj_vehicle_data));
                
        obj->vehicle->tomob = pObjIndex->vehicle->tomob;
        obj->vehicle->positions = pObjIndex->vehicle->positions;
        obj->vehicle->r_positions = pObjIndex->vehicle->r_positions;
        obj->vehicle->start_message_dp = pObjIndex->vehicle->start_message_dp;
        obj->vehicle->start_message_do = pObjIndex->vehicle->start_message_do;
        obj->vehicle->start_message_io = pObjIndex->vehicle->start_message_io;
        obj->vehicle->stop_message_dp = pObjIndex->vehicle->stop_message_dp;
        obj->vehicle->stop_message_do = pObjIndex->vehicle->stop_message_do;
        obj->vehicle->stop_message_io = pObjIndex->vehicle->stop_message_io;
        obj->vehicle->interior_desc = pObjIndex->vehicle->interior_desc;
        obj->vehicle->capacity = pObjIndex->vehicle->capacity;
        obj->vehicle->turret_high = pObjIndex->vehicle->turret_high;
        obj->vehicle->turret_low = pObjIndex->vehicle->turret_low;
    }

    obj->next = object_list;
    obj->range = pObjIndex->range;
    obj->owner = NULL;
    obj->ammo = pObjIndex->ammo;
    obj->ammo_type = pObjIndex->ammo_type;
    obj->burn_time = pObjIndex->burn_time;
    obj->choke_time = pObjIndex->choke_time;
    obj->rounds_per_second = pObjIndex->rounds_per_second;
    obj->rounds_delay = pObjIndex->rounds_delay;
    obj->armor = pObjIndex->armor;
    obj->wait_time = pObjIndex->rounds_delay;
    obj->hp_char = pObjIndex->hp_char;
    obj->hp_struct = pObjIndex->hp_struct;
    obj->attack_message = pObjIndex->attack_message;
    obj->extract_me = 0;
    obj->valid = VALID_VALUE;
    obj->team = pObjIndex->team;
    obj->scope_type = pObjIndex->scope_type;

    if ( obj->item_type == ITEM_TEAM_VEHICLE ||
         obj->item_type == ITEM_TEAM_ENTRANCE )
    {
        obj->interior = alloc_mem(sizeof(ROOM_DATA));
        *(obj->interior) = room_zero;
        obj->interior->interior_of = obj;

        if ( obj->item_type == ITEM_TEAM_VEHICLE )
            obj->interior->name = str_dup(OBJ_INTERIOR_DESC(obj));

        obj->interior->description = obj->interior->name;
        for ( count = 0; count < 6; count++ )
        {
            SET_BIT(obj->interior->exit[count], EX_ISWALL);
            SET_BIT(obj->interior->exit[count], EX_ISNOBREAKWALL);
        }
        obj->interior->contents = NULL;
        obj->interior->people = NULL;
        obj->interior->mine = NULL;
        obj->interior->this_level = the_city;
        obj->interior->x = obj->interior->y = obj->interior->level = -1;
    }
    else
        obj->interior = NULL;

    obj->dam_mult = pObjIndex->dam_mult;

    for ( count = 0; count < 3; count++ )
    {
        obj->damage_char[count] = pObjIndex->damage_char[count];
        obj->damage_structural[count] =
            pObjIndex->damage_structural[count];
    }

    if ( IS_SET(pObjIndex->general_flags, GEN_LEAVE_TRAIL) ||
         IS_SET(pObjIndex->usage_flags, USE_MAKE_OBJ_ROOM) )
    {
        obj->spawnTrail = get_obj_index(pObjIndex->spawnTrail);
        obj->spawnMax = pObjIndex->spawnMax;
        obj->spawnTimer = pObjIndex->spawnTimer;
    }
    else
    {
        obj->spawnTrail = 0;
        obj->spawnMax = 0;
        obj->spawnTimer = 0;
    }

    object_list = obj;
    pObjIndex->count++;

    return obj;
}

/* duplicate an object exactly -- except contents */
void
clone_object (OBJ_DATA * parent, OBJ_DATA * clone)
{
    int count;

/*    EXTRA_DESCR_DATA *ed,*ed_new; */

    if ( parent == NULL || clone == NULL )
        return;

    /* start fixing the object */
    clone->name = str_dup(parent->name);
    clone->short_descr = str_dup(parent->short_descr);
    clone->description = str_dup(parent->description);
    clone->item_type = parent->item_type;
    clone->extract_flags = parent->extract_flags;
    clone->num_extract_explode = parent->num_extract_explode;
    clone->num_extract_burn = parent->num_extract_burn;
    clone->general_flags = parent->general_flags;
    clone->usage_flags = parent->usage_flags;
    clone->wear_flags = parent->wear_flags;
    clone->weight = parent->weight;
    clone->timer = parent->timer;
    clone->cooked_timer = parent->cooked_timer;
    clone->owner = parent->owner;
    clone->range = parent->range;
    clone->dam_mult = parent->dam_mult;
    clone->team = parent->team;
    clone->generic_counter = 0;
    clone->scope_type = parent->scope_type;

    if ( clone->item_type == ITEM_TEAM_VEHICLE )
    {
        clone->vehicle = alloc_perm(sizeof(struct obj_vehicle_data));
        clone->vehicle->tomob = str_dup(parent->vehicle->tomob);
        clone->vehicle->positions = parent->vehicle->positions;
        clone->vehicle->r_positions = parent->vehicle->r_positions;

        if ( parent->vehicle->start_message_dp )
        {
            clone->vehicle->start_message_dp =
                str_dup(parent->vehicle->start_message_dp);
            clone->vehicle->start_message_do =
                str_dup(parent->vehicle->start_message_do);
            clone->vehicle->start_message_io =
                str_dup(parent->vehicle->start_message_io);
        }
        else
            clone->vehicle->start_message_dp = clone->vehicle->start_message_do =
                clone->vehicle->start_message_io = NULL;

        if ( parent->vehicle->stop_message_dp )
        {
            clone->vehicle->stop_message_dp =
                str_dup(parent->vehicle->stop_message_dp);
            clone->vehicle->stop_message_do =
                str_dup(parent->vehicle->stop_message_do);
            clone->vehicle->stop_message_io =
                str_dup(parent->vehicle->stop_message_io);
        }
        else
            clone->vehicle->stop_message_dp = clone->vehicle->stop_message_do =
                clone->vehicle->stop_message_io = NULL;

        clone->vehicle->interior_desc = str_dup(parent->vehicle->interior_desc);
        clone->vehicle->capacity = parent->vehicle->capacity;
        clone->vehicle->turret_high = parent->vehicle->turret_high;
        clone->vehicle->turret_low = parent->vehicle->turret_low;
    }

    for ( count = 0; count < 3; count++ )
    {
        clone->damage_char[count] = parent->damage_char[count];
        clone->damage_structural[count] = parent->damage_structural[count];
    }
}


void
CopyNpcEvent (struct cmd_queue *dp, const struct cmd_queue *sp)
{
    struct cmd_node *src = sp->head;

    while ( src != NULL )
    {
        EnqueueNpcEvent(dp, str_dup(src->cmd));
        src = src->next;
    }
}


CHAR_DATA *
clone_mobile (CHAR_DATA * ch, int dup)
{
    CHAR_DATA *clone;
    OBJ_DATA *tracker;
    OBJ_DATA *obj, *obj2;
    OBJ_DATA *iter;

    clone = alloc_char();
    *clone = *ch;

    /* Clone strings: */
    clone->names = str_dup(ch->names);
    clone->short_descript = str_dup(ch->short_descript);

    if ( ch->where_start )
        clone->where_start = str_dup(ch->where_start);
    else
        clone->where_start = str_dup("god-store");

    if ( ch->npc )
    {
        clone->npc = calloc(1, sizeof(struct npc_data));
        CopyNpcEvent(&clone->npc->birth, &ch->npc->birth);
        CopyNpcEvent(&clone->npc->heartbeat, &ch->npc->heartbeat);
        CopyNpcEvent(&clone->npc->depress, &ch->npc->depress);
        CopyNpcEvent(&clone->npc->enforcer, &ch->npc->enforcer);
        CopyNpcEvent(&clone->npc->death, &ch->npc->death);
        CopyNpcEvent(&clone->npc->kill, &ch->npc->kill);
    }

    /* duplicate inventory */
    clone->carrying = NULL;
    clone->where_start = NULL;
    clone->owned_objs = NULL;
    clone->next_extract = NULL;

    char_to_room(clone, ch->in_room);

    for ( tracker = ch->carrying; tracker; tracker = tracker->next_content )
    {
        obj = create_object(tracker->pIndexData, 0);
        obj_to_char(obj, clone);
        obj->extract_me = !dup;

        for ( iter = tracker->contains; iter; iter = iter->next_content )
        {
            obj2 = create_object(iter->pIndexData, 0);
            obj_to_obj(obj2, obj);
            obj2->extract_me = !dup;
        }
    }

    if ( clone->npc->birth.head )
    {
        struct cmd_node *node = clone->npc->birth.head;
        char *hold;

        while ( node != NULL )
        {
            hold = npc_parse(node->cmd, clone, NULL, NULL, NULL);
            if ( !hold )
                continue;
            interpret(clone, hold);
            node = node->next;
        }
    }
    else
    {
        do_wear(clone, "all");
        do_load(clone, "");
    }

    clone->next = char_list;
    char_list = clone;
    return clone;
}

/*
 * Clear a new character.
 */
void
clear_char (CHAR_DATA * ch)
{
    static CHAR_DATA ch_zero;

    *ch = ch_zero;
    ch->logon = current_time;
    ch->position = POS_STANDING;
    ch->hit = 1;
    ch->max_hit = 3000;
}



/*
 * Translates mob virtual number to its obj index struct.
 * Hash table lookup.
 */
OBJ_INDEX_DATA *
get_obj_index (int vnum)
{
    if (vnum <= 0 || vnum >= top_obj_index ||
        all_objects[vnum - 1].vnum != vnum)
    {
        if ( fBootDb )
        {
            logmesg("Get_obj_index: bad vnum %d.", vnum);
            exit(STATUS_ERROR);
        }
        return NULL;
    }

    return &(all_objects[vnum - 1]);
}

/*
 * Read a letter from a file.
 */
char
fread_letter (FILE * fp)
{
    char c;

    do
    {
        c = getc(fp);
    }
    while ( isspace(c) );

    return c;
}



/*
 * Read a number from a file.
 */
int
fread_number (FILE * fp)
{
    int number;
    bool sign;
    char c;

    do
    {
        c = getc(fp);
    }
    while ( isspace(c) );

    number = 0;

    sign = FALSE;
    if ( c == '+' )
    {
        c = getc(fp);
    }
    else if ( c == '-' )
    {
        sign = TRUE;
        c = getc(fp);
    }

    if ( !isdigit(c) )
    {
        char line[256];

        logmesg("Fread_number: bad format at byte %d", ftell(fp));
        logmesg("Line is: %s", fgets(line, 255, fp));
        exit(STATUS_ERROR);
    }

    while ( isdigit(c) )
    {
        number = number * 10 + c - '0';
        c = getc(fp);
    }

    if ( sign )
        number = 0 - number;

    if ( c == '|' )
        number += fread_number(fp);
    else if ( c != ' ' )
        ungetc(c, fp);

    return number;
}


long
fread_flag (FILE * fp)
{
    int number;
    char c;

    do
    {
        c = getc(fp);
    }
    while ( isspace(c) );

    number = 0;

    if ( !isdigit(c) )
    {
        while ( ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') )
        {
            number |= flag_convert(c);
            c = getc(fp);
        }
    }

    if ( c == '|' )
        number |= fread_flag(fp);

    while ( isdigit(c) )
    {
        number = number * 10 + c - '0';
        c = getc(fp);
    }

    if ( c == '|' )
        number |= fread_flag(fp);

    else if ( c != ' ' )
        ungetc(c, fp);

    return number;
}


long
flag_convert (char letter)
{
    long bitsum = 0;
    char i;

    if ( 'A' <= letter && letter <= 'Z' )
    {
        bitsum = 1;
        for ( i = letter; i > 'A'; i-- )
            bitsum *= 2;
    }
    else if ( 'a' <= letter && letter <= 'z' )
    {
        bitsum = (1 << 26);
        for ( i = letter; i > 'a'; i-- )
            bitsum *= 2;
    }

    return bitsum;
}


char *
escape_qstring (const char *qs)
{
    static char temp[65537];
    char *wptr = temp;

    if ( isspace(*qs) )
        *(wptr++) = '"';

    for ( ; *qs; qs++ )
        switch (*qs)
        {
            case '\a':
                *(wptr++) = '\\';
                *(wptr++) = 'a';
                break;
            case '\b':
                *(wptr++) = '\\';
                *(wptr++) = 'b';
                break;
            case '\f':
                *(wptr++) = '\\';
                *(wptr++) = 'f';
                break;
            case '\n':
                *(wptr++) = '\\';
                *(wptr++) = 'n';
                break;
            case '\r':
                *(wptr++) = '\\';
                *(wptr++) = 'r';
                break;
            case '\t':
                *(wptr++) = '\\';
                *(wptr++) = 't';
                break;
            case '\v':
                *(wptr++) = '\\';
                *(wptr++) = 'v';
                break;
            case '\x1B':
                *(wptr++) = '\\';
                *(wptr++) = 'e';
                break;
            case '\'':
                *(wptr++) = '\\';
                *(wptr++) = '\'';
                break;
            case '"':
                *(wptr++) = '\\';
                *(wptr++) = '"';
                break;
            case '\\':
                *(wptr++) = '\\';
                *(wptr++) = '\\';
                break;
            default:
                *(wptr++) = *qs;
                break;
        }

    if ( temp[0] == '"' )
        *(wptr++) = '"';

    *wptr = '\0';
    return (temp);
}


char *
fread_qstring (FILE * fp)
{
    char delim = '\n';
    char temp[32769];           /* 32768+1 because \n->\r\n expansion */
    char *topTemp;
    char *ptr;
    char c;

    topTemp = temp + 32766;     /* &temp[32767-1]; */
    ptr = temp;

    do
        c = fgetc(fp);
    while ( isspace(c) );

    if ( c == '"' || c == '\'' )
        delim = c;
    else
        ungetc(c, fp);

    while ( (c = fgetc(fp)) != EOF && c != delim && ptr < topTemp )
    {
        if ( c == '\\' )
        {
            switch ((c = fgetc(fp)))
            {
                case 'a':
                    *(ptr++) = '\a';
                    break;
                case 'b':
                    *(ptr++) = '\b';
                    break;
                case 'e':
                    *(ptr++) = '\x1B';
                    break;
                case 'f':
                    *(ptr++) = '\f';
                    break;
                case 'n':
                    *(ptr++) = '\n';
                    break;
                case 'r':
                    *(ptr++) = '\r';
                    break;
                case 't':
                    *(ptr++) = '\t';
                    break;
                case 'v':
                    *(ptr++) = '\v';
                    break;
                case '"':
                    *(ptr++) = '"';
                    break;
                case '\'':
                    *(ptr++) = '\'';
                    break;
                case '\n':
                    break;
                case '\\':
                    *(ptr++) = '\\';
                    break;
                default:
                    ungetc(c, fp);
                    break;
            }
            continue;
        }

        if ( c == '\n' )
        {                       /* This must be a quote terminated string. */
            *(ptr++) = '\r';
            *(ptr++) = '\n';
        }
        else
            *(ptr++) = c;
    }

    if ( c == EOF )
    {
        logmesg("Unterminated string beginning with \"%.24s\"", temp);
        exit(1);
    }
    else if ( ptr == topTemp )
    {
        logmesg
            ("Unterminated or too long string beginning with \"%.24s\"",
             temp);
        exit(1);
    }

    *ptr = '\0';

    /* Now store the string in memory. */
    if ( !*temp )
        return (str_empty + 0);

    ptr = top_string;
    top_string += strlen(temp) + 1;

    if ( top_string - string_space > MAX_STRING )
    {
        logmesg("REALBUG: Out of string memory.");
        exit(1);
    }

    strcpy(ptr, temp);
    return (ptr);
}


char *
fread_string (FILE * fp)
{
    char temp[32000], *thestring;
    int count = 1;

    temp[0] = getc(fp);

    while ( isspace(temp[0]) )
        temp[0] = getc(fp);

    if ( temp[0] != '~' )
    {
        while ( (count < 32000) && ((temp[count++] = getc(fp)) != '~') )
            if ( temp[count - 1] == '\n' )
            {
                temp[count++ - 1] = '\r';
                temp[count - 1] = '\n';
            }
            else if ( temp[count - 1] == '\r' )
                count--;

        temp[count - 1] = '\0';
        if ( count >= 20000 )
            logmesg("REALBUG: Unterminated string found in "
                       "fread_string.");
        thestring = top_string;
        top_string = &(top_string[strlen(temp) + 1]);
        if ( top_string > string_space + MAX_STRING )
        {
            logmesg("REALBUG: Out of string memory.");
            exit(STATUS_ERROR);
        }
        strcpy(thestring, temp);
        return thestring;
    }
    else
        return &str_empty[0];
}

char *
fread_string_eol (FILE * fp)
{
    char temp[32000], *thestring;
    int count = 1;

    temp[0] = getc(fp);

    while ( isspace(temp[0]) )
        temp[0] = getc(fp);

    if ( temp[0] != '~' )
    {
        while ( (count < 32000) && ((temp[count++] = getc(fp)) != '\n') )
            if ( temp[count - 1] == '\n' )
            {
                temp[count++ - 1] = '\r';
                temp[count - 1] = '\n';
            }
            else if ( temp[count - 1] == '\r' )
                count--;

        temp[count - 1] = '\0';
        if ( count >= 32000 )
            logmesg("REALBUG: Unterminated string found in "
                       "fread_string.");
        thestring = top_string;
        top_string = &(top_string[strlen(temp) + 1]);
        if ( top_string > string_space + MAX_STRING )
        {
            logmesg("REALBUG: Out of string memory.");
            exit(STATUS_ERROR);
        }
        strcpy(thestring, temp);
        return thestring;
    }
    else
        return &str_empty[0];
}

/*
 * Read to end of line (for comments).
 */
void
fread_to_eol (FILE * fp)
{
    char c;

    do
    {
        c = getc(fp);
    }
    while ( c != '\n' && c != '\r' );

    do
    {
        c = getc(fp);
    }
    while ( c == '\n' || c == '\r' );

    ungetc(c, fp);
    return;
}



/*
 * Read one word (into static buffer).
 */
char *
fread_word (FILE * fp)
{
    static char word[MAX_INPUT_LENGTH];
    char *pword;
    char cEnd;

    do
    {
        cEnd = getc(fp);
    }
    while ( isspace(cEnd) );

    if ( cEnd == '\'' || cEnd == '"' )
    {
        pword = word;
    }
    else
    {
        word[0] = cEnd;
        pword = word + 1;
        cEnd = ' ';
    }

    for ( ; pword < word + MAX_INPUT_LENGTH; pword++ )
    {
        *pword = getc(fp);
        if ( cEnd == ' ' ? isspace(*pword) : *pword == cEnd )
        {
            if ( cEnd == ' ' )
                ungetc(*pword, fp);
            *pword = '\0';
            return word;
        }
    }

    logmesg("Fread_word: word too long.");
    exit(STATUS_ERROR);
    return NULL;
}



void
expand_city ()
{
#define EXPAND_AMOUNT 6
    ROOM_DATA *old_level1;
    int old_size, x, y;
    OBJ_DATA *obj;
    CHAR_DATA *ch;
    ROOM_DATA *old_room;
    int count;

    expand_event = 1;

    logmesg("EXPANDING CITY FROM %d, %d",
            the_city->x_size, the_city->y_size);

    old_level1 = the_city->rooms_on_level;
    old_size = the_city->x_size * the_city->y_size * sizeof(ROOM_DATA);
    the_city->x_size += EXPAND_AMOUNT;
    the_city->y_size += EXPAND_AMOUNT;
    the_city->rooms_on_level =
        alloc_mem(the_city->x_size * the_city->y_size * sizeof(ROOM_DATA));

    /* room descriptions */
    clear_level(the_city->rooms_on_level, the_city->x_size,
                the_city->y_size, the_city);
    set_descs_level(the_city->rooms_on_level, the_city->x_size,
                    the_city->y_size, the_city);

    /* walls */
    set_walls_level(the_city->rooms_on_level, the_city->x_size,
                    the_city->y_size);

    /* we add more downs to make them more plentiful with the bigger grid */
    the_city->reference_x += EXPAND_AMOUNT / 2;
    the_city->reference_y += EXPAND_AMOUNT / 2;

    randomize_level(the_city);

    /* down exits */
    for ( x = 0; x < the_city->x_size - EXPAND_AMOUNT; x++ )
        for ( y = 0; y < the_city->y_size - EXPAND_AMOUNT; y++ )
        {
            /* this is a dirty hack, but it preserves the call to index_room */
            /* (index_room relies on old_level1->this_level which points to
               the_city) */

            the_city->x_size -= EXPAND_AMOUNT;
            the_city->y_size -= EXPAND_AMOUNT;

            old_room = index_room(old_level1, x, y);

            /* Fix it so that expansions don't remember bad information. */
            for ( int team = 0; team < g_numTeams; team++ )
                UnsetSquareFeature(team, x, y, SQRF_DOWN | SQRF_DEPOT);

            the_city->x_size += EXPAND_AMOUNT;
            the_city->y_size += EXPAND_AMOUNT;
            if ( !old_room->exit[DIR_DOWN] )
            {
                logmesg("copying down exit for %d, %d", x, y);
                (index_room
                 (the_city->rooms_on_level, x + EXPAND_AMOUNT / 2,
                  y + EXPAND_AMOUNT / 2))->exit[DIR_DOWN] = 0;
            }
        }

    /* move objects from old level to new level */
    for ( obj = object_list; obj; obj = obj->next )
    {
        if ( !obj->in_room )
            if ( !obj->destination )
            {
                if (obj->carried_by || obj->in_obj ||
                    obj->item_type == ITEM_TEAM_ENTRANCE)
                    continue;
                logmesg("lost object detected: vnum (%d), short (%s)",
                           obj->vnum, obj->short_descr);
                continue;
            }
            else
            {
                x = obj->destination->x + EXPAND_AMOUNT / 2;
                y = obj->destination->y + EXPAND_AMOUNT / 2;

                if ( obj->destination->mine == obj )
                {
                    (index_room(the_city->rooms_on_level, x, y))->mine =
                        obj;
                    obj->destination->mine = NULL;
                }
                obj->destination =
                    index_room(the_city->rooms_on_level, x, y);
            }
        else
        {
            if ( obj->in_room->level )
                continue;

            if ( !obj->in_room->x )
                x = 0;
            else
                x = (obj->in_room->x ==
                     the_city->x_size - EXPAND_AMOUNT -
                     1) ? the_city->x_size - 1 : obj->in_room->x +
                    EXPAND_AMOUNT / 2;
            if ( !obj->in_room->y )
                y = 0;
            else
                y = (obj->in_room->y ==
                     the_city->y_size - EXPAND_AMOUNT -
                     1) ? the_city->y_size - 1 : obj->in_room->y +
                    EXPAND_AMOUNT / 2;

            obj_from_room(obj);
            obj_to_room(obj, index_room(the_city->rooms_on_level, x, y));
        }
    }

    /* move mobs from old level to new level */
    for ( ch = char_list; ch; ch = ch->next )
    {
        if ( ch->in_room->level )
            continue;

        if ( !ch->in_room->x )
            x = 0;
        else
            x = (ch->in_room->x == the_city->x_size - EXPAND_AMOUNT - 1) ?
                the_city->x_size - 1 : ch->in_room->x + EXPAND_AMOUNT / 2;
        if ( !ch->in_room->y )
            y = 0;
        else
            y = (ch->in_room->y == the_city->y_size - EXPAND_AMOUNT - 1) ?
                the_city->y_size - 1 : ch->in_room->y + EXPAND_AMOUNT / 2;

        char_from_room(ch);
        char_to_room(ch, index_room(the_city->rooms_on_level, x, y));
    }
    for ( count = 0; count < 3; count++ )
        ammo_repop[count] = index_room(the_city->rooms_on_level,
                                       ammo_repop[count]->x +
                                       EXPAND_AMOUNT / 2,
                                       ammo_repop[count]->y +
                                       EXPAND_AMOUNT / 2);
    free_mem(old_level1, old_size);
    logmesg("EXPANSION COMPLETE");
    expand_event = 0;
}

void
do_status (CHAR_DATA * ch, char *argument)
{
    return;
}

/*
 * This function is here to aid in debugging.
 * If the last expression in a function is another function call,
 *   gcc likes to generate a JMP instead of a CALL.
 * This is called "tail chaining."
 * It hoses the debugger call stack for that call.
 * So I make this the last call in certain critical functions,
 *   where I really need the call stack to be right for debugging!
 *
 * If you don't understand this, then LEAVE IT ALONE.
 * Don't remove any calls to tail_chain anywhere.
 *
 * -- Furey
 */
void
tail_chain (void)
{
    return;
}


void
load_cmd_helps (FILE * fp)
{
    bool cEOF = FALSE;
    long savepos;
    char *buf;
    int i;

    savepos = ftell(fp);

    while ( !feof(fp) )
    {
        buf = fread_string(fp);

        if ( !strcmp(buf, "$") )
        {
            cEOF = TRUE;
            break;
        }
        else if ( feof(fp) || !(buf = fread_string(fp)) )
        {
            logmesg("Unexpected EOF in command helps.");
            exit(1);
        }
        else if ( !strcmp(buf, "$") )
        {
            logmesg
                ("Command helps has bad record (got $ for synopsis).");
            exit(1);
        }

        g_noCmdSynopses++;
    }

    if ( !g_noCmdSynopses )
    {
        logmesg("No command synopses available.");
        return;
    }

    fseek(fp, savepos, SEEK_SET);
    g_cmdSynopses =
        alloc_perm(sizeof(struct cmd_synopses_data) * g_noCmdSynopses);

    for ( i = 0; i < g_noCmdSynopses; i++ )
    {
        g_cmdSynopses[i].name = fread_string(fp);
        g_cmdSynopses[i].description = fread_string(fp);
    }

    (void) fread_string(fp);
    logmesg("Loaded %d command synopses...", g_noCmdSynopses);
}
