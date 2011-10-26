/*
 * This is special code for ranked players, moved here because it was really
 * cluttering up other stuff.
 */

#include  "ground0.h"


#undef    HIGH_RANK_REP
#define   HIGH_RANK_RATIO     1

#define   INCARNATE_RANK      RANK_BADASS


static int killsReq[RANK_MAX] = {
    0,
    0,
    250,
    1000,
    5000,
    10000
};

static int repReq[RANK_MAX] = {
    -2500,
    0,
    0,
    0,
#ifdef HIGH_RANK_REP
    5000,
    10000
#else
    0,
    0
#endif
};

static int ratReq[RANK_MAX] = {
    0,
    0,
    0,
    0,
#ifdef HIGH_RANK_RATIO
    2,
    2
#else
    0,
    0
#endif
};

const char *ranks[RANK_MAX] = {
    "&cJOSH",
    "",
    "&BNOVICE",
    "&XMERC",
    "&GHUNTER",
    "&RBADASS"
};

#undef W
#define W(x)    (match == -1 ? 0 : x[match])

static inline bool
meetRatioReq (CHAR_DATA * ch, int x, int old)
{
    if ( ratReq[x] < 0 )
    {
        if ( ch->deaths >= ch->kills * -ratReq[x] && ratReq[x] <= old )
            return TRUE;
    }
    else
    {
        if ( ch->kills >= ch->deaths * ratReq[x] && ratReq[x] >= old )
            return TRUE;
    }

    return FALSE;
}


static inline bool
meetRepReq (CHAR_DATA * ch, int x, int old)
{
    if ( repReq[x] < 0 )
    {
        if ( REP(ch) <= repReq[x] && repReq[x] <= old )
            return TRUE;
    }
    else if ( repReq[x] > 0 )
    {
        if ( REP(ch) >= repReq[x] && repReq[x] >= old )
            return TRUE;
    }
    else
        return TRUE;

    return FALSE;
}


int
RANK (CHAR_DATA * ch)
{
    int match = RANK_UNRANKED;
    int x;

    if ( IS_NPC(ch) )
        return RANK_UNRANKED;
    if ( REP(ch) <= -5000 )
        return RANK_JOSH;

    for ( x = 0; x < RANK_MAX; x++ )
    {
        if (ch->kills >= killsReq[x] &&
            (match == -1 || killsReq[x] >= killsReq[match]) &&
            meetRepReq(ch, x, W(repReq)) && meetRatioReq(ch, x, W(ratReq)))
            match = x;
    }

    return (match == -1 ? RANK_UNRANKED : match);
}


void
report_new_rank (CHAR_DATA * ch, int old_rank)
{
    char buf[MAX_STRING_LENGTH];
    int rank = RANK(ch);

    if ( !rank )
        sprintf(buf, "%s%s&n is no longer ranked!  Hahah!\r\n",
                team_table[ch->team].namecolor, ch->names);
    else if ( rank < old_rank )
        sprintf(buf, "%s%s&n has dropped to the rank of %s&n.  Hahah!\r\n",
                team_table[ch->team].namecolor, ch->names, ranks[rank]);
    else
        sprintf(buf, "%s%s&n has advanced to the rank of %s!  Fear!\r\n",
                team_table[ch->team].namecolor, ch->names, ranks[rank]);

    do_gecho(NULL, buf);
}


int
check_special_attack (CHAR_DATA * ch, OBJ_DATA * weap)
{
    CHAR_DATA *victim = ch->fighting;

    if ( !ch->fighting || ch->fighting == ch )
        return 0;
    if ( !weap )
        return 0;

    /* Wielding an explosive. */
    if ( weap->item_type == ITEM_EXPLOSIVE )
    {
        return 0;
    }

    /* Weapon, but no ammo. */
    if (!weap->contains &&
        !IS_SET(weap->general_flags, GEN_NO_AMMO_NEEDED))
    {
        return 0;
    }

    /*
     * From herein, we know:
     *   - They have a weapon.
     *   - The weapon is loaded.
     *   - The weapon is not an explosive.
     */

    /* Death Incarnate */
#ifdef INCARNATE_RANK
    if ( RANK(ch ) >= INCARNATE_RANK && ch->hit < 2750 &&
        ch->in_room == victim->in_room &&
        number_range(0, 50 * killsReq[RANK_BADASS]) < ch->kills)
    {
        CHAR_DATA *next_ch;

        logmesg("%s goes ballistic -- DEATH INCARNATE!", ch->names);

        send_to_char
            ("Suddenly there's nothing but perfect silence, perfect peace.  You feel\r\n"
             "your eyes close, shutting out compassion, fear, even anger.  You only\r\n"
             "know one thing now: the icey grips of death.  You embrace it, know it,...\r\n"
             "control it.  You're dimly aware of your body moving with terrifying\r\n"
             "speed, breaking through flesh and bone.  There are a series of sickening\r\n"
             "cracks and last agonizing gasps as the world cascades back around you.\r\n"
             "You open your eyes to a world painted red with the blood of your enemies.\r\n"
             "All those who stood before you now lay broken at your feet.\r\n",
             ch);

        act("$n's eyes roll back into his head as his weapons drop from $s limp\r\n" "fingers.  A dark aura rises up around $m and the world seems to be\r\n" "perfectly still, perfectly peaceful.  A low quaking sound begins to\r\n" "form, but from where you're not sure.  It seems to be coming from\r\n" "all directions as it grows louder.\r\n\r\n" "$n's eyes snap open.  They are calm, deadly, and devoid of humanity.\r\n" "$s movements are a blur.  Faster than thought you find yourself face\r\n" "down on the ground, gasping for air through lungs that you no longer\r\n" "have.  $n HAS &YBECOME &RDEATH&n!", ch, NULL, NULL, TO_ROOM);

        for ( victim = ch->in_room->people; victim; victim = next_ch )
        {
            next_ch = victim->next_in_room;
            if ( victim == ch )
                continue;
            victim->last_hit_by = ch;
            char_death(victim, "death incarnate");
        }

        return 1;
    }
#endif

    return 0;
}
