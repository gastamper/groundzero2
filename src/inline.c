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
 * Module:      inline.c
 * Synopsis:    Inline control sequence processing.
 * Author:      Satori
 * Created:     01 Dec 2001
 *
 * NOTES:
 */

#include "ground0.h"

// Since we have a C99-compatable compiler now:
const bool esc_terminator[256] = {
    [0x40] = 1,[0x41] = 1,[0x42] = 1,[0x43] = 1,[0x44] = 1,
    [0x45] = 1,[0x46] = 1,[0x47] = 1,[0x48] = 1,[0x49] = 1,
    [0x4A] = 1,[0x4B] = 1,[0x4C] = 1,[0x4D] = 1,[0x4E] = 1,
    [0x4F] = 1,[0x50] = 1,[0x51] = 1,[0x52] = 1,[0x53] = 1,
    [0x54] = 1,[0x55] = 1,[0x56] = 1,[0x57] = 1,[0x58] = 1,
    [0x59] = 1,[0x5A] = 1,[0x5B] = 1,[0x5C] = 1,[0x5D] = 1,
    [0x5E] = 1,[0x5F] = 1,[0x60] = 1,[0x61] = 1,[0x62] = 1,
    [0x63] = 1,[0x64] = 1,[0x65] = 1,[0x66] = 1,[0x67] = 1,
    [0x68] = 1,[0x69] = 1,[0x6A] = 1,[0x6B] = 1,[0x6C] = 1,
    [0x6D] = 1,[0x6E] = 1,[0x6F] = 1,[0x70] = 1,[0x71] = 1,
    [0x72] = 1,[0x73] = 1,[0x74] = 1,[0x75] = 1,[0x76] = 1,
    [0x77] = 1,[0x78] = 1,[0x79] = 1,[0x7A] = 1,[0x7B] = 1,
    [0x7C] = 1,[0x7D] = 1,[0x7E] = 1
};

// Required trusts for user input codes:
const int codeTrustLevel[256] = {
    ['_'] = 1,['{'] = 1,['}'] = 1,['u'] = 1,['('] = 8,
    [')'] = 8,['%'] = 8,['!'] = 8,['i'] = 1,['?'] = 8
};

#define STO(_)                                                          \
 ({                                                                     \
    if ( szbuf < 1 ) {    /* Out of space. */                           \
        if ( inesc )  /* In escape sequence, back out. */               \
            while ( backout-- > 0 ) buf--;                              \
        *buf = '\0';                                                    \
        return orig - base;                                             \
    } else if ((_) == '\x1B' && !inesc) {                               \
        inesc = 1;                                                      \
        backout = 0;                                                    \
        *(buf++) = '\x1B';                                              \
        szbuf--;                                                        \
    } else if (inesc && esc_terminator[(int)(_)]) {                     \
        inesc = 0;                                                      \
        *(buf++) = (_);                                                 \
        szbuf--;                                                        \
    } else {        /* Store a regular character. */                    \
        backout++;                                                      \
        *(buf++) = (_);                                                 \
        szbuf--;                                                        \
    }                                                                   \
    (_);                                                                \
})


int
ColorTranslate (char c)
{
    switch (LOWER(c))
    {
        case TCON_TOK:
            return 256;
        case '_':
            return 257;
        case '{':
            return 258;
        case '}':
            return 259;
        case 'u':
            return 260;
        case '`':
            return 261;
        case '(':
            return 262;
        case ')':
            return 263;
        case '%':
            return 264;
        case '!':
            return 265;
        case 'i':
            return 266;
        case '?':
            return 267;
        case 'x':
            return 30;
        case 'r':
            return 31;
        case 'g':
            return 32;
        case 'y':
            return 33;
        case 'b':
            return 34;
        case 'm':
            return 35;
        case 'c':
            return 36;
        case 'w':
            return 37;
        case 'n':
            return 0;
        default:
            return -1;
    }
}


#define CNUM(_)         ch->pcdata->_

int
x_GetUserColor (struct char_data *ch, char code)
{
    int cn;

    switch (code)
    {
        case 'A':
            cn = CNUM(color_combat_o);
            break;
        case 'a':
            cn = CNUM(color_action);
            break;
        case 'w':
            cn = CNUM(color_wizi);
            break;
        case 'X':
            cn = CNUM(color_xy);
            break;
        case 'L':
            cn = CNUM(color_level);
            break;
        case 'H':
            cn = CNUM(color_hp);
            break;
        case 'D':
            cn = CNUM(color_desc);
            break;
        case 'O':
            cn = CNUM(color_obj);
            break;
        case 's':
            cn = CNUM(color_say);
            break;
        case 't':
            cn = CNUM(color_tell);
            break;
        case 'r':
            cn = CNUM(color_reply);
            break;
        case 'E':
            cn = CNUM(color_exits);
            break;
        case 'C':
            cn = CNUM(color_combat_condition_s);
            break;
        case 'c':
            cn = CNUM(color_combat_condition_o);
            break;
        default:
            return -1;
    }

    if ( cn <= 7 )
        return 30 + cn;
    if ( cn <= 15 )
        return (30 + (cn % 8)) | (1 << 26);

    return -1;
}

#undef CNUM

int
ProcessColors (struct descriptor_data *d, bool strip, const char *orig,
              int szorig, char *buf, int szbuf)
{
    static char *protoHeader = "http://";
    static int PROTO_LENGTH = 7;
    int protoPlace = 0;

    struct term_data *td = (d ? &d->termdata : NULL);
    const char *base = orig;
    bool invisible = false;
    bool inesc = false;
    bool inurl = false;
    int backout = 0;
    int clr;

    if ( !strchr(orig, TCON_TOK) )
    {
        strncpy(buf, orig, szbuf);
        buf[szbuf - 1] = '\0';
        return 0;
    }

    /* Subtract one for the terminating NUL. */
    szbuf--;

    for ( ; szorig && *orig; szorig--, orig++ )
    {
        if ( *orig == TCON_TOK && *(orig + 1) )
        {
            szorig--;

            if ( inurl && *(orig + 1) != '?' )
            {
                STO(TCON_TOK);
                continue;
            }

            switch ((clr = ColorTranslate(*(++orig))))
            {
                case -1:        /* Error. */
                    STO(TCON_TOK);
                    STO(*orig);
                    break;
                case 256:       /* TCON_TOK. */
                    STO(TCON_TOK);
                    break;
                case 257:       /* Newline. */
                    STO('\r');
                    STO('\n');
                    break;
                case 258:       /* Save color state. */
                    if ( strip || !d )
                        break;
                    memcpy((char *) &d->store_termdata,
                           (const char *) td, sizeof(struct term_data));
                    break;
                case 259:       /* Restore color state. */
                    if ( strip || !d )
                        break;

                    memcpy((char *) td,
                           (const char *) &d->store_termdata,
                           sizeof(struct term_data));

                    /* Write the entire code back out. */
                    STO('\x1B');
                    STO('[');
                    STO('0' + td->isbold);
                    STO(';');
                    if ( td->isflash )
                    {
                        STO('5');
                        STO(';');
                    }
                    else if ( td->isbold )
                    {
                        STO('2');
                        STO('5');
                        STO(';');
                    }
                    STO('3');
                    STO('0' + td->fg - 30);
                    STO('m');
                    break;
                case 260:       /* User defined colors. */
                    if (strip || !d || !d->character ||
                        !d->character->pcdata)
                    {
                        invisible = false;
                        szorig--;
                        orig++;
                        break;
                    }

                    szorig--;
                    orig++;
                    clr = x_GetUserColor(d->character, *orig);
                    if ( clr == -1 )
                        break;

                    /* Remove intense bit. */
                    int fgc = clr & ~(1 << 26);
                    bool bold = clr & (1 << 26) ? 1 : 0;

                    if ( !td || td->isbold != bold || td->fg != fgc )
                    {
                        if ( !td || td->isbold != bold )
                        {
                            STO('\x1B');
                            STO('[');
                            if ( bold )
                            {
                                STO('1');
                                if ( td )
                                    td->isbold = 1;
                            }
                            else
                            {
                                STO('0');
                                if ( td )
                                {
                                    td->isbold = 0;
                                    td->fg = 0;
                                    if ( td->isflash )
                                    {
                                        STO(';');
                                        STO('5');
                                    }
                                }
                            }
                            STO('m');
                        }

                        if ( !td || td->fg != fgc )
                        {
                            STO('\x1B');
                            STO('[');
                            STO('3');
                            STO('0' + (fgc - 30));
                            STO('m');
                            td->fg = fgc;
                        }
                    }
                    break;
                case 261:       /* Tilde. */
                    STO('~');
                    break;
                case 262:       /* Flashing on */
                    if ( strip || (td && td->isflash) )
                    {
                        invisible = false;
                        break;
                    }
                    td->isflash = 1;
                    STO('\x1B');
                    STO('[');
                    STO('5');
                    STO('m');
                    break;
                case 263:       /* Flashing off. */
                    if ( strip )
                    {
                        invisible = false;
                        break;
                    }
                    td->isflash = 0;
                    STO('\x1B');
                    STO('[');
                    STO('2');
                    STO('5');
                    STO('m');
                    break;
                case 264:       /* Clear screen. */
                    if ( strip )
                    {
                        invisible = false;
                        break;
                    }
                    STO('\x1B');
                    STO('[');
                    STO('2');
                    STO('J');
                    break;
                case 265:       /* Beep. */
                    STO('\a');
                    break;
                case 266:       /* Invisible. */
                    if ( strip )
                    {
                        invisible = true;
                        break;
                    }
                    STO('\x1B');
                    STO('[');
                    STO('0');
                    STO(';');
                    STO('4');
                    STO('0');
                    STO(';');
                    if ( !number_range(0, 2) )
                    {
                        STO('1');
                        STO(';');
                        STO('2');
                        STO('2');
                        STO(';');
                    }
                    STO('3');
                    STO('0');
                    STO('m');
                    td->fg = 30;
                    td->isbold = 0;
                    td->isflash = 0;
                    break;
                case 267:
                    inurl = false;
                    break;
                case 0: /* Reset. */
                    if ( strip )
                    {
                        invisible = false;
                        break;
                    }
                    STO('\x1B');
                    STO('[');
                    STO('0');
                    STO(';');
                    STO('3');
                    STO('7');
                    STO('m');
                    td->fg = 37;
                    td->isbold = 0;
                    td->isflash = 0;
                    break;
                default:        /* Colors. */
                    if ( strip )
                    {
                        invisible = false;
                        break;
                    }

                    char tmp[64];
                    char *tmp_ptr = tmp;

                    *(tmp_ptr++) = '\x1B';
                    *(tmp_ptr++) = '[';

                    /* Foreground color. */
                    if ( isupper(*orig) && (!td || !td->isbold) )
                    {
                        *(tmp_ptr++) = '1';
                        *(tmp_ptr++) = ';';
                        if ( td )
                            td->isbold = 1;
                    }
                    else if ( islower(*orig) && (!td || td->isbold) )
                    {
                        *(tmp_ptr++) = '0';
                        *(tmp_ptr++) = ';';

                        if ( td )
                        {
                            td->fg = 37;
                            td->isbold = 0;
                            if ( td->isflash )
                            {
                                *(tmp_ptr++) = '5';
                                *(tmp_ptr++) = ';';
                            }
                        }
                    }

                    if ( !td || td->fg != clr )
                    {
                        *(tmp_ptr++) = '3';
                        *(tmp_ptr++) = '0' + (clr - 30);
                        *(tmp_ptr++) = ';';
                        if ( td )
                            td->fg = clr;
                    }

                    if ( *(tmp_ptr - 1) == ';' )
                    {
                        *(tmp_ptr - 1) = 'm';
                        *tmp_ptr = '\0';

                        for ( tmp_ptr = tmp; *tmp_ptr; tmp_ptr++ )
                            STO(*tmp_ptr);
                    }

                    break;
            }
        }
        else if ( !invisible || *orig == '\r' || *orig == '\n' )
        {
            if ( inurl )
            {
                inurl = !isspace(*orig);
            }
            else if ( *orig == protoHeader[protoPlace] )
            {
                if ( ++protoPlace >= PROTO_LENGTH )
                {
                    inurl = true;
                    protoPlace = 0;
                }
            }
            else
            {
                protoPlace = 0;
            }

            STO(*orig);
        }
    }

    *buf = '\0';
    return 0;
}
