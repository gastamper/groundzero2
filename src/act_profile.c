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
 * Module:      act_profile.c
 * Synopsis:    The ``profile'' command and related utilities.
 * Author:      Satori
 * Created:     31 Mar 2002
 *
 * NOTES:
 * Moved here from act_info.c because it's so long.
 */

#include "ground0.h"

static int field_string(const char *, char *);
static void calc_weapon_dam(struct obj_data *, int *, int *, int *);
void profile_miscellaneous(struct char_data *, struct obj_data *);
void profile_weapon(struct char_data *, struct obj_data *);
void profile_armor(struct char_data *, struct obj_data *);
void profile_ammo(struct char_data *, struct obj_data *);
void profile_explosive(struct char_data *, struct obj_data *);

extern char **ammo_types;
extern int top_obj_index;


void
do_profile (struct char_data *ch, char *argument)
{
    struct obj_index_data *oid = NULL;
    struct obj_data *obj = NULL;
    char arg[MAX_INPUT_LENGTH];
    int count;

    while ( isspace(*argument) )
        argument++;

    if ( !*argument )
    {
        send_to_char("&cUSAGE&X: &Wprofile &X<&nobject&X>\r\n", ch);
        return;
    }

    /*
     * Progression.  Look for the object in the following places, in order:
     * 1) Inventory,
     * 2) Equipment,
     * 3) Room,
     * 4) World,
     * 5) Object prototypes.
     */
    if ( (obj = get_obj_carry(ch, argument) ) == NULL &&
        (obj = get_obj_wear(ch, argument)) == NULL &&
        (obj = get_obj_here(ch, argument)) == NULL &&
        (obj = get_obj_world(ch, argument)) == NULL)
    {
        /* Not found anywhere in the world, so try a prototype. */
        count = number_argument(argument, arg);
        int ivn;

        for ( ivn = 1; ivn < top_obj_index; ivn++ )
            if ( (oid = get_obj_index(ivn)) != NULL )
                if ( is_name_obj(arg, oid->name) && --count < 1 )
                    break;

        /* Not found in prototypes, either; error. */
        if ( ivn == top_obj_index )
        {
            printf_to_char(ch, "%s: not found\r\n", argument);
            return;
        }

        /* Otherwise, load a version to profile.  Destroy it later. */
        if ( (obj = create_object(oid, 0)) == NULL )
        {
            send_to_char
                ("Could not load object from prototype to profile.\r\n",
                 ch);
            return;
        }
    }

    printf_to_char(ch, "&m%s&n\r\n", without_colors(obj->short_descr));

    /* Dispatch to type-specific profiling function. */
    switch (obj->item_type)
    {
        case ITEM_MISC:
            profile_miscellaneous(ch, obj);
            break;
        case ITEM_WEAPON:
            profile_weapon(ch, obj);
            break;
        case ITEM_ARMOR:
            profile_armor(ch, obj);
            break;
        case ITEM_EXPLOSIVE:
            profile_explosive(ch, obj);
            break;
        case ITEM_AMMO:
            profile_ammo(ch, obj);
            break;
        default:
            break;
    }

    if ( obj->general_flags )
    {
        char buf[MAX_STRING_LENGTH];
        int x = 0;

        strcpy(buf, "\r\n");

        /* Profile general abilities. */
        if ( IS_SET(obj->general_flags, GEN_CONDITION_MONITOR) && (x = 1) )
            strcat(buf, "Transports wearer to safe room when near death.\r\n");
        if ( IS_SET(obj->general_flags, GEN_BURNS_ROOM) && (x = 1) )
            strcat(buf, "Burns people in the same room as it.\r\n");
        if ( IS_SET(obj->general_flags, GEN_LEAVE_TRAIL) && (x = 1) )
            strcat(buf, "Leaves a trail behind it.\r\n");
        if ( IS_SET(obj->general_flags, GEN_DETECT_MINE) && (x = 1) )
            strcat(buf, "When worn, detects the presence of and protects again mines.\r\n");
        if ( IS_SET(obj->general_flags, GEN_ANTI_BLIND) && (x = 1) )
            strcat(buf, "Protects against blindness.\r\n");
        if ( IS_SET(obj->general_flags, GEN_DARKS_ROOM) && (x = 1) )
            strcat(buf, "Obscures vision.\r\n");
        if ( IS_SET(obj->general_flags, GEN_EXTRACT_ON_IMPACT) && (x = 1) )
            strcat(buf, "Explodes on impact.\r\n");
        if ( IS_SET(obj->general_flags, GEN_SEE_IN_DARK) && (x = 1) )
            strcat(buf, "Enables wearer to see in the dark.\r\n");
        if ( IS_SET(obj->general_flags, GEN_ANTI_DAZE) && (x = 1) )
            strcat(buf, "Protects the wearer against stun.\r\n");
        if ( IS_SET(obj->general_flags, GEN_CHOKES_ROOM) && (x = 1) )
            strcat(buf, "Suffocates people in the same room as it.\r\n");
        if ( IS_SET(obj->general_flags, GEN_IGNORE_ARMOR) && (x = 1) )
            strcat(buf, "Damage is not affected by armor absorption.\r\n");
        if ( IS_SET(obj->general_flags, GEN_CAN_PUSH) && (x = 1) )
            strcat(buf, "Can be pushed.\r\n");
        if ( IS_SET(obj->general_flags, GEN_NODEPOT) && (x = 1) )
            strcat(buf, "Does not repop in depots.\r\n");
        if ( IS_SET(obj->general_flags, GEN_ANTI_POISON) && (x = 1) )
            strcat(buf, "Protects against poisonous gas.\r\n");
        if ( IS_SET(obj->general_flags, GEN_TIMER_ITEM) && (x = 1) )
            strcat(buf, "Has a programmable timer.\r\n");
        if ( IS_SET(obj->general_flags, GEN_COMBUSTABLE) && (x = 1) )
            strcat(buf, "Can be damaged by fire.\r\n");
        if ( IS_SET(obj->general_flags, GEN_CAN_BURY) )
            strcat(buf, "Can be buried (is a mine).\r\n");

        strcat(buf, "\r\n");

        /* Extraction abilities. */
        if ( IS_SET(obj->extract_flags, EXTRACT_EXPLODE_ON_EXTRACT ) &&
            (x = 1))
            strcat(buf, "Explodes when extracted.\r\n");
        if ( IS_SET(obj->extract_flags, EXTRACT_BLIND_ON_EXTRACT ) &&
            (x = 1))
            strcat(buf, "Causes blindness when extracted.\r\n");
        if ( IS_SET(obj->extract_flags, EXTRACT_BURN_ON_EXTRACT) && (x = 1) )
            strcat(buf, "Ignites fires when extracted.\r\n");
        if ( IS_SET(obj->extract_flags, EXTRACT_STUN_ON_EXTRACT) && (x = 1) )
            strcat(buf, "Causes stun when extracted.\r\n");
        if ( IS_SET(obj->extract_flags, EXTRACT_DARK_ON_EXTRACT) && (x = 1) )
            strcat(buf, "Creates smoke screen when extracted.\r\n");
        if ( IS_SET(obj->extract_flags, EXTRACT_EXTINGUISH_ON_EXTRACT ) &&
            (x = 1))
            strcat(buf, "Douses fires when extracted.\r\n");
        if ( IS_SET(obj->extract_flags, EXTRACT_ANNOUNCE) && (x = 1) )
            strcat(buf,
                   "Announces victim when stepped on (mine only).\r\n");
        if ( IS_SET(obj->extract_flags, EXTRACT_CHOKE_ON_EXTRACT ) &&
            (x = 1))
            strcat(buf, "Releases poisonous gas when extracted.\r\n");
        if ( IS_SET(obj->extract_flags, EXTRACT_TELERAN_ON_EXTRACT ) &&
            (x = 1))
            strcat(buf, "Randomly teleports victims when extracted.\r\n");
        if ( IS_SET(obj->extract_flags, EXTRACT_TELELOC_ON_EXTRACT ) &&
            (x = 1))
            strcat(buf, "Teleports owner to victim when extracted.\r\n");

        /* Only print if we found something. */
        if ( x == 1 )
            send_to_char(buf, ch);
    }

    /* Destroy the object if loaded from a prototype. */
    if ( oid != NULL )
        extract_obj(obj, 1);

    send_to_char("&n", ch);
}


static void
calc_weapon_dam (struct obj_data *wpn, int *lower, int *upper, int *max)
{
    struct obj_index_data *oi;

    if ( wpn->item_type != ITEM_WEAPON )
        return;

    *lower = 666666666;
    *upper = 0;
    *max = 0;

    if ( !IS_SET(wpn->general_flags, GEN_NO_AMMO_NEEDED) )
    {
        for ( int ivn = 1; ivn < top_obj_index; ivn++ )
        {
            if ( (oi = get_obj_index(ivn) ) != NULL &&
                oi->item_type == ITEM_AMMO &&
                oi->ammo_type == wpn->ammo_type && oi->damage_char[0] > 0)
            {
                *upper = UMAX(*upper, oi->damage_char[0]);
                *lower = UMIN(*lower, oi->damage_char[0]);
                *max = UMAX(*max, oi->ammo);
            }
        }
    }
    else
    {
        *upper = wpn->damage_char[0];
        *lower = wpn->damage_char[0];
    }

    *lower = UMAX(1, ((*lower) - 200));
    *upper =
        ((int) ((double) (*upper) * wpn->dam_mult)) * UMAX(1,
                                                           wpn->
                                                           rounds_per_second);
    *lower =
        ((int) ((double) (*lower) * wpn->dam_mult)) * UMAX(1,
                                                           wpn->
                                                           rounds_per_second);
}


static int
field_string (const char *fld, char *buf)
{
    char field[19];

    sprintf(field, "%.18s", fld);
    return sprintf(buf, "&c%-18s&X: &n", field);
}


void
profile_miscellaneous (struct char_data *ch, struct obj_data *obj)
{
    char first[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    int dlower = 0, dupper = 0;
    int k = 0;
    int x = 0;                  /* Throw-away temporary. */

    one_argument(obj->name, first);
    k += field_string("Purpose", buf + k);

    if ( IS_SET(obj->usage_flags, USE_AIRSTRIKE) && (x = 1) )
    {
        struct obj_index_data *tmp = get_obj_index(VNUM_NAPALM);

        k += sprintf(buf + k, "Delivers precise airstrikes.\r\n");
        k += field_string("Use", buf + k);
        k += sprintf(buf + k, "&Wuse %s &X<&nx&X> <&ny&X>\r\n", first);

        if ( tmp != NULL )
        {
            one_argument(tmp->name, first);

            k += field_string("Direct Damage", buf + k);
            k += sprintf(buf + k,
                         "&R%d &nbiological &X/ &R%d&n structural\r\n",
                         tmp->damage_char[1], tmp->damage_structural[1]);
            k += field_string("See Also", buf + k);
            k += sprintf(buf + k, "&Wprofile %s&n\r\n", first);
        }
    }
    else if ( IS_SET(obj->usage_flags, USE_FIRE) && (x = 1) )
    {
        k += sprintf(buf + k, "Extinguishes fires within the room.\r\n");
        k += field_string("Use", buf + k);
        k += sprintf(buf + k, "&Wuse %s\r\n", first);
    }
    else if ( IS_SET(obj->usage_flags, USE_TANKKIT) && (x = 1) )
    {
        k += sprintf(buf + k, "Aids structural repair to vehicles.\r\n");
        k += field_string("Use", buf + k);
        k += sprintf(buf + k, "&Wuse %s\r\n", first);
        k += field_string("Heals By", buf + k);
        k += sprintf(buf + k, "%d", obj->damage_char[0]);
    }
    else if ( IS_SET(obj->usage_flags, USE_EVAC) && (x = 1) )
    {
        k += sprintf(buf + k, "Evacuates (transports) player.\r\n");
        k += field_string("Use", buf + k);
        k += sprintf(buf + k, "&Wuse %s &X<&nx&X> <&ny&X>\r\n", first);
    }
    else if ( IS_SET(obj->usage_flags, USE_BACKUP) && (x = 1) )
    {
        k += sprintf(buf + k, "Drops in support troops.\r\n");
        k += field_string("Use", buf + k);
        k += sprintf(buf + k, "&Wuse %s &X[<&nx&X> <&ny&X>]\r\n", first);
        k += field_string("Note", buf + k);
        k += sprintf(buf + k,
                     "If used without arguments, the troops will follow you.\r\n");
    }
    else if ( IS_SET(obj->usage_flags, USE_HEAL) && (x = 1) )
    {
        k += sprintf(buf + k, "Heals a character.\r\n");
        k += field_string("Use", buf + k);
        k += sprintf(buf + k, "&Wuse %s &X[&nperson&X]\r\n", first);
        k += field_string("Heals By", buf + k);
        k += sprintf(buf + k, "%d", obj->damage_char[0]);
    }
    else if ( IS_SET(obj->usage_flags, USE_TURRET) && (x = 1) )
    {
        int foo;
        calc_weapon_dam(get_eq_char(turret, WEAR_WIELD), &dlower, &dupper,
                        &foo);
        k += sprintf(buf + k,
                     "Constructs a stationary, autofiring turret.\r\n");
        k += field_string("Use", buf + k);
        k += sprintf(buf + k, "&Wuse %s\r\n", first);
        k += field_string("Damage Range", buf + k);
        k += sprintf(buf + k, "%d - %d\r\n", dlower, dupper);
    }
    else if ( IS_SET(obj->usage_flags, USE_MAKE_DROID) && (x = 1) )
    {
        int foo;
        calc_weapon_dam(get_eq_char(pill_box, WEAR_WIELD), &dlower,
                        &dupper, &foo);
        k += sprintf(buf + k, "Creates a sentinel attack droid.\r\n");
        k += field_string("Use", buf + k);
        k += sprintf(buf + k, "&Wuse %s\r\n", first);
        k += field_string("Damage Range", buf + k);
        k += sprintf(buf + k, "%d - %d\r\n", dlower, dupper);
    }
    else if ( IS_SET(obj->usage_flags, USE_MAKE_SEEKER_DROID) && (x = 1) )
    {
        int foo;
        calc_weapon_dam(get_eq_char(pill_box, WEAR_WIELD), &dlower,
                        &dupper, &foo);
        k += sprintf(buf + k, "Creates a seeker attack droid.\r\n");
        k += field_string("Use", buf + k);
        k += sprintf(buf + k, "&Wuse %s &X<&ntarget&X>\r\n", first);
        k += field_string("Damage Range", buf + k);
        k += sprintf(buf + k, "%d - %d\r\n", dlower, dupper);
    }
    else if ( IS_SET(obj->usage_flags, USE_MAKE_WALL) && (x = 1) )
    {
        k += sprintf(buf + k, "Construct a temporary wall.\r\n");
        k += field_string("Use", buf + k);
        k += sprintf(buf + k, "&Wuse %s &X<&ndir&X>\r\n", first);
    }
    else if ( IS_SET(obj->usage_flags, USE_HEALTH_MONITOR) && (x = 1) )
    {
        k += sprintf(buf + k,
                     "Remotely monitor a character's health.\r\n");
        k += field_string("Use", buf + k);
        k += sprintf(buf + k, "&Wuse %s &X<&ntarget&X>\r\n", first);
        k += field_string("Charges", buf + k);
        k += sprintf(buf + k, "%d remaining", obj->ammo);
    }
    else if ( IS_SET(obj->usage_flags, USE_BATTLEMAP) && (x = 1) )
    {
        k += sprintf(buf + k, "Get an overview of the battle field.\r\n");
        k += field_string("Use", buf + k);
        k += sprintf(buf + k, "&Wuse %s &X<&ntarget&X>\r\n", first);
        k += field_string("Charges", buf + k);
        k += sprintf(buf + k, "%d remaining", obj->ammo);
    }
    else if ( IS_SET(obj->usage_flags, USE_MAKE_OBJ_ROOM) && (x = 1) )
    {
        k += sprintf(buf + k, "Spawns another object in the room when used.\r\n");
        k += field_string("Use", buf + k);
        k += sprintf(buf + k, "&Wuse %s\r\n", first);
        k += field_string("Creates", buf + k);
        k += sprintf(buf + k, "%s", obj->spawnTrail->short_descr);
    }
    else if ( IS_SET(obj->usage_flags, USE_SATELLITE) && (x = 1) )
    {
        k += sprintf(buf + k, "Shows a topological map of the topmost level.\r\n");
        k += field_string("Use", buf + k);
        k += sprintf(buf + k, "&Wuse %s\r\n", first);
    }

    if ( x == 1 )
        send_to_char(buf, ch);
}


void
profile_weapon (struct char_data *ch, struct obj_data *wpn)
{
    char buf[MAX_STRING_LENGTH];
    int dupper = 0, dlower = 0;
    int k = 0;
    int amax;

    calc_weapon_dam(wpn, &dlower, &dupper, &amax);

    if ( !IS_SET(wpn->general_flags, GEN_NO_AMMO_NEEDED) )
    {
        k += field_string("Ammo Type", buf + k);
        k += sprintf(buf + k, "%s\r\n",
                     IFNOT(ammo_types[wpn->ammo_type], "undefined"));
        k += field_string("Max Ammo", buf + k);
        k += sprintf(buf + k, "%d\r\n", amax);
    }

    k += field_string("Range", buf + k);
    k += sprintf(buf + k, "%d\r\n", wpn->range);

    if ( wpn->rounds_delay > 0 )
    {
        k += field_string("Delay", buf + k);
        k += sprintf(buf + k, "%d round%s\r\n", wpn->rounds_delay,
                     wpn->rounds_delay == 1 ? "" : "s");
    }

    k += field_string("Rounds per Second", buf + k);
    k += sprintf(buf + k, "%d RPS\r\n", wpn->rounds_per_second);
    k += field_string("Damage Mult", buf + k);
    k += sprintf(buf + k, "%.2f\r\n", wpn->dam_mult);
    k += field_string("Approx Damage", buf + k);
    k += sprintf(buf + k, "%d - %d per round\r\n", dlower, dupper);

    k += field_string("Handedness", buf + k);

    if ( IS_SET(wpn->wear_flags, ITEM_TWO_HANDS) )
        k += sprintf(buf + k, "two-handed\r\n");
    else if ( IS_SET(wpn->wear_flags, ITEM_SEC_HAND) )
        k += sprintf(buf + k, "either (dualable)\r\n");
    else
        k += sprintf(buf + k, "either\r\n");

    send_to_char(buf, ch);
}


void
profile_armor (struct char_data *ch, struct obj_data *obj)
{
    char buf[MAX_STRING_LENGTH];
    int k = 0;

    k += field_string("Worn On", buf + k);
    k += sprintf(buf + k, "%s\r\n",
                 wear_bit_name(obj->wear_flags & ~ITEM_TAKE));
    k += field_string("Absorption", buf + k);
    k += sprintf(buf + k, "%d damage\r\n", obj->armor);

    send_to_char(buf, ch);
}


void
profile_explosive (struct char_data *ch, struct obj_data *obj)
{
    char buf[MAX_STRING_LENGTH];
    int k = 0;

    k += field_string("Bio. Damage", buf + k);
    k += sprintf(buf + k, "%d held / %d in room / %d shrapnel\r\n",
                 obj->damage_char[0], obj->damage_char[1],
                 obj->damage_char[2]);

    k += field_string("Struct. Damage", buf + k);
    k += sprintf(buf + k, "%d held / %d in room / %d shrapnel\r\n",
                 obj->damage_structural[0], obj->damage_structural[1],
                 obj->damage_structural[2]);

    send_to_char(buf, ch);
}


void
profile_ammo (struct char_data *ch, struct obj_data *obj)
{
    char buf[MAX_STRING_LENGTH];
    int k = 0;

    k += field_string("Contains", buf + k);
    k += sprintf(buf + k, "%d shots\r\n", obj->ammo);
    k += field_string("Damage Range", buf + k);
    k += sprintf(buf + k, "%d - %d per shot\r\n",
                 UMAX(1, obj->damage_char[0] - 200), obj->damage_char[0]);

    send_to_char(buf, ch);
}
