/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
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

#include <arpa/telnet.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <zlib.h>

#define NOCRYPT

#define STATUS_SHUTDOWN 1
#define STATUS_BOOT_ERROR 2
#define STATUS_ERROR 3
#define STATUS_REBOOT 100

#define CHAR_ATTR_SZ        10
#define OBJ_ATTR_SZ         15
#define ROOM_ATTR_SZ        10

#define TCON_TOK            '&'

#define exit(a) if (fBootDb && ((a) == STATUS_ERROR)) \
                exit (STATUS_BOOT_ERROR); else exit (a)
#define is_digit(a) (((a) >= '0') && ((a) <= '9'))

#define str_prefix(a, b)  strn_cmp((a), (b), strlen(a))
#define str_infix(a, b)   (!strstr(without_colors((b)), (a)))
#define PERCENT(a, b)     ((b) ? (((a) * 100) / (b)) : 0)

#define indirect(inst, off, type) \
    (*(type *)((char *)inst+off))

/*
 * Accommodate old non-Ansi compilers.
 */
#if defined(TRADITIONAL)
#define const
#define args( list )            ( )
#define DECLARE_DO_FUN( fun )       void fun( )
#else
#define args( list )            list
#define DECLARE_DO_FUN( fun )       DO_FUN    fun
#endif

/* system calls */
int unlink();
int system();


/*
 * EXTERIOR SHELL.
 * The idea comes from Visko.  He uses a function for this, but I don't want
 * any overhead for a cleanup.  Basically, it executes the second argument of
 * the function in the exterior of whatever the given character is in (this
 * is useful for "look out" and scan from inside a vehicle, etc.).
 */
#define exterior_shell(_ch, _cmd)                     \
  do {                                                \
    ROOM_DATA *back = _ch->in_room;                   \
    ROOM_DATA *room = back->interior_of               \
                      ? back->interior_of->in_room    \
                      : back->inside_mob->in_room;    \
    char_from_room(_ch);                              \
    char_to_room(_ch, room);                          \
    _cmd;                                             \
    char_from_room(_ch);                              \
    char_to_room(_ch, back);                          \
  } while (0)

#define TRUE true
#define FALSE false

typedef short int sh_int;
typedef unsigned char byte;


/*
 * Structure types.
 */
//typedef struct ban_data BAN_DATA;
typedef struct char_data CHAR_DATA;
typedef struct room_data ROOM_DATA;
typedef struct level_data LEVEL_DATA;
typedef struct descriptor_data DESCRIPTOR_DATA;
typedef struct help_data HELP_DATA;
typedef struct note_data NOTE_DATA;
typedef struct obj_data OBJ_DATA;
typedef struct obj_index_data OBJ_INDEX_DATA;
typedef struct pc_data PC_DATA;
typedef struct time_info_data TIME_INFO_DATA;
typedef struct outfit_data OUTFIT_DATA;
typedef struct god_type GOD_TYPE;
typedef struct top_data TOP_DATA;


/*
 * Function types.
 */
typedef void DO_FUN(CHAR_DATA * ch, char *argument);



/*
 * String and memory management parameters.
 */
#define MAX_KEY_HASH         2048
#define MAX_STRING_LENGTH    4096
#define MAX_INPUT_LENGTH      512
#define PAGELEN            22

#define MAX_INT         32768

#define VALID_VALUE                82   /* some exact number to avoid accidental
                                           validation due to memory corruption */

/*
 * Game parameters.
 * Increase the max'es if you add more of something.
 * Adjust the pulse numbers to suit yourself.
 */
/*#define MAX_SOCIALS         256 */
#define MAX_IN_GROUP           15
#define MAX_TRUST                  10
#define MAX_NUMBER_CARRY           15
#define HIT_POINTS_MORTAL        6666
#define HIT_POINTS_IMMORTAL   1000000
#define LD_TICKS           30

#define MAX_TOPNAME_LEN 24
#define NUM_TOP_KILLS   10
#define NUM_TOP_DEATHS  10
#define NUM_TOP_HOURS   15

#define PULSE_PER_SECOND    4
#define PULSE_PER_MINUTE    (60 * PULSE_PER_SECOND)
#define PULSE_PER_HOUR      (60 * PULSE_PER_MINUTE)

#define RLSEC               * PULSE_PER_SECOND
#define RLMIN               * PULSE_PER_MINUTE
#define RLHOUR              * PULSE_PER_HOUR

#define PULSE_OBJECTS       ( 1 RLSEC)
#define PULSE_VIOLENCE      ( 1 RLSEC)
#define PULSE_TICK          (10 RLSEC)
#define PULSE_SAVE          ( 3 RLMIN)
#define PULSE_BUTTON        ( 1 RLSEC)
#define PULSE_PURGE         (20 RLMIN)
#define PULSE_ENFORCER      ( 1 RLMIN)
#define PULSE_PING          (40 RLSEC)

#define EXP_SECONDS         10  /* Number of seconds after 'pull'. */

#define MECH_DAMAGE_CONST   1000
#define MAX_CHARGE_MECH     5
#define CHARGE_MECH_RATIO   3

/*
 * Site ban structure.
 */
struct time_info_data
{
    int hour;
    int day;
    int month;
    int year;
};

/* Connected state for a channel. */
#define CON_PLAYING                     0
#define CON_RESOLVE_ADDRESS             1
#define CON_GET_ANSI                    2
#define CON_GET_NAME                    3
#define CON_GET_OLD_PASSWORD            4
#define CON_CONFIRM_NEW_NAME            5
#define CON_GET_ADMIN_PASS              6
#define CON_GET_NEW_PASSWORD            7
#define CON_CONFIRM_NEW_PASSWORD        8
#define CON_GET_ADMIN_CHAR_NAME         9
#define CON_GET_NEW_SEX                 10
#define CON_DEFAULT_CHOICE              11
#define CON_SOLO                        12
#define CON_READ_IMOTD                  13
#define CON_READ_MOTD                   14
#define CON_BREAK_CONNECT               15
#define CON_READ_NEWBIE                 16
#define CON_RELOG                       17
#define CON_Q_ISO6429                   18

#define COUNT_WEAPONS                   0
#define COUNT_ARMOR                     1
#define COUNT_EXPLOSIVES                2
#define COUNT_AMMO                      3
#define COUNT_MISC                      4
#define NUM_COUNTS                      5

#define ITEM_MISC                       0
#define ITEM_WEAPON                     1
#define ITEM_ARMOR                      2
#define ITEM_EXPLOSIVE                  3
#define ITEM_AMMO                       4
#define ITEM_TEAM_VEHICLE               5
#define ITEM_TEAM_ENTRANCE              6
#define ITEM_SCOPE                      7

/* ammo types */
#define AMMO_9MM                        0
#define AMMO_GREN_LAUNCHER              6
#define AMMO_FLARE                      13
#define AMMO_TANK_GUN                   14

/* general flags */
#define GEN_DEPRESS                     (A)
#define GEN_CAN_BURY                    (B)
#define GEN_CONDITION_MONITOR           (C)
#define GEN_BURNS_ROOM                  (D)
#define GEN_NO_AMMO_NEEDED              (E)
#define GEN_LEAVE_TRAIL                 (F)
#define GEN_DETECT_MINE                 (G)
#define GEN_ANTI_BLIND                  (H)
#define GEN_DRAG_TO                     (I)     /* XXX: Unimplemented. */
#define GEN_DARKS_ROOM                  (J)
#define GEN_EXTRACT_ON_IMPACT           (K)
#define GEN_SEE_IN_DARK                 (L)
#define GEN_SEE_FAR                     (M)
#define GEN_ANTI_DAZE                   (N)
#define GEN_DEPLOYMENT                  (O)
#define GEN_CHOKES_ROOM                 (P)
#define GEN_IGNORE_ARMOR                (Q)
#define GEN_CAN_PUSH                    (R)
#define GEN_NODEPOT                     (S)
#define GEN_ANTI_POISON                 (T)
#define GEN_SPRAY_ROOM                  (U)     /* XXX: Unimplemented. */
#define GEN_TIMER_ITEM                  (V)
#define GEN_REMOTE_EXTRACTED            (W)     /* XXX: Unimplemented. */
#define GEN_STICKY                      (X)     /* XXX: Unimplemented. */
#define GEN_DIRECTIONAL                 (Y)
#define GEN_COMBUSTABLE                 (Z)
#define GEN_WALK_DAMAGE                 (aa)


/* extract flags */
#define EXTRACT_EXPLODE_ON_EXTRACT      (A)
#define EXTRACT_BLIND_ON_EXTRACT        (B)
#define EXTRACT_BURN_ON_EXTRACT         (C)
#define EXTRACT_STUN_ON_EXTRACT         (D)
#define EXTRACT_DARK_ON_EXTRACT         (E)
#define EXTRACT_EXTINGUISH_ON_EXTRACT   (F)
#define EXTRACT_ANNOUNCE                (G)
#define EXTRACT_CHOKE_ON_EXTRACT        (H)
#define EXTRACT_TELERAN_ON_EXTRACT      (I)
#define EXTRACT_TELELOC_ON_EXTRACT      (J)
#define EXTRACT_MAKE_OBJ_ROOM           (K)

/* usage flags */
#define USE_HEAL                        (A)
#define USE_TANKKIT                     (B)
#define USE_AIRSTRIKE                   (C)
#define USE_EVAC                        (D)
#define USE_MAKE_DROID                  (E)
#define USE_MAKE_SEEKER_DROID           (F)
#define USE_FIRE                        (G)
#define USE_BACKUP                      (H)
#define USE_TURRET                      (I)
#define USE_MAKE_WALL                   (J)
#define USE_HEALTH_MONITOR              (K)
#define USE_BATTLEMAP                   (L)
#define USE_MAKE_OBJ_ROOM               (M)
#define USE_SATELLITE                   (N)

/* search types */
#define SEARCH_ITEM_TYPE                0
#define SEARCH_GEN_FLAG                 1
#define SEARCH_AMMO_TYPE                2

/* square features for team map memory */
#define SQRF_MINE       (1 << 0)
#define SQRF_DOWN       (1 << 1)
#define SQRF_DEPOT      (1 << 2)
void SetSquareFeature(int, int, int, char);
void UnsetSquareFeature(int, int, int, char);
bool HasSquareFeature(int, int, int, char);

extern int VNUM_HQ;
extern int VNUM_NAPALM;
extern int VNUM_FIRE;
extern int VNUM_GAS;
extern int VNUM_DARK;

/* What to do when we run into a wall. */
#define MWALL_NOMINAL     0
#define MWALL_IGNORE      1     /* No clipping */
#define MWALL_DESTROY     2     /* Crush, crush, crush */

#define MAX_HISTORY_SIZE  10
#define MAX_SB_LEN        512

/** TERMINAL TYPES *********/
#define ttype_has_color(t)  ((t) < TTYPE_PLAIN)
#define hascolor(ch)        ((ch)->desc && ttype_has_color((ch)->desc->ttype))

enum
{
    TTYPE_LINUX,
    TTYPE_XTERM,
    TTYPE_ANSI,
    TTYPE_VT102,
    TTYPE_VT100,
    TTYPE_VT10X,
    TTYPE_PLAIN,

    TTYPE_NULL
};

#ifdef GZ2_TELNET_C_
const char *ttypes[] = {
    "LINUX",
    "XTERM",
    "ANSI",
    "VT102",
    "VT100",
    "VT10X",
    "PLAIN",

    NULL
};
#endif


struct term_data
{
    int fg;
    int bg;
    uint isbold:1;
    uint isflash:1;
};


/*
 * Descriptor (channel) structure.
 */
struct descriptor_data
{
    DESCRIPTOR_DATA *next;
    DESCRIPTOR_DATA *snoop_by;

    CHAR_DATA *character;
    CHAR_DATA *original;

    GOD_TYPE *adm_data;
    unsigned int time;

    struct dns_query_data *dns_query;
    char host[80];

    sh_int descriptor;
    sh_int connected;

    int wait;
    bool fcommand;
    bool onprompt; // character has prompt displayed

    char inbuf[4 * MAX_INPUT_LENGTH];
    char incomm[MAX_INPUT_LENGTH];
    char inlast[MAX_HISTORY_SIZE][MAX_INPUT_LENGTH];
    int hpoint;
    int repeat;

    char *outbuf;
    int outsize;
    int outtop;
    int max_out;

    char *showstr_head;
    char *showstr_point;

    z_stream *outcom_stream;
    unsigned char *outcom;

    FILE *ping_pipe;
    DESCRIPTOR_DATA *ping_next;
    int ping_wait;

    struct term_data termdata;
    struct term_data store_termdata;

    int telstate;
    char subneg_buf[MAX_SB_LEN + 1];
    int subneg_len;
    char telopt[NTELOPTS];

    char ttype_recvd[MAX_SB_LEN + 1];
    int ttype;

    int lines;
    int columns;
    unsigned int sga:1;

    byte botLagFlag;        /* Command combination lag states. */
};


/* allocation validation structure */
struct allocations_type
{
    void *ptr;
    int size;
    char *filename;
    int line_num;
    struct allocations_type *next;
};

struct god_type
{
    char *rl_name;
    sh_int trust;
    char *game_names;
    char *password;
    char *room_name;
    int honorary;
};


struct team_type
{
    char *who_name;
    char *name;
    int score;
    int players;
    char *namecolor;
    bool independent;
    bool active;
    CHAR_DATA *teamleader;
    int x_mult;
    int y_mult;
    OBJ_DATA *hq;
    char *map_memory;
};


struct outfit_data
{
    int team;
    int vnum;
    int quantity;
    bool leader_only;
};


struct top_data
{
    char name[MAX_TOPNAME_LEN + 1];
    int val;
};


/*
 * TO types for act.
 */
#define TO_ROOM             0
#define TO_NOTVICT          1
#define TO_VICT             2
#define TO_CHAR             3
#define TO_NOT_NOSOCIALS    (1 << 10)       /* Flag to stop socials. */

/* Rank definitions. */
enum rank_et
{
    RANK_JOSH,
    RANK_UNRANKED,
    RANK_NOVICE,
    RANK_MERC,
    RANK_HUNTER,
    RANK_BADASS,
    RANK_MAX
};

#define REP(ch)   ((ch)->kills - (ch)->deaths)


/*
 * Help table types.
 */
struct help_data
{
    HELP_DATA *next;
    sh_int level;
    char *keyword;
    char *text;
};

struct wiznet_type
{
    char *name;
    long flag;
    int level;
};

/*
 * Data structure for notes.
 */

#define PNOTE_NODEL         (1 << 0)
#define PNOTE_URGENT        (1 << 1)
#define PNOTE_STUPID        (1 << 2)
#define PNOTE_SMART         (1 << 3)
#define PNOTE_QOTD          (1 << 4)
#define PNOTE_SOTD          (1 << 5)
#define PNOTE_GENERAL       (1 << 30)   /* Not a real flag. */


struct note_data
{
    NOTE_DATA *next;
    char *sender;
    char *date;
    char *to_list;
    char *subject;
    char *text;
    time_t date_stamp;
    unsigned long flags;
};

/* RT ASCII conversions -- used so we can have letters in this file */

#define A           1
#define B           2
#define C           4
#define D           8
#define E           16
#define F           32
#define G           64
#define H           128

#define I           256
#define J           512
#define K               1024
#define L           2048
#define M           4096
#define N           8192
#define O           16384
#define P           32768

#define Q           65536
#define R           131072
#define S           262144
#define T           524288
#define U           1048576
#define V           2097152
#define W           4194304
#define X           8388608

#define Y           16777216
#define Z           33554432
#define aa          67108864    /* doubled due to conflicts */
#define bb          134217728
#define cc          268435456
#define dd          536870912
#define ee          1073741824

/* mob behaviors */
#define BEHAVIOR_LD 0
#define BEHAVIOR_GUARD 1
#define BEHAVIOR_SEEK 2
#define BEHAVIOR_WANDER 3
#define BEHAVIOR_PILLBOX 4
#define BEHAVIOR_SEEKING_PILLBOX 5
#define BEHAVIOR_PLAYER_CONTROL 6
#define BEHAVIOR_MEDIC 7
#define BEHAVIOR_BOOMER 8
#define BEHAVIOR_TOSSBACK 9
#define BEHAVIOR_SEEKING_BOOMER 10

#define LIFETIME_SMOKE 100

/*
 * Bits for 'affected_by'.
 * Used in #MOBILES.
 */
#define AFF_BLIND       (A)
#define AFF_DAZE        (B)

/*
 * Sex.
 * Used in #MOBILES.
 */
#define SEX_NEUTRAL           0
#define SEX_MALE              1
#define SEX_FEMALE            2

/* dice */
#define DICE_NUMBER         0
#define DICE_TYPE           1
#define DICE_BONUS          2

/*
 * Wear flags.
 * Used in #OBJECTS.
 */
#define ITEM_TAKE       (A)
#define ITEM_WEAR_FINGER    (B)
#define ITEM_WEAR_NECK      (C)
#define ITEM_WEAR_BODY      (D)
#define ITEM_WEAR_HEAD      (E)
#define ITEM_WEAR_LEGS      (F)
#define ITEM_WEAR_FEET      (G)
#define ITEM_WEAR_HANDS     (H)
#define ITEM_WEAR_ARMS      (I)
#define ITEM_WEAR_SHIELD    (J)
#define ITEM_WEAR_ABOUT     (K)
#define ITEM_WEAR_WAIST     (L)
#define ITEM_WEAR_WRIST     (M)
#define ITEM_WIELD      (N)
#define ITEM_SEC_HAND           (O)
#define ITEM_HOLD       (P)
#define ITEM_TWO_HANDS          (Q)


/*
 * Room flags.
 * Used in #ROOMS.
 */
#define ROOM_DARK       (A)
#define ROOM_TANK               (B)
#define ROOM_CONNECTED          (C)     /* used at boot to guarantee connectivity */
#define ROOM_MECH       (D)
#define ROOM_GAS        (E)
#define ROOM_PRIVATE        (J)
#define ROOM_SAFE       (K)
#define ROOM_SOLITARY       (L)
#define ROOM_GODS_ONLY      (P)
#define ROOM_NEWBIES_ONLY   (R)


/*
 * Directions.
 * Used in #ROOMS.
 */
#define DIR_NORTH             0
#define DIR_EAST              1
#define DIR_SOUTH             2
#define DIR_WEST              3
#define DIR_UP                4
#define DIR_DOWN              5
#define NUM_OF_DIRS           6



/*
 * Exit flags.
 * Used in #ROOMS.
 */
#define EX_ISWALL             (A)
#define EX_ISNOBREAKWALL      (B)
#define EX_FAKEWALL           (C)

/*
 * Equpiment wear locations.
 * Used in #RESETS.
 */
#define WEAR_NONE             -1
#define WEAR_LIGHT            0
#define WEAR_FINGER_L         1
#define WEAR_FINGER_R         2
#define WEAR_NECK_1           3
#define WEAR_NECK_2           4
#define WEAR_BODY             5
#define WEAR_HEAD             6
#define WEAR_LEGS             7
#define WEAR_FEET             8
#define WEAR_HANDS            9
#define WEAR_ARMS            10
#define WEAR_SHIELD          11
#define WEAR_ABOUT           12
#define WEAR_WAIST           13
#define WEAR_WRIST_L         14
#define WEAR_WRIST_R         15
#define WEAR_WIELD           16
#define WEAR_SEC             17
#define WEAR_HOLD            18
#define MAX_WEAR             19



/***************************************************************************
 *                                                                         *
 *                   VALUES OF INTEREST TO AREA BUILDERS                   *
 *                   (End of this section ... stop here)                   *
 *                                                                         *
 ***************************************************************************/

/*
 * Positions.
 */
#define POS_DEAD              0
#define POS_MORTAL            1
#define POS_INCAP             2
#define POS_STUNNED           3
#define POS_SLEEPING              4
#define POS_RESTING           5
#define POS_SITTING           6
#define POS_FIGHTING              7
#define POS_STANDING              8



/*
 * ACT bits for players.
 */
#define ACT_DAMAGEABLE_ABS      (A)
#define ACT_NO_STUN             (B)
#define ACT_NO_BLIND            (C)
#define ACT_NO_GAS              (D)
#define ACT_NO_BURN             (E)
            /* . . . */
#define PLR_FREEZE_STATS        (I)
#define PLR_NOVOTE              (J)
#define PLR_SUSPECTED_BOT       (K)     /* Fuck with a suspected bot. */
#define PLR_AGGRO_ALL           (L)
#define PLR_AGGRO_OTHER         (M)
#define PLR_HOLYLIGHT           (N)
#define PLR_WIZINVIS            (O)
#define PLR_NOLEADER            (P)
#define PLR_REPOP               (Q)
#define PLR_BADPOP              (R)
#define PLR_RANKONLY            (S)
#define PLR_NONOTE              (T)
#define PLR_RESERVED_U          (U)     /* Formerly censor. */
#define PLR_RESERVED_V          (V)     /* Formerly statusbar. */
#define PLR_LOG                 (W)
#define PLR_DENY                (X)
#define PLR_FREEZE              (Y)
#define PLR_TRAITOR             (Z)

/*temp flags */
/*tank flags */
#define MAN_SHIELD              (A)
#define MAN_TURRET              (B)
#define MAN_DRIVE               (C)
#define MAN_GUN                 (D)
#define MAN_HUD                 (E)
#define TRAITOR                 (F)
#define IN_VEHICLE              (G)
#define IS_VEHICLE              (H)
#define REPORT                  (I)
#define TEMP_BLOCK_ED           (J)

/* RT comm flags -- may be used on both mobs and chars */
#define COMM_QUIET              (A)
#define COMM_NOWIZ              (B)
#define COMM_NOIMP              (C)
#define COMM_NOBEEP             (D)
// #define COMM_NOPAGE             (E)Unused
#define COMM_NOBOUNTY           (F)
#define COMM_NOKILLS            (G)
#define COMM_NOSKULLS           (H)
#define COMM_AUTOSCAN           (I)
#define COMM_UNUSED_J           (J)
#define COMM_UNUSED_K           (K)
#define COMM_COMPACT            (L)
#define COMM_BRIEF              (M)
#define COMM_PROMPT             (N)
#define COMM_COMBINE            (O)
#define COMM_TELNET_GA          (P)
#define COMM_NOSOCIALS          (Q)
#define COMM_UNUSED_R           (R)
#define COMM_UNUSED_S           (S)
#define COMM_NOEMOTE            (T)
#define COMM_NOSHOUT            (U)
#define COMM_NOTELL             (V)
#define COMM_NOCHANNELS         (W)

/* Wizchannels. */
#define WIZ_ON                  (A)
#define WIZ_LOGINS              (B)
#define WIZ_SITES               (C)
#define WIZ_SECURE              (D)
#define WIZ_PREFIX              (E)
#define WIZ_BADPOP              (F)
#define WIZ_BANS                (G)
#define WIZ_PLOG                (H)


struct level_data
{
    sh_int num_levels;
    sh_int level_number;
    sh_int x_size;
    sh_int y_size;
    sh_int reference_x;         /* x,y coordinates of room directly below */
    sh_int reference_y;         /* the reference from the previous level */
    ROOM_DATA *rooms_on_level;
    LEVEL_DATA *level_down;
    LEVEL_DATA *level_up;
};

struct room_data
{
    CHAR_DATA *people;
    OBJ_DATA *contents;
    OBJ_DATA *mine;
    LEVEL_DATA *this_level;
    OBJ_DATA *interior_of;
    CHAR_DATA *inside_mob;
    ROOM_DATA *next;
    long exit[6];
    long room_flags;
    int x, y, level;
    char *name;
    char *description;
    bool visited;       /* Graph traversal flag. */
};

/*
 * One character (PC or NPC).
 */
struct char_data
{
    CHAR_DATA *next;
    CHAR_DATA *prev;
    CHAR_DATA *next_in_room;
    CHAR_DATA *next_extract;
    CHAR_DATA *leader;
    CHAR_DATA *fighting;
    CHAR_DATA *chasing;
    CHAR_DATA *reply;
    CHAR_DATA *last_hit_by;
    CHAR_DATA *owner;
    CHAR_DATA *dubbing;
    OBJ_DATA *carrying;
    ROOM_DATA *in_room;
    ROOM_DATA *interior;
    PC_DATA *pcdata;
    int allows_followers;
    char *kill_msg;
    char *suicide_msg;
    char *names;
    sh_int valid;               /* added for debugging purposes */
    sh_int sex;
    sh_int timer;
    sh_int wait;
    int kills, deaths;
    int hit;
    int max_hit;
    long act, temp_flags;
    sh_int armor;
    int affected_by;
    sh_int position;
    int team;
    OBJ_DATA *owned_objs;
    CHAR_DATA *next_owned;
    int worth;

    /* For mobiles, only. */
    struct npc_data *npc;
    sh_int start_pos;
    sh_int ld_behavior;
    char *short_descript;
    OBJ_DATA *genned_by;
    char *where_start;
    sh_int move_delay;
    int abs;                    /* Inherent absorption. */

    /*
     * FOR PLAYERS ONLY.
     *
     * We really should reorganize all of this crap so we don't waste memory
     * giving mobiles this stuff.
     */
    int teamkill;
    CHAR_DATA *owned_chars;
    CHAR_DATA *last_team_kill;
    sh_int donate_num;
    FILE *log_fp;
    DESCRIPTOR_DATA *desc;
    NOTE_DATA *pnote;
    int prepare_dir;            /* Prepare to attack to the <x>. */
    int ld_timer;
    int idle;
    time_t logon;
    time_t last_note;
    time_t last_fight;
    long comm;                  /* RT added to pad the vector */
    long wiznet;                /* wiznet */
    sh_int invis_level;
    HELP_DATA *phelp;
    sh_int trust;
    int played;

    /* These are for vehicles, only. */
    int turret_dir;
    int shield_dir;
    int gun_dir;

    unsigned int in_extract:1;
};

struct cmd_node
{
    const char *cmd;
    struct cmd_node *next;
};

struct cmd_queue
{
    struct cmd_node *head;
    struct cmd_node *tail;
};

struct npc_data
{
    struct cmd_queue birth;
    struct cmd_queue heartbeat;
    struct cmd_queue depress;
    struct cmd_queue enforcer;
    struct cmd_queue death;
    struct cmd_queue kill;
};

#define MAX_PALETTE_ENTS      16

/*
 * Data which only PC's have.
 */
struct pc_data
{
    PC_DATA *next;
    time_t voted_on;
    char *password;
    char *poofin_msg;
    char *poofout_msg;
    char *title_line;
    int solo_hit;
    bool confirm_delete;
    int listen_data;

    time_t time_traitored;
    char volunteering;

    byte color_action;
    byte color_combat_condition_s;
    byte color_combat_condition_o;
    byte color_xy;
    byte color_wizi;
    byte color_hp;
    byte color_combat_o;
    byte color_level;
    byte color_exits;
    byte color_desc;
    byte color_obj;
    byte color_say;
    byte color_tell;
    byte color_reply;

    /* Pronoun support */
    uint it_is_chardata:1;
    union
    {
        struct char_data *cd;
        struct obj_data *od;
    }
    it;
    struct char_data *him;
    struct char_data *her;

    /* Override autodetected. */
    int ttype;
    int lines;
    int columns;

    /* Palette for Linux terminal users. */
    int palette[MAX_PALETTE_ENTS];

    char *created_site;

    /* Game Stats */
    int gs_kills;
    int gs_deaths;
    int gs_booms;
    int gs_lemmings;
    int gs_idle;
    int gs_hellpts;
    int gs_teamed_kills;
};


/*
 * Data which only PC's have.
 */
struct obj_index_data
{
    char *name;
    char *short_descr;
    char *description;
    char *explode_desc;
    char *attack_message;
    sh_int vnum;
    int hp_struct;
    int hp_char;
    sh_int item_type;
    sh_int count;
    sh_int prob_in, number_to_put_in;
    int extract_flags, general_flags, usage_flags;
    sh_int num_extract_burn;
    sh_int num_extract_explode;
    int wear_flags;
    sh_int ammo_type;
    sh_int burn_time;
    sh_int choke_time;
    sh_int ammo;
    sh_int weight;
    sh_int range;
    sh_int rounds_per_second;
    sh_int rounds_delay;
    double dam_mult;
    sh_int armor;
    sh_int damage_char[3];      /* 0. direct 1. in room 2. 1 away */
    sh_int damage_structural[3];
    sh_int scope_type;
    int team;

    sh_int arrival_time;
    char *destination;

    sh_int spawnTrail;
    char spawnMax;
    sh_int spawnTimer;

    /* Vehicle shit. */
    struct obj_vehicle_data *vehicle;
};


struct obj_vehicle_data
{
    char *tomob;
    int positions;
    int r_positions;
    char *start_message_dp;
    char *start_message_do;
    char *start_message_io;
    char *stop_message_dp;
    char *stop_message_do;
    char *stop_message_io;
    int capacity;
    char *interior_desc;
    int turret_high;
    int turret_low;
};


/*
 * One object.
 */
struct obj_data
{
    OBJ_DATA *next;
    OBJ_DATA *next_content;
    OBJ_DATA *contains;
    OBJ_DATA *in_obj;
    CHAR_DATA *carried_by;
    OBJ_INDEX_DATA *pIndexData;
    ROOM_DATA *in_room;
    CHAR_DATA *owner;
    CHAR_DATA *last_hit_by;
    char *name;                 /* Keywords. */
    char *short_descr;          /* Action message: name */
    char *description;          /* Action message: ... lies here. */
    char *explode_desc;         /* Action message: explode noun. */
    char *attack_message;       /* Action message: attack verb. */
    sh_int valid;               /* added for debugging purposes */
    sh_int item_type;
    sh_int vnum;
    int extract_flags, general_flags, usage_flags;
    sh_int num_extract_burn;
    sh_int num_extract_explode;
    int team;
    int hp_struct;
    int hp_char;
    sh_int extract_me;
    int wear_flags;
    sh_int wear_loc;
    sh_int weight;
    sh_int timer;
    sh_int cooked_timer;
    sh_int range;
    sh_int rounds_per_second;
    sh_int rounds_delay;
    double dam_mult;
    sh_int armor;
    sh_int arrival_time;
    sh_int ammo;
    sh_int ammo_type;
    sh_int wait_time;
    sh_int orientation;
    sh_int burn_time;
    sh_int choke_time;
    sh_int generic_counter;
    sh_int scope_type;

    ROOM_DATA *destination;
    ROOM_DATA *interior;        /* tank or plane interior */
    ROOM_DATA *startpoint;

    sh_int damage_char[3];      /* 0. direct 1. in room 2. 1 away */
    sh_int damage_structural[3];

    OBJ_DATA *next_owned;

    OBJ_INDEX_DATA *spawnTrail;
    char spawnMax;
    sh_int spawnTimer;

    /* Vehicle shit. */
    struct obj_vehicle_data *vehicle;
};

/*
 * Utility macros.
 */
#define ABS(a)          ((a) < 0 ? -(a) : (a))
#define UMIN(a, b)      ((a) < (b) ? (a) : (b))
#define UMAX(a, b)      ((a) > (b) ? (a) : (b))
#define URANGE(a, b, c) (UMAX((a), UMIN((b), (c))))
#define LOWER(c)        ((c) >= 'A' && (c) <= 'Z' ? (c)+'a'-'A' : (c))
#define UPPER(c)        ((c) >= 'a' && (c) <= 'z' ? (c)+'A'-'a' : (c))
#define IS_SET(flag, bit)   ((flag) & (bit))
#define SET_BIT(var, bit)   ((var) |= (bit))
#define REMOVE_BIT(var, bit)    ((var) &= ~(bit))
#define IFNOT(v, then)  ((v) ? (v) : (then))


/*
 * Character macros.
 */
#define IS_NPC(ch)      (((ch)->pcdata == NULL))
#define IS_IMMORTAL(ch)     (get_trust(ch))
#define IS_HERO(ch)     (get_trust(ch))
#define IS_IMP(ch)      (get_trust(ch) == MAX_TRUST)
#define IS_TRUSTED(ch,level)    (get_trust(ch))

#define GET_AGE(ch)     ((int) (17 + ((ch)->played \
                    + current_time - (ch)->logon )/72000))
#define HOURS_PLAYED(ch)  ((((ch)->played + current_time - (ch)->logon) / 3600))

#define IS_AWAKE(ch)        (ch->position > POS_SLEEPING)

#define WAIT_STATE(ch, npulse)  ((ch)->wait = UMAX((ch)->wait, (npulse)))
#define DESC_WAIT_STATE(d, npulse) ((d)->wait = UMAX((d)->wait, (npulse)))


/*
 * Object macros.
 */
#define CAN_WEAR(obj, part) (IS_SET((obj)->wear_flags,  (part)))

#define CHECK_VEHICLE(o, m)     (o->item_type == ITEM_TEAM_VEHICLE ? \
                                 o->vehicle->m : \
                                 dummy_vehicle.m)

#define OBJ_CAPACITY(o)         (CHECK_VEHICLE(o, capacity))
#define OBJ_TOMOB(o)            (CHECK_VEHICLE(o, tomob))
#define OBJ_POSITIONS(o)        (CHECK_VEHICLE(o, positions))
#define OBJ_R_POSITIONS(o)      (CHECK_VEHICLE(o, r_positions))
#define OBJ_START_MESSAGE_DP(o) (CHECK_VEHICLE(o, start_message_dp))
#define OBJ_START_MESSAGE_DO(o) (CHECK_VEHICLE(o, start_message_do))
#define OBJ_START_MESSAGE_IO(o) (CHECK_VEHICLE(o, start_message_io))
#define OBJ_STOP_MESSAGE_DP(o)  (CHECK_VEHICLE(o, stop_message_dp))
#define OBJ_STOP_MESSAGE_DO(o)  (CHECK_VEHICLE(o, stop_message_do))
#define OBJ_STOP_MESSAGE_IO(o)  (CHECK_VEHICLE(o, stop_message_io))
#define OBJ_INTERIOR_DESC(o)    (CHECK_VEHICLE(o, interior_desc))
#define OBJ_TURRET_HIGH(o)      (CHECK_VEHICLE(o, turret_high))
#define OBJ_TURRET_LOW(o)       (CHECK_VEHICLE(o, turret_low))


/*
 * Description macros.
 */
#define PERS(ch, looker)    ( can_see( looker, (ch) ) ?     \
                ( IS_NPC(ch) ? (ch)->short_descript \
                : (ch)->names ) : "someone" )


/* Structures for Polls */
struct poll_opt_data
{
    char *text;
    int tally;
};

struct poll_data
{
    char *question;
    char *author;
    int voters;
    time_t timestamp;
    int numOptions;
    struct poll_opt_data *options;
    struct poll_data *next;
};


/*
 * Structure for a social in the socials table.
 */
struct social_type
{
    char *name;
    char *char_no_arg;
    char *others_no_arg;
    char *char_found;
    char *others_found;
    char *vict_found;
    char *char_not_found;
    char *char_auto;
    char *others_auto;
};

/*
 * Global constants.
 */
extern struct social_type *social_table;
extern const struct god_type imp_table[];
extern const struct wiznet_type wiznet_table[];
extern char *const dir_name[];
extern const int dir_cmod[NUM_OF_DIRS][3];
extern const sh_int rev_dir[];

extern struct obj_vehicle_data dummy_vehicle;
extern bool g_forceAction;

/*
 * Global variables.
 */
extern struct team_type *team_table;
extern HELP_DATA *help_first;

//extern BAN_DATA *ban_list;
//extern BAN_DATA *nban_list;
extern CHAR_DATA *char_list;
extern DESCRIPTOR_DATA *descriptor_list;
extern NOTE_DATA *note_list;
extern OBJ_DATA *object_list;

extern OBJ_DATA *object_list;
extern OBJ_INDEX_DATA *all_objects;
extern LEVEL_DATA *the_city;

//extern BAN_DATA *ban_free;
extern CHAR_DATA *char_free;
extern DESCRIPTOR_DATA *descriptor_free;
extern NOTE_DATA *note_free;
extern OBJ_DATA *obj_free;
extern PC_DATA *pcdata_free;

extern time_t current_time;
extern bool fLogAll;
extern char log_buf[];
extern TIME_INFO_DATA time_info;
extern sh_int teleport_time;
extern int tick_counter;
extern int expansions;
extern int ground0_down;
extern ROOM_DATA *ammo_repop[3];
extern ROOM_DATA *deploy_area;
extern ROOM_DATA *safe_area;
extern ROOM_DATA *satori_area;
extern ROOM_DATA *god_general_area;
extern ROOM_DATA *explosive_area;
extern ROOM_DATA *store_area;
extern ROOM_DATA *graveyard_area;
extern GOD_TYPE *god_table;
extern int iteration;
extern CHAR_DATA *next_violence;
extern int max_on;
extern int buttontimer;
extern CHAR_DATA *buttonpresser;

extern CHAR_DATA *enforcer;
extern CHAR_DATA *pill_box;
extern CHAR_DATA *guardian;
extern CHAR_DATA *tank_mob;
extern CHAR_DATA *mech_mob;
extern CHAR_DATA *trooper;
extern CHAR_DATA *medic;
extern CHAR_DATA *marine;
extern CHAR_DATA *turret;

extern TOP_DATA top_players_kills[NUM_TOP_KILLS];
extern TOP_DATA top_players_deaths[NUM_TOP_DEATHS];
extern TOP_DATA top_players_hours[NUM_TOP_HOURS];
extern CHAR_DATA *extract_list;
extern int expand_event;
extern bool fBootDb;

extern int g_numTeams;
extern int TEAM_NONE;
extern int TEAM_TRAITOR;
extern int TEAM_ADMIN;
extern int TEAM_MOB;

extern bool g_safeVolMode;

/*
 * The crypt(3) function is not available on some operating systems.
 * In particular, the U.S. Government prohibits its export from the
 *   United States to foreign countries.
 * Turn on NOCRYPT to keep passwords in plain text.
 */
#if defined(NOCRYPT)
#define crypt(s1, s2)   (s1)
#endif

#define extract_obj(a, b)   x_extract_obj((a), (b), __FILE__, __LINE__, __FUNCTION__)


/*
 * Data files used by the server.
 *
 * AREA_LIST contains a list of areas to boot.
 * All files are read in completely at bootup.
 * Most output files (bug, idea, typo, shutdown) are append-only.
 *
 * The NULL_FILE is held open so that we have a stream handle in reserve,
 *   so players can go ahead and telnet to all the other descriptors.
 * Then we close it whenever we need to open a file (e.g. a save file).
 */

#define SLASH           "/"
#define ROOT            ".." SLASH
#define PLAYER_DIR      ROOT "player" SLASH     /* Player files        */
#define BAK_PLAYER_DIR  PLAYER_DIR "bak" SLASH  /* deleted player files */
#define PLAYER_TEMP     PLAYER_DIR "temp"       /* temporary file */
#define BOOT_DIR        ROOT "boot" SLASH       /* the db's root */
#define SCEN_DIR        BOOT_DIR "scn" SLASH    /* scenarios */
#define DFLT_SCEN_DIR   SCEN_DIR "DEFAULT" SLASH        /* default scenario */
#define CUR_SCEN_DIR    SCEN_DIR "current" SLASH        /* current scenario */
#define INDEX_FILE      "index" /* index of files in a scen */

#define TOP_FILE        BOOT_DIR "kills.top"    /* top kills list */
#define TOP_DEATH_FILE  BOOT_DIR "deaths.top"   /* top deaths list */
#define TOP_HOURS_FILE  BOOT_DIR "hours.top"    /* top hours list */

#define BOOT_LIST       BOOT_DIR "files.boot"   /* List of boot files */
#define BAN_LIST        BOOT_DIR "ban.boot"     /* List of banned sites */
#define NBAN_LIST       BOOT_DIR "nban.boot"    /* List of banned sites */
#define POLL_FILE       BOOT_DIR "polls.boot"   /* Public Polls */

#define BUG_FILE        BOOT_DIR "bugs.txt"     /* For 'bug' and bug() */
#define IDEA_FILE       BOOT_DIR "ideas.txt"    /* For 'idea' */
#define TYPO_FILE       BOOT_DIR "typos.txt"    /* For 'typo' */
#define NOTE_FILE       BOOT_DIR "notes.txt"    /* For 'note' */
#define SHUTDOWN_FILE   BOOT_DIR "shutdown.txt" /* For 'shutdown' */
#define REBOOT_FILE     BOOT_DIR "reboot.txt"   /* For 'reboot' */



/*
 * Our function prototypes.
 * One big lump ... this is every function in Merc.
 */

/* act_comm.c */
bool is_note_to(CHAR_DATA * ch, NOTE_DATA * pnote);
void check_sex(CHAR_DATA * ch);
void add_follower(CHAR_DATA * ch, CHAR_DATA * master);
void stop_follower(CHAR_DATA * ch);
void die_follower(CHAR_DATA * ch);
bool is_same_group(CHAR_DATA * ach, CHAR_DATA * bch);

DECLARE_DO_FUN(do_commbadge);

/* act_info.c */
char *diagnose(struct char_data *);
char *diagnose_simple(struct char_data *);
void set_title(CHAR_DATA * ch, char *title);

/* act_move.c */
bool move_char(CHAR_DATA * ch, int door, bool follow, int walls);
ROOM_DATA *get_to_room(ROOM_DATA * curr_room, sh_int door);
ROOM_DATA *index_room(ROOM_DATA * curr_room, sh_int x, sh_int y);
void complete_movement(CHAR_DATA *, int);

DECLARE_DO_FUN(do_north);
DECLARE_DO_FUN(do_east);
DECLARE_DO_FUN(do_south);
DECLARE_DO_FUN(do_west);
DECLARE_DO_FUN(do_up);
DECLARE_DO_FUN(do_down);

/* act_team.c */
void autoleader(int, CHAR_DATA *, CHAR_DATA *);
int lookup_team(char *);
void boot(CHAR_DATA *);
void induct(CHAR_DATA *, int);
int frobPlayerCount(int, int);
int lowestteam(void);
int worstTeam(void);
int bestTeam(void);
struct char_data *get_team_guard(int);

DECLARE_DO_FUN(do_resig);
DECLARE_DO_FUN(do_resign);
DECLARE_DO_FUN(do_volunteer);
DECLARE_DO_FUN(do_boot);
DECLARE_DO_FUN(do_showvol);
DECLARE_DO_FUN(do_induct);
DECLARE_DO_FUN(do_forgive);

/* act_obj.c */
void get_obj(CHAR_DATA * ch, OBJ_DATA * obj, OBJ_DATA * container);
void send_object(OBJ_DATA *, ROOM_DATA *, sh_int);
void drop_obj(CHAR_DATA *, OBJ_DATA *);

/* act_wiz.c */
void wiznet(char *, CHAR_DATA *, OBJ_DATA *, long, long, int);

/* comm.c */
void show_string(struct descriptor_data *d, char *input);
void close_socket(DESCRIPTOR_DATA * dclose);
void write_to_buffer
args((DESCRIPTOR_DATA * d, const char *txt, int length));
void send_to_team(const char *txt, int team);
void send_to_char(const char *txt, CHAR_DATA * ch);
void page_to_char(const char *txt, CHAR_DATA * ch);
void act(const char *format, CHAR_DATA * ch,
         const void *arg1, const void *arg2, int type);
void act_new(const char *format, CHAR_DATA * ch,
             const void *arg1, const void *arg2, int type, int min_pos);
void printf_to_char(CHAR_DATA * ch, char *fmt, ...);

/* db.c */
void scatter_objects(void);
CHAR_DATA *clone_mobile(CHAR_DATA * ch, int dup);
ROOM_DATA *find_location(CHAR_DATA * ch, char *argument);
void boot_db(void);
void area_update(void);
OBJ_DATA *create_object(OBJ_INDEX_DATA * pObjIndex, int level);
void clone_object(OBJ_DATA * parent, OBJ_DATA * clone);
void clear_char(CHAR_DATA * ch);
void free_char(CHAR_DATA * ch);
OBJ_INDEX_DATA *get_obj_index(int vnum);
char fread_letter(FILE * fp);
int fread_number(FILE * fp);
long fread_flag(FILE * fp);
char *escape_qstring(const char *);
char *fread_qstring(FILE * fp);
char *fread_string(FILE * fp);
char *fread_string_eol(FILE * fp);
void fread_to_eol(FILE * fp);
char *fread_word(FILE * fp);
long flag_convert(char letter);
void mark_alloc(void *ptr, int size, struct allocations_type **first_alloc,
                char *filename, int line_num);
void mark_free(void *ptr, struct allocations_type **first_alloc,
               char *filename, int line_num);
struct allocations_type *get_mem_reference(void *ptr,
                                           struct allocations_type
                                           **first_alloc);
int size_correct(void *ptr, int size,
                 struct allocations_type **first_alloc);

#define alloc_mem(sMem)      f_alloc_mem (sMem, __FILE__, __LINE__)
#define alloc_perm(sMem)     f_alloc_perm (sMem, __FILE__, __LINE__)
#define free_mem(pMem, sMem) f_free_mem (pMem, sMem, __FILE__, __LINE__)
#define str_dup(str)         f_str_dup (str, __FILE__, __LINE__)
#define free_string(pstr)    f_free_string (pstr, __FILE__, __LINE__)

void *f_alloc_mem(int sMem, char *filename, int line_num);
void *f_alloc_perm(int sMem, char *filename, int line_num);
void f_free_mem(void *pMem, int sMem, char *filename, int line_num);
char *f_str_dup(const char *str, char *filename, int line_num);
void f_free_string(char *pstr, char *filename, int line_num);

int number_fuzzy(int number);
int number_range(int from, int to);
int number_percent(void);
int number_door(void);
int number_bits(int width);
long number_mm(void);
int dice(int number, int size);
int interpolate(int level, int value_00, int value_32);
void smash_tilde(char *str);


void tail_chain(void);
void expand_city();

/* string.c */
int str_cmp(const char *, const char *);
int strn_cmp(const char *, const char *, size_t);
int str_suffix(const char *, const char *);
int str_len(const char *);
void strn_cpy(char *, const char *, int);
char *capitalize(const char *str);
char *without_colors(const char *str);
void smash_tilde(char *str);

/* inline.c */
int ProcessColors(struct descriptor_data *, bool, const char *, int,
                  char *, int);

/* fight.c */
bool is_safe(CHAR_DATA * ch, CHAR_DATA * victim);
void violence_update(void);
void update_pos(CHAR_DATA * victim);
void stop_fighting(CHAR_DATA * ch, bool fBoth);
void heal_char(CHAR_DATA *, sh_int);
void bang_obj(OBJ_DATA * obj, int perm_extract);
void choke_obj(OBJ_DATA *, ROOM_DATA *, int);
void burn_obj(OBJ_DATA *);
void char_death(CHAR_DATA *, const char *);
int damage_char(CHAR_DATA *, CHAR_DATA *, sh_int, bool, const char *);
int shoot_damage(CHAR_DATA *, CHAR_DATA *, sh_int, bool);
int damage_obj(OBJ_DATA *, sh_int, sh_int, CHAR_DATA *);
int mod_hp_obj(OBJ_DATA *, sh_int, sh_int, CHAR_DATA *);
int check_obj_alive(OBJ_DATA *);
int damage_room(ROOM_DATA *, sh_int);
void falling_wall(ROOM_DATA *, sh_int);
int test_char_alive(CHAR_DATA *);
int check_char_alive(CHAR_DATA *, const char *);
int shoot_char(CHAR_DATA *, CHAR_DATA *, OBJ_DATA *, int);
void adjust_kp(CHAR_DATA *, CHAR_DATA *);
void kill_message(CHAR_DATA *, CHAR_DATA *, const char *);

/* handler.c */
long wiznet_lookup(const char *name);
bool is_old_mob(CHAR_DATA * ch);
int get_age(CHAR_DATA * ch);
void reset_char(CHAR_DATA * ch);
int get_trust(CHAR_DATA * ch);
int can_carry_n(CHAR_DATA * ch);
int can_carry_w(CHAR_DATA * ch);
bool is_name(char *str, char *namelist);
bool is_name_exact(char *str, char *namelist);
bool is_name_obj(char *str, char *namelist);
void char_from_room(CHAR_DATA * ch);
void char_to_room(CHAR_DATA * ch, ROOM_DATA * pRoomIndex);
void obj_to_char(OBJ_DATA * obj, CHAR_DATA * ch);
void obj_from_char(OBJ_DATA * obj);
int apply_ac(OBJ_DATA * obj, int iWear, int type);
OBJ_DATA *get_eq_char(CHAR_DATA * ch, int iWear);
void equip_char(CHAR_DATA * ch, OBJ_DATA * obj, int iWear);
void unequip_char(CHAR_DATA * ch, OBJ_DATA * obj);
int count_obj_list(OBJ_INDEX_DATA * obj, OBJ_DATA * list);
void obj_from_room(OBJ_DATA * obj);
void obj_to_room(OBJ_DATA * obj, ROOM_DATA * pRoomIndex);
void obj_to_obj(OBJ_DATA * obj, OBJ_DATA * obj_to);
void obj_from_obj(OBJ_DATA * obj);
void x_extract_obj(OBJ_DATA * obj, int perm_extract, const char *, int, const char *);
void extract_char(CHAR_DATA * ch, bool fPull);
CHAR_DATA *get_char_room(CHAR_DATA * ch, char *argument);
CHAR_DATA *get_char_world(CHAR_DATA * ch, char *argument);
OBJ_DATA *get_obj_type(OBJ_INDEX_DATA * pObjIndexData);
OBJ_DATA *get_obj_list
args((CHAR_DATA * ch, char *argument, OBJ_DATA * list));
OBJ_DATA *get_obj_carry(CHAR_DATA * ch, char *argument);
OBJ_DATA *get_obj_wear(CHAR_DATA * ch, char *argument);
OBJ_DATA *get_obj_here(CHAR_DATA * ch, char *argument);
OBJ_DATA *get_obj_world(CHAR_DATA * ch, char *argument);
int get_obj_number(OBJ_DATA * obj);
int get_obj_weight(OBJ_DATA * obj);
bool room_is_dark(ROOM_DATA * pRoomIndex);
bool room_is_private(ROOM_DATA * pRoomIndex);
bool can_see(CHAR_DATA * ch, CHAR_DATA * victim);
bool can_see_obj(CHAR_DATA * ch, OBJ_DATA * obj);
bool can_see_room(CHAR_DATA * ch, ROOM_DATA * pRoomIndex);
bool can_see_linear_char(CHAR_DATA * ch, CHAR_DATA * victim,
                         int shooting, bool second);
bool can_see_linear_room(CHAR_DATA * ch, ROOM_DATA * a_room);
int numeric_direction(char *arg);
bool can_drop_obj(CHAR_DATA * ch, OBJ_DATA * obj);
char *item_type_name(OBJ_DATA * obj);
char *wear_bit_name(int wear_flags);
char *act_bit_name(int act_flags);
char *comm_bit_name(int comm_flags);
int max_sight(CHAR_DATA *);
int max_sight_fire(CHAR_DATA *, bool);
void disownObject(OBJ_DATA *);
void claimObject(CHAR_DATA *, OBJ_DATA *);
OBJ_DATA *find_eq_char(CHAR_DATA *, int, unsigned int);
OBJ_DATA *find_worn_eq_char(CHAR_DATA *, int, unsigned int);
int count_carried(CHAR_DATA *);
int count_people(ROOM_DATA *);
OBJ_DATA *find_ammo(OBJ_DATA *);

/* top.c */
void initialise_top(TOP_DATA * topv, size_t sz);
void show_top_list(CHAR_DATA * ch, TOP_DATA * topv, size_t sz);
void read_top(const char *fname, TOP_DATA * topv, size_t sz);
void check_top_kills(CHAR_DATA * ch);
void check_top_deaths(CHAR_DATA * ch);
void check_top_hours(CHAR_DATA * ch);

/* interp.c */
void interpret(CHAR_DATA * ch, char *argument);
bool is_number(char *arg);
int number_argument(char *argument, char *arg);
char *one_argument(char *argument, char *arg_first);
char *one_argument_pcase(char *argument, char *arg_first);
char *one_word(char *argument, char *arg_first);

/* poll.c */
DECLARE_DO_FUN(do_poll);
void save_polls();
void load_polls();

/* save.c */
void save_char_obj(CHAR_DATA * ch);
CHAR_DATA *load_char_obj(DESCRIPTOR_DATA * d, char *name);

/* update.c */
void gain_condition(CHAR_DATA * ch, int iCond, int value);
void update_handler(void);

/* pfile.c */
char *pfile_name(const char *, const char *);
bool rename_pfile(const char *, const char *);
bool remove_pfile(const char *);
bool backup_pfile(const char *);
bool player_exists(const char *);

/* utils.c */
int get_line(FILE *, char *, size_t);
void append_file(CHAR_DATA * ch, char *file, char *str);
void log_base_string(const char *str);
void logmesg(const char *str, ...);
void touch(const char *);
int copy_file(const char *, const char *);
void logerr(const char *, ...);
void wizlog(CHAR_DATA *, OBJ_DATA *, long, long, int, const char *, ...);

/* social-edit.c */
void load_social_table();
void save_social_table();

/* vehicle.c */
const char *vehicleTypeName(OBJ_DATA *);
// extern inline bool isVehicle(OBJ_DATA *);
void scan_direction(CHAR_DATA *, ROOM_DATA *, int, int, bool);
void vehicle_hud(OBJ_DATA *, CHAR_DATA *);

/* rank.c */
int RANK(CHAR_DATA *);
void report_new_rank(CHAR_DATA *, int);
int check_special_attack(CHAR_DATA *, OBJ_DATA *);

/* act_team.c */
void boot(CHAR_DATA *);
void induct(CHAR_DATA *, int);
int lookup_team(char *);
void autoleader(int, CHAR_DATA *, CHAR_DATA *);
int frobPlayerCount(int, int);
int lowestteam(void);
int worstTeam(void);
int bestTeam(void);

/* mob_vehicle.c */
struct char_data *convert_vehicle(struct obj_data *);
struct obj_data *revert_vehicle(struct char_data *);
int control_vehicle(struct char_data *, struct char_data *, int);
const char *man_name(int);
struct char_data *find_manning(struct room_data *, int);
int man_position(struct char_data *, int);
void stop_manning(struct char_data *);
int verify_manning(struct room_data *);

/* Some command functions used throughout. */
DECLARE_DO_FUN(do_goto);
DECLARE_DO_FUN(do_teleport);
DECLARE_DO_FUN(do_gecho);
DECLARE_DO_FUN(do_depress);
DECLARE_DO_FUN(do_reboot);
DECLARE_DO_FUN(do_wear);
DECLARE_DO_FUN(do_stand);
