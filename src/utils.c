/*
 * Module:      utils.c
 * Owner:       Satori
 * Checked-in:  11 Oct, 2000
 * Version:     0.0.0a
 *
 * Various utility routines.  These are terribly useful things that didn't
 * really belong any where else -- so they all arrived here.
 */


#include    "ground0.h"
#include    "localconfig.h"

int
get_line (FILE * fp, char *buf, size_t nbuf)
{
    register char *ptr;
    int lines = 0;

    do
    {
        if ( !(ptr = fgets(buf, nbuf, fp)) )
            return -1;
        lines++;
        *(buf + strlen(buf) - 1) = '\0';
    }
    while ( !*buf || *buf == '*' );

    return lines;
}


void
touch (const char *fname)
{
    FILE *fp = fopen(fname, "a");

    if ( !fp )
    {
        char buf[128];

        sprintf(buf, "touch(%s)", fname);
        logerr(buf);
        return;
    }

    fclose(fp);
}


int
copy_file (const char *oldf, const char *newf)
{
    char buf[MAX_INPUT_LENGTH];
    int errorState = 0;
    FILE *fpOld;
    FILE *fpNew;
    int len;

    if ( !(fpOld = fopen(oldf, "r")) )
    {
        logerr("copy_file:%s", oldf);
        return -1;
    }
    else if ( !(fpNew = fopen(newf, "w")) )
    {
        logerr("copy_file:%s", newf);
        fclose(fpOld);
        return -1;
    }

    while ( (len = fread(buf, MAX_INPUT_LENGTH, 1, fpOld)) > 0 )
        if ( fwrite(buf, len, 1, fpNew) != len )
        {
            logerr("copy_file");
            errorState = -1;
            break;
        }

    fclose(fpOld);
    fclose(fpNew);
    return errorState;
}


void
append_file (CHAR_DATA * ch, char *file, char *str)
{
    FILE *fp;

    if ( IS_NPC(ch) || str[0] == '\0' )
        return;

    if ( (fp = fopen(file, "a")) == NULL )
    {
        logerr(file);
        send_to_char("Could not open the file!\n\r", ch);
    }
    else
    {
        fprintf(fp, "%s: %s\n", ch->names, str);
        fclose(fp);
    }
}



void
log_base_player (CHAR_DATA * ch, char *str)
{
    char fname[MAX_STRING_LENGTH];
    time_t ct = time(0);
    char *timestr;

    if ( ch->log_fp == NULL )
    {
        sprintf(fname, "../log/players/%s.log", ch->names);
        ch->log_fp = fopen(fname, "a");
    }

    if ( ch->log_fp == NULL || !str || !*str )
        return;

    timestr = asctime(localtime(&ct));
    *(timestr + strlen(timestr) - 1) = '\0';

    fprintf(ch->log_fp, "%d %s: %s\n", iteration, timestr, str);
    fflush(ch->log_fp);
}


void
log_player (CHAR_DATA * ch, char *fmt, ...)
{
    char buf[MAX_STRING_LENGTH];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, MAX_STRING_LENGTH, fmt, ap);
    va_end(ap);

    log_base_player(ch, buf);
}


void
log_base_string (const char *mesg)
{
    time_t ct = time(0);
    char *timestr = asctime(localtime(&ct));

    *(timestr + strlen(timestr) - 1) = '\0';
    fprintf(stderr, "%d %s: %s\n", iteration, timestr, mesg);
    fflush(stderr);
}


void
logmesg (const char *fmt, ...)
{
    char buf[MAX_STRING_LENGTH];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, MAX_STRING_LENGTH, fmt, ap);
    va_end(ap);

    log_base_string(buf);

#ifdef LOG_STDOUT
    fprintf(stdout, buf);
    fprintf(stdout, "\r\n");
#endif
}


void
logerr (const char *fmt, ...)
{
    char buf[MAX_STRING_LENGTH];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, MAX_STRING_LENGTH, fmt, ap);
    va_end(ap);

    logmesg("%s: %s", buf, strerror(errno));
}


void
wizlog (CHAR_DATA * ch, OBJ_DATA * obj, long flag, long flag_skip,
       int min_level, const char *fmt, ...)
{
    char buf[MAX_STRING_LENGTH];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, MAX_STRING_LENGTH, fmt, ap);
    va_end(ap);

    log_base_string(buf);
    wiznet(buf, ch, obj, flag, flag_skip, min_level);
}


int
number_range (int from, int to)
{
    if ( from > to )
    {
        int swap = from;

        from = to;
        to = swap;
    }
    else if ( from == to )
        return from;

    return random() % (to - from + 1) + from;
}


int
CountBits (register uint bitv)
{
    register int count = 0;

    while ( bitv )
    {
        count++;
        bitv &= bitv - 1;
    }
    return count;
}
