
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


void disownObject(struct obj_data *);
void claimObject(struct char_data *, struct obj_data *);

DECLARE_DO_FUN (do_look);


/*
 * Local functions.
 */
void scatter_obj (OBJ_DATA * obj);
bool can_see_linear_char (CHAR_DATA * ch, CHAR_DATA * victim, int shooting, bool second);
bool can_see_linear_room (CHAR_DATA * ch, ROOM_DATA * a_room);
bool rooms_linear_with_no_walls (ROOM_DATA * room1, ROOM_DATA * room2);
CHAR_DATA *char_file_active (char *argument);

/* returns a flag for wiznet */
long
wiznet_lookup (const char *name)
{
    int flag;

    for ( flag = 0; wiznet_table[flag].name != NULL; flag++ )
    {
        if ( LOWER(name[0]) == LOWER(wiznet_table[flag].name[0] )
            && !str_prefix(name, wiznet_table[flag].name))
            return flag;
    }

    return -1;
}

/*
 * Retrieve a character's trusted level for permission checking.
 */
int
get_trust (CHAR_DATA * ch)
{
    return ch->trust;
}

/*
 * Retrieve a character's age.
 */
int
get_age (CHAR_DATA * ch)
{
    return 17 + (ch->played + (int) (current_time - ch->logon)) / 72000;
}


/*
 * Retrieve a character's carry capacity.
 */
int
can_carry_n (CHAR_DATA * ch)
{
    if ( !IS_NPC(ch) && ch->trust )
        return 1000;

    return 150;
}



/*
 * Retrieve a character's carry capacity.
 */
int
can_carry_w (CHAR_DATA * ch)
{
    if ( !IS_NPC(ch) && ch->trust )
        return 1000000;

    return 500;
}


bool
is_name (char *str, char *namelist)
{
    char name[MAX_INPUT_LENGTH], part[MAX_INPUT_LENGTH];
    char *list, *string;


    string = str;
    /* we need ALL parts of string to match part of namelist */
    for ( ;; )
    {                           /* start parsing string */
        str = one_argument(str, part);

        if ( part[0] == '\0' )
            return TRUE;

        /* check to see if this is part of namelist */
        list = namelist;
        for ( ;; )
        {                       /* start parsing namelist */
            list = one_argument(list, name);
            if ( name[0] == '\0' )        /* this name was not found */
                return FALSE;

            if ( !str_cmp(string, name) )
                return TRUE;    /* full pattern match */

            if ( !str_prefix(part, name) )
                break;
        }
    }
}

bool
is_name_obj (char *str, char *namelist)
{
    char name[MAX_INPUT_LENGTH], part[MAX_INPUT_LENGTH];
    char *list, *string;

    string = str;
    /* we need ALL parts of string to match part of namelist */
    for ( ;; )
    {                           /* start parsing string */
        str = one_argument(str, part);

        if ( part[0] == '\0' )
            return TRUE;

        /* check to see if this is part of namelist */
        list = namelist;
        for ( ;; )
        {                       /* start parsing namelist */
            list = one_argument(list, name);
            if ( part[0] == '!' )
            {
                if ( name[0] == '\0' )    /* this name was not found */
                    break;

                if ( !str_prefix(part + 1, name) )
                    return FALSE;
            }
            else
            {
                if ( name[0] == '\0' )    /* this name was not found */
                    return FALSE;

                if ( !str_prefix(part, name) )
                    break;
            }
        }
    }
}

bool
is_name_exact (char *str, char *namelist)
{
    char name[MAX_INPUT_LENGTH], part[MAX_INPUT_LENGTH];
    char *list, *string;


    string = str;
    /* we need ALL parts of string to match part of namelist */
    for ( ;; )
    {                           /* start parsing string */
        str = one_argument(str, part);

        if ( part[0] == '\0' )
            return TRUE;

        /* check to see if this is part of namelist */
        list = namelist;
        for ( ;; )
        {                       /* start parsing namelist */
            list = one_argument(list, name);
            if ( name[0] == '\0' )        /* this name was not found */
                return FALSE;

            if ( !str_cmp(string, name) )
                return TRUE;    /* full pattern match */

            if ( !str_cmp(part, name) )
                break;
        }
    }
}



/*
 * Move a char out of a room.
 */
void
char_from_room (CHAR_DATA * ch)
{
    if ( ch->in_room == NULL )
    {
        logmesg("Error: char_from_room: in_room == NULL for %s", ch->names);
        exit(-1);
    }

    if ( ch == ch->in_room->people )
    {
        ch->in_room->people = ch->next_in_room;
    }
    else
    {
        CHAR_DATA *prev;

        for ( prev = ch->in_room->people; prev; prev = prev->next_in_room )
        {
            if ( prev->next_in_room == ch )
            {
                prev->next_in_room = ch->next_in_room;
                break;
            }
        }

        if ( prev == NULL )
            logmesg("Erorr: char_from_room: ch not found.");
    }

    ch->in_room = NULL;
    ch->next_in_room = NULL;
}



/*
 * Move a char into a room.
 */
void
char_to_room (CHAR_DATA * ch, ROOM_DATA * pRoomIndex)
{
    if ( pRoomIndex == NULL )
    {
        logmesg("Error: char_to_room: pRoomIndex == NULL for %s.", ch->names);
        return;
    }

    ch->in_room = pRoomIndex;
    ch->next_in_room = pRoomIndex->people;
    pRoomIndex->people = ch;
}

void
bug_object_state (OBJ_DATA * obj)
{
    if ( obj->in_room )
        logmesg("in room %d, %d, %d.", obj->in_room->x, obj->in_room->y,
                   obj->in_room->level);
    if ( obj->destination )
        logmesg("has destination %d, %d, %d.", obj->destination->x,
                   obj->destination->y, obj->destination->level);
    if ( obj->in_obj )
        logmesg("inside object %s.", obj->in_obj->name);
    if ( obj->carried_by )
        logmesg("carried by %s.", obj->carried_by->names);
}


/*
 * Give an obj to a char.
 */
void
obj_to_char (OBJ_DATA * obj, CHAR_DATA * ch)
{
    if ( obj->in_room || obj->destination || obj->in_obj || obj->carried_by )
    {
        logmesg("obj_to_char: %s could not go to %s because it is "
                   "somewhere.", obj->name, ch->names);
        bug_object_state(obj);
        return;
    }
    obj->next_content = ch->carrying;
    ch->carrying = obj;
    obj->carried_by = ch;
}



/*
 * Take an obj from its character.
 */
void
obj_from_char (OBJ_DATA * obj)
{
    CHAR_DATA *ch;

    if ( (ch = obj->carried_by) == NULL )
    {
        logmesg("Error: obj_from_char: NULL ch.");
        bug_object_state(obj);
        return;
    }

    if ( obj->wear_loc != WEAR_NONE )
        unequip_char(ch, obj);

    if ( ch->carrying == obj )
    {
        ch->carrying = obj->next_content;
    }
    else
    {
        OBJ_DATA *prev;

        for ( prev = ch->carrying; prev != NULL; prev = prev->next_content )
        {
            if ( prev->next_content == obj )
            {
                prev->next_content = obj->next_content;
                break;
            }
        }

        if ( prev == NULL )
            logmesg("Error: obj_from_char: obj not in list.");
    }

    obj->carried_by = NULL;
    obj->next_content = NULL;
}


/*
 * Find a piece of eq on a character.
 */
OBJ_DATA *
get_eq_char (CHAR_DATA * ch, int iWear)
{
    OBJ_DATA *obj;

    if ( ch == NULL )
        return NULL;

    for ( obj = ch->carrying; obj != NULL; obj = obj->next_content )
    {
        if ( obj->carried_by != ch )
        {
            if ( obj->carried_by )
                logmesg("Error: get_eq_char: Carried by %s; in list of %s",
                           obj->carried_by->names, ch->names);
            else
                logmesg("Error: get_eq_char: Carried by null; in list of %s",
                           ch->names);
        }
        if ( obj->wear_loc == iWear )
            return obj;
    }

    return NULL;
}

/*
 * Find a piece of eq on a character.
 */
OBJ_DATA *
find_eq_char (CHAR_DATA * ch, int search_type, unsigned int search_value)
{
    OBJ_DATA *obj;

    if ( !ch )
        return NULL;

    for ( obj = ch->carrying; obj != NULL; obj = obj->next_content )
    {
        if ( obj->carried_by != ch )
            logmesg("find_eq_char: carried_by not ch");
        switch (search_type)
        {
            case SEARCH_ITEM_TYPE:
                if ( obj->item_type == search_value )
                    return obj;
                break;
            case SEARCH_GEN_FLAG:
                if ( IS_SET(obj->general_flags, search_value) )
                    return obj;
                break;
            case SEARCH_AMMO_TYPE:
                if ( (obj->item_type == ITEM_AMMO ) &&
                    (obj->ammo_type == search_value))
                    return obj;
                break;
        }
    }

    return NULL;
}


/* Find a worn piece of eq on a char. */
OBJ_DATA *
find_worn_eq_char (CHAR_DATA * ch, int search_type,
                  unsigned int search_value)
{
    OBJ_DATA *obj;
    int i;

    if ( ch == NULL )
        return NULL;

    for ( i = 0; i < MAX_WEAR; i++ )
    {
        if ( !(obj = get_eq_char(ch, i)) )
            continue;

        switch (search_type)
        {
            case SEARCH_ITEM_TYPE:
                if ( obj->item_type == search_value )
                    return obj;
                break;
            case SEARCH_GEN_FLAG:
                if ( IS_SET(obj->general_flags, search_value) )
                    return obj;
                break;
            case SEARCH_AMMO_TYPE:
                if ( (obj->item_type == ITEM_AMMO ) &&
                    (obj->ammo_type == search_value))
                    return obj;
                break;
        }
    }

    return NULL;
}


/*
 * Equip a char with an obj.
 */
void
equip_char (CHAR_DATA * ch, OBJ_DATA * obj, int iWear)
{
    if ( get_eq_char(ch, iWear) != NULL )
    {
        logmesg("Equip_char: %s already equipped (%d).", ch->names, iWear);
        return;
    }

    ch->armor += obj->armor;
    obj->wear_loc = iWear;
}



/*
 * Unequip a char with an obj.
 */
void
unequip_char (CHAR_DATA * ch, OBJ_DATA * obj)
{
    if ( obj->wear_loc == WEAR_NONE )
    {
        logmesg("Unequip_char: %s already unequipped %s.", ch->names, obj->name);
        return;
    }

    obj->wear_loc = -1;
    ch->armor -= obj->armor;
}


int
count_carried (CHAR_DATA * ch)
{
    OBJ_DATA *obj;
    int count = 0;

    for ( obj = ch->carrying; obj; obj = obj->next_content )
        if ( obj->wear_loc == WEAR_NONE )
            count++;

    return (count);
}



/*
 * Count occurrences of an obj in a list.
 */
int
count_obj_list (OBJ_INDEX_DATA * pObjIndex, OBJ_DATA * list)
{
    OBJ_DATA *obj;
    int nMatch;

    nMatch = 0;
    for ( obj = list; obj != NULL; obj = obj->next_content )
    {
        if ( obj->pIndexData == pObjIndex )
            nMatch++;
    }

    return nMatch;
}



/*
 * Move an obj out of a room.
 */
void
obj_from_room (OBJ_DATA * obj)
{
    ROOM_DATA *in_room;

    if ( (in_room = obj->in_room) == NULL )
    {
        logmesg("Error: obj_from_room: NULL room for %s.", obj->name);
        return;
    }

    if ( obj == in_room->contents )
    {
        in_room->contents = obj->next_content;
    }
    else
    {
        OBJ_DATA *prev;

        for ( prev = in_room->contents; prev; prev = prev->next_content )
        {
            if ( prev->next_content == obj )
            {
                prev->next_content = obj->next_content;
                break;
            }
        }

        if ( prev == NULL )
        {
            logmesg("Error: obj_from_room: obj not found.");
            return;
        }
    }

    if ( (obj->in_room == explosive_area) && obj->in_room->people )
    {
        act("$p flashes out of existence to be destroyed somewhere.",
            obj->in_room->people, obj, NULL, TO_ROOM);
        act("$p flashes out of existence to be destroyed somewhere.",
            obj->in_room->people, obj, NULL, TO_CHAR);
    }

    if ( IS_SET(obj->general_flags, GEN_DARKS_ROOM) )
        REMOVE_BIT(obj->in_room->room_flags, ROOM_DARK);
    if ( IS_SET(obj->general_flags, GEN_CHOKES_ROOM) )
        REMOVE_BIT(obj->in_room->room_flags, ROOM_GAS);

    obj->in_room = NULL;
    obj->next_content = NULL;
}



/*
 * Move an obj into a room.
 */
void
obj_to_room (OBJ_DATA * obj, ROOM_DATA * pRoomIndex)
{
    if (obj->in_room ||
        (obj->destination && pRoomIndex != explosive_area &&
         pRoomIndex != deploy_area) || obj->in_obj || obj->carried_by)
    {
        logmesg("obj_to_room: %s could not go to %d,%d,%d because "
                   "it is somewhere.", obj->name, pRoomIndex->x,
                   pRoomIndex->y, pRoomIndex->level);

        if ( obj->in_room || obj->destination )
        {
            ROOM_DATA *rm =
                (obj->in_room ? obj->in_room : obj->destination);

            logmesg("somewhere is %p (%s [%d, %d] - L%d",
                       rm, rm->name, rm->x, rm->y, rm->level);
        }
        else if ( obj->in_obj )
            logmesg("somewhere is in object %s",
                       obj->in_obj->short_descr);
        else if ( obj->carried_by )
            logmesg("somewhere is on character %s",
                       obj->carried_by->names);
        else
            logmesg("don't know where somewhere is");

        bug_object_state(obj);
        return;
    }

    if ( (pRoomIndex == explosive_area) && (explosive_area->people) )
    {
        act("$p flashes into existance.", explosive_area->people, obj,
            NULL, TO_ROOM);
        act("$p flashes into existance.", explosive_area->people, obj,
            NULL, TO_CHAR);
    }

    if ( IS_SET(obj->general_flags, GEN_DARKS_ROOM) )
        SET_BIT(pRoomIndex->room_flags, ROOM_DARK);
    if ( IS_SET(obj->general_flags, GEN_CHOKES_ROOM) )
        SET_BIT(pRoomIndex->room_flags, ROOM_GAS);

    obj->next_content = pRoomIndex->contents;
    pRoomIndex->contents = obj;
    obj->in_room = pRoomIndex;
    obj->carried_by = NULL;
}



/*
 * Move an object into an object.
 */
void
obj_to_obj (OBJ_DATA * obj, OBJ_DATA * obj_to)
{
    if ( obj->in_room || obj->destination || obj->in_obj || obj->carried_by )
    {
        logmesg("obj_to_obj: %s could not go to %s because it is "
                   "somewhere.", obj->name, obj_to->name);
        bug_object_state(obj);
        return;
    }

    obj->next_content = obj_to->contains;
    obj_to->contains = obj;
    obj->in_obj = obj_to;
    obj->carried_by = NULL;
}

void
obj_from_obj (OBJ_DATA * obj)
{
    OBJ_DATA *tmp;

    if ( !obj->in_obj )
    {
        logmesg("Error: obj_from_obj: %s not in an object", obj->name);
        bug_object_state(obj);
        return;
    }
    else if ( obj == obj->in_obj->contains )
    {
        obj->in_obj->contains = obj->in_obj->contains->next_content;
        obj->in_obj = NULL;
        return;
    }

    tmp = obj->in_obj->contains;

    while ( tmp && tmp->next_content != obj )
        tmp = tmp->next_content;

    if ( !tmp )
    {
        logmesg("Error: obj_from_obj: %s not in contents list of container %s",
                   obj->name, obj->in_obj->name);
        bug_object_state(obj);
        return;
    }

    tmp->next_content = obj->next_content;
    obj->in_obj = NULL;
    obj->next_content = NULL;
}


void
scatter_obj (OBJ_DATA * obj)
{
    ROOM_DATA *temp_room = NULL;

    if ( IS_SET(obj->general_flags, GEN_DEPLOYMENT ) &&
        (obj->in_room || obj->destination))
        return;

    if ( obj->in_room || obj->destination || obj->in_obj || obj->carried_by )
    {
        logmesg
            ("Error: %s could not be recycled because it still is somewhere.",
             obj->name);
        bug_object_state(obj);
        return;
    }

    if ( (obj->item_type == ITEM_AMMO ) && !expand_event && !fBootDb &&
        !IS_SET(obj->general_flags, GEN_NODEPOT))
        temp_room = ammo_repop[number_range(0, 2)];
    else
        temp_room = index_room(the_city->rooms_on_level,
                               number_range(0, the_city->x_size - 1),
                               number_range(0, the_city->y_size - 1));

    obj_to_room(obj, temp_room);

    if ( temp_room->people )
    {
        act("$p glides down to earth on a small parachute to land at "
            "your feet.", temp_room->people, obj, NULL, TO_ROOM);
        act("$p glides down to earth on a small parachute to land at "
            "your feet.", temp_room->people, obj, NULL, TO_CHAR);
    }
}

/*
 * Extract an obj from the world.
 */
void
x_extract_obj (OBJ_DATA * obj, int perm_extract, const char *file, int line, const char *func)
{
    OBJ_DATA *new_obj;

    if ( obj->valid != VALID_VALUE )
    {
        logmesg(log_buf,
                   "Error: x_extract_obj: the object was not set to valid (%s).",
                   obj->name);
        return;
    }

    if ( obj->in_room != NULL )
        obj_from_room(obj);
    else if ( obj->carried_by != NULL )
        obj_from_char(obj);

    if ( !perm_extract )
    {
        new_obj = create_object(obj->pIndexData, 0);
        scatter_obj(new_obj);
    }

    if ( obj->interior )
    {
        CHAR_DATA *the_dead, *dead_next;
        OBJ_DATA *extra_obj, *extra_next;

//  Is this logmesg necessary?
        logmesg("x_extract_obj: Taking out the interior of %s.", obj->name);
        for (the_dead = obj->interior->people; the_dead;
             the_dead = dead_next)
        {
            dead_next = the_dead->next_in_room;
            the_dead->last_hit_by = obj->owner;
            char_death(the_dead, "container death");
        }

        for (extra_obj = obj->interior->contents; extra_obj;
             extra_obj = extra_next)
        {
            extra_next = extra_obj->next_content;
            extract_obj(extra_obj, extra_obj->extract_me);
        }
        free_mem(obj->interior, sizeof(ROOM_DATA));
        obj->interior = NULL;
    }

    if ( obj->item_type == ITEM_TEAM_VEHICLE && obj->vehicle != NULL )
    {
        /* No longer necessary to free the strings; they're shared. */
        free_mem(obj->vehicle, sizeof(struct obj_vehicle_data));
    }

    while ( obj->contains )
    {
        new_obj = obj->contains;
        obj->contains = obj->contains->next_content;
        extract_obj(new_obj, new_obj->extract_me);
    }

    if ( object_list == obj )
    {
        object_list = obj->next;
    }
    else
    {
        OBJ_DATA *prev;

        for ( prev = object_list; prev != NULL; prev = prev->next )
        {
            if ( prev->next == obj )
            {
                prev->next = obj->next;
                break;
            }
        }

        if ( prev == NULL )
        {
            logmesg("%s:%d: from %s: Extract_obj: obj %d not found.",
                       file, line, func, obj->pIndexData->vnum);
            return;
        }
    }

    obj->valid = 0;
    free_string(obj->name);
    free_string(obj->description);
    free_string(obj->short_descr);

    if ( obj->explode_desc )
        free_string(obj->explode_desc);

    if ( obj->owner != NULL )
        disownObject(obj);

    --obj->pIndexData->count;
    obj->next = obj_free;
    obj_free = obj;
}



/*
 * Extract a char from the world.
 */
void
extract_char (CHAR_DATA * ch, bool fPull)
{
    CHAR_DATA *wch;

    if ( ch->in_room == NULL && ch->desc != NULL )
    {
        void coredump(void);

        logmesg("Error: extract_char: NULL room for %s.", ch->names);
        coredump();
        return;
    }

    if ( IS_SET(ch->temp_flags, IN_VEHICLE | IS_VEHICLE) )
    {
        stop_manning(ch);
        REMOVE_BIT(ch->temp_flags, IN_VEHICLE | IS_VEHICLE);
    }

    ch->fighting = NULL;

    char_from_room(ch);

    for ( wch = char_list; wch != NULL; wch = wch->next )
    {
        if ( wch->last_hit_by == ch )
            wch->last_hit_by = NULL;
        if ( wch->fighting == ch )
            wch->fighting = NULL;

        if ( wch->chasing == ch )
        {
            wch->chasing = NULL;
            wch->ld_behavior = BEHAVIOR_PILLBOX;
        }

        if ( wch->owner == ch )
            wch->owner = NULL;
    }

    if ( ch->genned_by )
    {
        OBJ_DATA *radio = ch->genned_by;

        ch->genned_by = NULL;
        radio->generic_counter--;

        if ( radio->generic_counter <= 0 )
            extract_obj(radio, radio->extract_me);
    }

    if ( !fPull )
    {
        char_to_room(ch, safe_area);

        if ( !IS_NPC(ch) && ch->desc )
        {
            do_look(ch, "auto");

            act("A mangled and bloody corpse arrives in a box carried by several "
                "orderlies, \n\rwho dump the unsightly remains into a large "
                "machine with the words\n\rREGENERATION UNIT inscribed on the "
                "side.  Several seconds later\n\ra door on the side opens and "
                "$n steps out grinning from ear to ear.", ch, NULL, NULL, TO_ROOM);
        }

        return;
    }

    for ( wch = char_list; wch != NULL; wch = wch->next )
    {
        if ( wch->reply == ch )
            wch->reply = NULL;
        if ( wch->last_team_kill == ch )
            wch->last_team_kill = NULL;
    }

    if ( ch == char_list )
    {
        char_list = ch->next;
    }
    else
    {
        CHAR_DATA *tmp = char_list;

        while ( tmp && tmp->next != ch )
            tmp = tmp->next;

        if ( tmp )
            tmp->next = ch->next;
    }

    if ( ch->desc )
        ch->desc->character = NULL;

    if ( ch == next_violence )
        next_violence = ch->next;

    while ( ch->owned_objs )
        extract_obj(ch->owned_objs, ch->owned_objs->extract_me);

    for ( struct descriptor_data * d = descriptor_list; d; d = d->next )
        if ( d->character && d->character->dubbing == ch )
            d->character->dubbing = NULL;

    ch->valid = 0;
    free_char(ch);
}



/*
 * Find a char in the room.
 */
CHAR_DATA *
get_char_room (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *rch;
    int number;
    int count;

    number = number_argument(argument, arg);
    count = 0;

    if ( !str_cmp(arg, "self") )
        return ch;
    if ( !ch->in_room )
        return NULL;

    if ( ch->pcdata )
    {
        if ( !str_cmp(arg, "him" ) && ch->pcdata->him &&
            ch->pcdata->him->in_room == ch->in_room)
            return ch->pcdata->him;
        else if ( !str_cmp(arg, "her" ) && ch->pcdata->her &&
                 ch->pcdata->her->in_room == ch->in_room)
            return ch->pcdata->her;
        else if ( !str_cmp(arg, "it" ) && ch->pcdata->it_is_chardata &&
                 ch->pcdata->it.cd &&
                 ch->pcdata->it.cd->in_room == ch->in_room)
            return ch->pcdata->it.cd;
    }

    for ( rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room )
    {
        if ( !can_see(ch, rch) || !is_name(arg, rch->names) )
            continue;
        if ( ++count == number )
        {
            if ( ch->pcdata )
            {
                switch (rch->sex)
                {
                    case SEX_MALE:
                        ch->pcdata->him = rch;
                        break;
                    case SEX_FEMALE:
                        ch->pcdata->her = rch;
                        break;
                    default:
                        ch->pcdata->it_is_chardata = 1;
                        ch->pcdata->it.cd = rch;
                        break;
                }
            }
            return rch;
        }
    }

    return NULL;
}




/*
 * Find a char in the world.
 */
CHAR_DATA *
get_char_world (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *wch;
    int number;
    int count;

    if ( ch && ((wch = get_char_room(ch, argument)) != NULL) )
        return wch;
    if ( ch && ch->pcdata )
    {
        if ( !str_cmp(arg, "him") && ch->pcdata->him )
            return ch->pcdata->him;
        else if ( !str_cmp(arg, "her") && ch->pcdata->her )
            return ch->pcdata->her;
        else if ( !str_cmp(arg, "it" ) && ch->pcdata->it_is_chardata &&
                 ch->pcdata->it.cd)
            return ch->pcdata->it.cd;
    }

    number = number_argument(argument, arg);
    count = 0;
    for ( wch = char_list; wch != NULL; wch = wch->next )
    {
        if ( wch->in_room == NULL || (ch && !can_see(ch, wch) )
            || !is_name(arg, wch->names))
            continue;
        if ( ++count == number )
        {
            if ( ch && ch->pcdata )
            {
                switch (wch->sex)
                {
                    case SEX_MALE:
                        ch->pcdata->him = wch;
                        break;
                    case SEX_FEMALE:
                        ch->pcdata->her = wch;
                        break;
                    default:
                        ch->pcdata->it_is_chardata = 1;
                        ch->pcdata->it.cd = wch;
                        break;
                }
            }
            return wch;
        }
    }

    return NULL;
}

CHAR_DATA *
char_file_active (char *argument)
{
    CHAR_DATA *wch;
    char name_cap1[MAX_INPUT_LENGTH], *name_cap2;
    sh_int count;

    for ( wch = char_list; wch != NULL; wch = wch->next )
    {
        name_cap2 = capitalize(argument);
        for ( count = 0; name_cap2[count]; count++ )
            name_cap1[count] = name_cap2[count];
        name_cap1[count] = 0;
        if ( !strcmp(name_cap1, capitalize(wch->names)) )
            return wch;
    }

    return NULL;
}



/*
 * Find some object with a given index data.
 * Used by area-reset 'P' command.
 */
OBJ_DATA *
get_obj_type (OBJ_INDEX_DATA * pObjIndex)
{
    OBJ_DATA *obj;

    for ( obj = object_list; obj != NULL; obj = obj->next )
    {
        if ( obj->pIndexData == pObjIndex )
            return obj;
    }

    return NULL;
}


/*
 * Find an obj in a list.
 */
OBJ_DATA *
get_obj_list (CHAR_DATA * ch, char *argument, OBJ_DATA * list)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;

    number = number_argument(argument, arg);
    count = 0;

    if ( number == 1 && !str_cmp(arg, "it" ) && ch && ch->pcdata &&
        !ch->pcdata->it_is_chardata && ch->pcdata->it.od)
    {
        for ( obj = list; obj; obj = obj->next_content )
            if ( obj == ch->pcdata->it.od )
                return ch->pcdata->it.od;
    }

    for ( obj = list; obj != NULL; obj = obj->next_content )
    {
        if ( is_name_obj(arg, obj->name) )
        {
            if ( ++count == number )
            {
                if ( ch && ch->pcdata )
                {
                    ch->pcdata->it_is_chardata = 0;
                    ch->pcdata->it.od = obj;
                }
                return obj;
            }
        }
    }

    return NULL;
}



/*
 * Find an obj in player's inventory.
 */
OBJ_DATA *
get_obj_carry (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;

    number = number_argument(argument, arg);
    count = 0;

    if ( number == 1 && !str_cmp(arg, "it" ) && ch && ch->pcdata &&
        !ch->pcdata->it_is_chardata && ch->pcdata->it.od)
    {
        for ( obj = ch->carrying; obj; obj = obj->next_content )
            if ( obj->wear_loc == WEAR_NONE && obj == ch->pcdata->it.od )
                return ch->pcdata->it.od;
    }

    for ( obj = ch->carrying; obj != NULL; obj = obj->next_content )
    {
        if ( obj->carried_by != ch )
            logmesg("Error: get_obj_carry: carried_by not ch.");
        if ( obj->wear_loc == WEAR_NONE && is_name_obj(arg, obj->name) )
        {
            if ( ++count == number )
            {
                if ( ch->pcdata )
                {
                    ch->pcdata->it_is_chardata = 0;
                    ch->pcdata->it.od = obj;
                }
                return obj;
            }
        }
    }

    return NULL;
}


/*
 * Find an obj in player's equipment.
 */
OBJ_DATA *
get_obj_wear (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;

    number = number_argument(argument, arg);
    count = 0;

    if ( number == 1 && !str_cmp(arg, "it" ) && ch && ch->pcdata &&
        !ch->pcdata->it_is_chardata && ch->pcdata->it.od)
    {
        for ( obj = ch->carrying; obj; obj = obj->next_content )
            if ( obj->wear_loc != WEAR_NONE && obj == ch->pcdata->it.od )
                return ch->pcdata->it.od;
    }

    for ( obj = ch->carrying; obj != NULL; obj = obj->next_content )
    {
        if ( obj->carried_by != ch )
            logmesg("Error: get_obj_wear: carried_by not ch");
        if ( obj->wear_loc != WEAR_NONE && is_name_obj(arg, obj->name) )
        {
            if ( ++count == number )
            {
                if ( ch->pcdata )
                {
                    ch->pcdata->it_is_chardata = 0;
                    ch->pcdata->it.od = obj;
                }
                return obj;
            }
        }
    }

    return NULL;
}



/*
 * Find an obj in the room or in inventory.
 */
OBJ_DATA *
get_obj_here (CHAR_DATA * ch, char *argument)
{
    OBJ_DATA *obj;

    obj = get_obj_list(ch, argument, ch->in_room->contents);
    if ( obj != NULL )
        return obj;

    if ( (obj = get_obj_carry(ch, argument)) != NULL )
        return obj;

    if ( (obj = get_obj_wear(ch, argument)) != NULL )
        return obj;

    return NULL;
}



/*
 * Find an obj in the world.
 */
OBJ_DATA *
get_obj_world (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;

    if ( (obj = get_obj_here(ch, argument)) != NULL )
        return obj;

    number = number_argument(argument, arg);
    count = 0;

    if ( number == 1 )
    {
        if ( !str_cmp(arg, "it" ) && ch && ch->pcdata &&
            !ch->pcdata->it_is_chardata && ch->pcdata->it.od)
            return ch->pcdata->it.od;

        for ( obj = object_list; obj != NULL; obj = obj->next )
        {
            if ( is_name_exact(arg, obj->name) )
            {
                if ( ++count == number )
                    return obj;
            }
        }
        count = 0;
    }
    for ( obj = object_list; obj != NULL; obj = obj->next )
    {
        if ( is_name(arg, obj->name) )
        {
            if ( ++count == number )
                return obj;
        }
    }
    return NULL;
}


/*
 * Return # of objects which an object counts as.
 * Thanks to Tony Chamberlain for the correct recursive code here.
 */
int
get_obj_number (OBJ_DATA * obj)
{
/*    int number;

   if (  obj->item_type == ITEM_CONTAINER )
   number = 0;
   else
   number = 1;

   for (  obj = obj->contains; obj != NULL; obj = obj->next_content  )
   number += get_obj_number( obj );

   return number; */
    return 1;
}


/*
 * True if room is private.
 */
bool
room_is_private (ROOM_DATA * pRoomIndex)
{
    CHAR_DATA *rch;
    int count;

    count = 0;
    for ( rch = pRoomIndex->people; rch != NULL; rch = rch->next_in_room )
        count++;

    if ( IS_SET(pRoomIndex->room_flags, ROOM_PRIVATE) && count >= 2 )
        return TRUE;

    if ( IS_SET(pRoomIndex->room_flags, ROOM_SOLITARY) && count >= 1 )
        return TRUE;

    return FALSE;
}


int
count_people (struct room_data *rm)
{
    struct char_data *ich = rm->people;
    int count = 0;

    while ( ich )
    {
        count++;
        ich = ich->next_in_room;
    }

    return (count);
}


/* visibility on a room -- for entering and exits */
bool
can_see_room (CHAR_DATA * ch, ROOM_DATA * pRoomIndex)
{
    if ( IS_SET(pRoomIndex->room_flags, ROOM_GODS_ONLY) && !IS_IMMORTAL(ch) )
        return FALSE;

    return TRUE;
}



/*
 * True if char can see victim.
 */
bool
can_see (CHAR_DATA * ch, CHAR_DATA * victim)
{
    OBJ_DATA *prot_obj;

    /* Can never see players/mobiles in the graveyard w/o holylight. */
    if ( !IS_SET(ch->act, PLR_HOLYLIGHT ) &&
         (victim->in_room == graveyard_area ||
          victim->in_room == store_area))
        return (FALSE);

    if ( (ch == victim) ||
         (IS_NPC(ch) &&
          (ch->in_room->level || ch->ld_behavior == BEHAVIOR_GUARD)) )
        return TRUE;

    if ( !IS_NPC(victim )
        && IS_SET(victim->act, PLR_WIZINVIS)
        && get_trust(ch) < victim->invis_level)
        return FALSE;

    if ( (!IS_NPC(ch) && IS_SET(ch->act, PLR_HOLYLIGHT) )
        || (IS_NPC(ch) && IS_IMMORTAL(ch)))
        return TRUE;

    if ( IS_SET(ch->affected_by, AFF_BLIND) )
        return FALSE;

    if ( (ch->in_room && IS_SET(ch->in_room->room_flags, ROOM_DARK) ) ||
        (victim->in_room &&
         IS_SET(victim->in_room->room_flags, ROOM_DARK)))
        if ( ((prot_obj = get_eq_char(ch, WEAR_HEAD)) == NULL ) ||
            !IS_SET(prot_obj->general_flags, GEN_SEE_IN_DARK))
            return FALSE;

    return TRUE;
}

int
max_sight (CHAR_DATA * ch)
{
    OBJ_DATA *attach;
    OBJ_DATA *obj;
    int range = 0;

    if ( (obj = get_eq_char(ch, WEAR_WIELD)) != NULL )
        range =
            obj->range + (IS_SET(obj->general_flags, GEN_SEE_FAR) ? 3 : 0);
    else if ( (obj = get_eq_char(ch, WEAR_SEC)) != NULL )
        range =
            obj->range + (IS_SET(obj->general_flags, GEN_SEE_FAR) ? 3 : 0);
    else
        return (3);

    for ( attach = obj->contains; attach; attach = attach->next_content )
        if ( attach->item_type == ITEM_SCOPE )
            range += attach->range;

    return (UMAX(3, range));
}


int
max_sight_fire (CHAR_DATA * ch, bool second)
{
    OBJ_DATA *attach;
    int addRange = 0;
    OBJ_DATA *obj;

    if ( !second && (obj = get_eq_char(ch, WEAR_WIELD)) == NULL )
        return 0;
    else if ( second && (obj = get_eq_char(ch, WEAR_SEC)) == NULL )
        return 0;

    for ( attach = obj->contains; attach; attach = attach->next_content )
        if ( attach->item_type == ITEM_SCOPE )
            addRange += attach->range;

    return (obj->range + addRange);
}


bool
can_see_linear_char (CHAR_DATA * ch, CHAR_DATA * victim, int shooting,
                    bool second)
{
    sh_int diff_x, diff_y, direction, count, pos_neg;
    ROOM_DATA *temp_room;
    int shooting_sight;

    if ( shooting )
        shooting_sight = max_sight_fire(ch, second);
    else
        shooting_sight = max_sight(ch);

    if ( can_see(ch, victim) )
    {
        if ( ch->in_room->level != victim->in_room->level )
            return FALSE;
        if ( ch->in_room == victim->in_room )
            return TRUE;
        if ( (ch->in_room->level < 0) || (ch->in_room->x < 0 ) ||
            (ch->in_room->y < 0))
            return FALSE;
        if ( ch->in_room->x == victim->in_room->x )
        {
            diff_x = 0;
            diff_y = ch->in_room->y - victim->in_room->y;
            if ( (diff_y > shooting_sight) || (diff_y < -shooting_sight) )
                return FALSE;
            else if ( diff_y > 0 )
                direction = 2;
            else
                direction = 0;
        }
        else if ( ch->in_room->y == victim->in_room->y )
        {
            diff_x = ch->in_room->x - victim->in_room->x;
            diff_y = 0;
            if ( (diff_x > shooting_sight) || (diff_x < -shooting_sight) )
                return FALSE;
            else if ( diff_x > 0 )
                direction = 3;
            else
                direction = 1;
        }
        else
            return FALSE;
        pos_neg = (diff_x > 0) ? -1 : ((diff_x < 0) ? 1 : 0);
        if ( !pos_neg )
            pos_neg = (diff_y > 0) ? -1 : ((diff_y < 0) ? 1 : 0);
        for ( count = 0; count != (diff_x ? -diff_x : -diff_y );
             count += pos_neg)
        {
            temp_room =
                index_room(ch->in_room, (diff_x ? count : 0),
                           (diff_y ? count : 0));
            if ( temp_room->exit[direction] )
                return FALSE;
        }
        return TRUE;
    }

    return FALSE;
}

bool
can_see_linear_room (CHAR_DATA * ch, ROOM_DATA * a_room)
{
    sh_int diff_x, diff_y, direction, count, pos_neg;
    ROOM_DATA *temp_room;

    if ( ch->in_room->level != a_room->level )
        return FALSE;
    if ( ch->in_room == a_room )
        return TRUE;
    if ( ch->in_room->x == a_room->x )
    {
        diff_x = 0;
        diff_y = ch->in_room->y - a_room->y;
        if ( (diff_y > max_sight(ch)) || (diff_y < -max_sight(ch)) )
            return FALSE;
        else if ( diff_y > 0 )
            direction = 2;
        else
            direction = 0;
    }
    else if ( ch->in_room->y == a_room->y )
    {
        diff_x = ch->in_room->x - a_room->x;
        diff_y = 0;
        if ( (diff_x > max_sight(ch)) || (diff_x < -max_sight(ch)) )
            return FALSE;
        else if ( diff_x > 0 )
            direction = 3;
        else
            direction = 1;
    }
    else
        return FALSE;
    pos_neg = (diff_x > 0) ? -1 : ((diff_x < 0) ? 1 : 0);
    if ( !pos_neg )
        pos_neg = (diff_y > 0) ? -1 : ((diff_y < 0) ? 1 : 0);
    for ( count = 0; count != (diff_x ? -diff_x : -diff_y );
         count += pos_neg)
    {
        temp_room =
            index_room(ch->in_room, (diff_x ? count : 0),
                       (diff_y ? count : 0));
        if ( temp_room->exit[direction] )
            return FALSE;
    }
    return TRUE;
}

bool
rooms_linear_with_no_walls (ROOM_DATA * room1, ROOM_DATA * room2)
{
    sh_int diff_x, diff_y, direction, count, pos_neg;
    ROOM_DATA *temp_room;

    if ( room1->level != room2->level )
        return FALSE;
    if ( room1 == room2 )
        return TRUE;
    if ( room1->x == room2->x )
    {
        diff_x = 0;
        diff_y = room1->y - room2->y;
        if ( diff_y > 0 )
            direction = 2;
        else
            direction = 0;
    }
    else if ( room1->y == room2->y )
    {
        diff_x = room1->x - room2->x;
        diff_y = 0;
        if ( diff_x > 0 )
            direction = 3;
        else
            direction = 1;
    }
    else
        return FALSE;
    pos_neg = (diff_x > 0) ? -1 : ((diff_x < 0) ? 1 : 0);
    if ( !pos_neg )
        pos_neg = (diff_y > 0) ? -1 : ((diff_y < 0) ? 1 : 0);
    for ( count = 0; count != (diff_x ? -diff_x : -diff_y );
         count += pos_neg)
    {
        temp_room =
            index_room(room1, (diff_x ? count : 0), (diff_y ? count : 0));
        if ( temp_room->exit[direction] )
            return FALSE;
    }
    return TRUE;
}

/*
 * Return ascii name of an item type.
 */
char *
item_type_name (OBJ_DATA * obj)
{
    switch (obj->item_type)
    {
        case ITEM_MISC:
            return "misc";
        case ITEM_WEAPON:
            return "weapon";
        case ITEM_ARMOR:
            return "armor";
        case ITEM_EXPLOSIVE:
            return "explosive";
        case ITEM_AMMO:
            return "ammo";
        case ITEM_TEAM_VEHICLE:
            return "team vehicle";
        case ITEM_TEAM_ENTRANCE:
            return "team entrance";
        case ITEM_SCOPE:
            return "scope";
    }

    logmesg("Error:S Item_type_name: unknown type %d for %s.", 
	obj->item_type, obj->name);
    return "(unknown)";
}


/* return ascii name of an act vector */
char *
act_bit_name (int act_flags)
{
    static char buf[512];

    buf[0] = '\0';

    if ( act_flags & PLR_AGGRO_ALL )
        strcat(buf, " agg_all");
    if ( act_flags & PLR_AGGRO_OTHER )
        strcat(buf, " agg_other");
    if ( act_flags & PLR_HOLYLIGHT )
        strcat(buf, " holy_light");
    if ( act_flags & PLR_WIZINVIS )
        strcat(buf, " wizinvis");
    if ( act_flags & PLR_NOLEADER )
        strcat(buf, " noleader");
    if ( act_flags & PLR_REPOP )
        strcat(buf, " basepop");
    if ( act_flags & PLR_BADPOP )
        strcat(buf, " unlucky_pop");
    if ( act_flags & PLR_RANKONLY )
        strcat(buf, " rankonly");
    if ( act_flags & PLR_NONOTE )
        strcat(buf, " nonote");
    if ( act_flags & PLR_LOG )
        strcat(buf, " log");
    if ( act_flags & PLR_DENY )
        strcat(buf, " deny");
    if ( act_flags & PLR_FREEZE )
        strcat(buf, " frozen");
    if ( act_flags & PLR_TRAITOR )
        strcat(buf, " traitor");
    if ( act_flags & PLR_NOVOTE )
        strcat(buf, " novote");
    if ( act_flags & PLR_FREEZE_STATS )
        strcat(buf, " statfreeze");
    if ( act_flags & PLR_SUSPECTED_BOT )
        strcat(buf, " tweakbot");
    return (buf[0] != '\0') ? buf + 1 : "none";
}

char *
comm_bit_name (int comm_flags)
{
    static char buf[512];

    buf[0] = '\0';

    if ( comm_flags & COMM_QUIET )
        strcat(buf, " quiet");
    if ( comm_flags & COMM_NOWIZ )
        strcat(buf, " no_wiz");
    if ( comm_flags & COMM_COMPACT )
        strcat(buf, " compact");
    if ( comm_flags & COMM_BRIEF )
        strcat(buf, " brief");
    if ( comm_flags & COMM_PROMPT )
        strcat(buf, " prompt");
    if ( comm_flags & COMM_COMBINE )
        strcat(buf, " combine");
    if ( comm_flags & COMM_NOEMOTE )
        strcat(buf, " no_emote");
    if ( comm_flags & COMM_NOSHOUT )
        strcat(buf, " no_shout");
    if ( comm_flags & COMM_NOTELL )
        strcat(buf, " no_tell");
    if ( comm_flags & COMM_NOCHANNELS )
        strcat(buf, " no_channels");

    return (buf[0] != '\0') ? buf + 1 : "none";
}


char *
wear_bit_name (int wear_flags)
{
    static char buf[512];

    buf[0] = '\0';
    if ( wear_flags & ITEM_TAKE )
        strcat(buf, " take");
    if ( wear_flags & ITEM_WEAR_FINGER )
        strcat(buf, " finger");
    if ( wear_flags & ITEM_WEAR_NECK )
        strcat(buf, " neck");
    if ( wear_flags & ITEM_WEAR_BODY )
        strcat(buf, " torso");
    if ( wear_flags & ITEM_WEAR_HEAD )
        strcat(buf, " head");
    if ( wear_flags & ITEM_WEAR_LEGS )
        strcat(buf, " legs");
    if ( wear_flags & ITEM_WEAR_FEET )
        strcat(buf, " feet");
    if ( wear_flags & ITEM_WEAR_HANDS )
        strcat(buf, " hands");
    if ( wear_flags & ITEM_WEAR_ARMS )
        strcat(buf, " arms");
    if ( wear_flags & ITEM_WEAR_SHIELD )
        strcat(buf, " shield");
    if ( wear_flags & ITEM_WEAR_ABOUT )
        strcat(buf, " body");
    if ( wear_flags & ITEM_WEAR_WAIST )
        strcat(buf, " waist");
    if ( wear_flags & ITEM_WEAR_WRIST )
        strcat(buf, " wrist");
    if ( wear_flags & ITEM_WIELD )
        strcat(buf, " wield");
    if ( wear_flags & ITEM_SEC_HAND )
        strcat(buf, " second");
    if ( wear_flags & ITEM_HOLD )
        strcat(buf, " hold");
    if ( wear_flags & ITEM_TWO_HANDS )
        strcat(buf, " twohands");

    return (buf[0] != '\0') ? buf + 1 : "none";
}


void
disownObject (struct obj_data *obj)
{
    struct obj_data *tmp;

    if ( obj->owner == NULL )
        return;

    if ( obj == obj->owner->owned_objs )
    {
        obj->owner->owned_objs = obj->owner->owned_objs->next_owned;
        return;
    }

    tmp = obj->owner->owned_objs;

    while ( tmp && tmp->next_owned != obj )
        tmp = tmp->next_owned;

    if ( !tmp || tmp->next_owned != obj )
        return;

    tmp->next_owned = obj->next_owned;
    obj->owner = NULL;
}


void
claimObject (struct char_data *ch, struct obj_data *obj)
{
    if ( obj->owner == ch )
        return;
    else if ( obj->owner )
        disownObject(obj);

    obj->owner = ch;
    obj->next_owned = ch->owned_objs;
    ch->owned_objs = obj;
}


struct obj_data *
find_ammo (struct obj_data *obj)
{
    struct obj_data *ammo;

    for ( ammo = obj->contains; ammo; ammo = ammo->next_content )
        if ( ammo->item_type == ITEM_AMMO )
            break;

    return (ammo);
}
