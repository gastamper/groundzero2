/**
 * NAME:        telnet.c
 * SYNOPSIS:    TELNET protocol support, etc.
 * AUTHOR:      satori
 * CREATED:     14 Sep 2001
 * MODIFIED:    14 Sep 2001
 */

#define TELCMDS
#define TELOPTS
#define GZ2_TELNET_C_
#include "ground0.h"
#include "telnet.h"
#include "net.h"
#undef GZ2_TELNET_C_

const char go_ahead_str[] = { IAC, GA, '\0' };


bool write_to_descriptor(struct descriptor_data *, char *, int, bool);

static inline void x_optneg(struct descriptor_data *, unsigned char, char);
static inline void x_subneg(struct descriptor_data *);


#if 0
void
byte_print (struct descriptor_data *d, const char *buf, int nbuf, bool out)
{
    char xbuf[MAX_STRING_LENGTH];
    register int idx = 0;
    int i;

    logmesg("%s %s buf (%d bytes); telstate = %d", d->host,
               (out ? "=>" : "<="), nbuf, d->telstate);

    for ( i = 0; i < nbuf; i++ )
        if ( !isprint(buf[i]) || !isascii(buf[i]) )
            idx += sprintf(xbuf + idx, "[<%#X>]", (unsigned char) buf[i]);
        else
            xbuf[idx++] = buf[i];

    xbuf[idx] = '\0';
    logmesg("buf = %s", xbuf);
}
#endif


/*
 * Main TELNET Sequence Processor
 *
 * This used to be a full-blown state machine, but I got lazy and so I did a
 * quick rewrite to make it less efficient, but much more compact.  If we're
 * lucky, the string comparisons don't hurt too much and we're using GCC and
 * the static inlines all get treated nicely (x_<foo>) so that this code
 * simply expands into an almost exact equivalent of the state machine.  I
 * don't think we're so lucky, but we may not be off by as much as I would
 * have originally thought.  -satori
 */
void
process_telnet (struct descriptor_data *d, const unsigned char *buf,
               int nbuf)
{
    register char *inptr = d->inbuf + strlen(d->inbuf);
    char *subneg_ptr = d->subneg_buf + d->subneg_len;
    register int i;

    if ( nbuf <= 0 )              /* Nothing to process.        */
        return;

    for ( i = 0; i < nbuf; i++ )
    {
        switch (d->telstate)
        {
            case TELST_NOMINAL:
                if ( buf[i] == IAC )
                {
                    d->telstate = TELST_IAC;
                    break;
                }
                *(inptr++) = (char) buf[i];
                break;

            case TELST_IAC:
                /* Default to returning to nominal state; override as desired. */
                d->telstate = TELST_NOMINAL;

                switch (buf[i])
                {
                    case IAC:
                        *(inptr++) = (char) IAC;
                        break;
                    case IP:
                    case SUSP:
                        logmesg
                            ("IAC IP/SUSP received from client connection - disconnecting.");
                        close_socket(d);
                        return;
                    case BREAK:
                        break;
                    case AYT:
                        net_write(d->descriptor, "\a((Aye))\r\n");
                        break;
                    case WILL:
                    case WONT:
                    case DO:
                    case DONT:
                        d->telstate = TELST_WILL + (buf[i] - WILL);
                        break;
                    case SB:
                        d->telstate = TELST_SB;
                        subneg_ptr = d->subneg_buf;
                        d->subneg_len = 0;
                        break;
                    case SE:
                        x_subneg(d);
                        break;
                    default:
                        break;
                }
                break;

            case TELST_WILL:
            case TELST_WONT:
            case TELST_DO:
            case TELST_DONT:
                x_optneg(d, WILL + (d->telstate - TELST_WILL), buf[i]);
                d->telstate = TELST_NOMINAL;
                break;

            case TELST_SB:
                if ( buf[i] == IAC )
                {
                    d->telstate = TELST_SB_IAC;
                    break;
                }
                else if ( d->subneg_len < MAX_SB_LEN )
                {
                    *(subneg_ptr++) = buf[i];
                    d->subneg_len++;
                }
                break;

            case TELST_SB_IAC:
                if ( buf[i] == IAC )
                {
                    if ( d->subneg_len < MAX_SB_LEN )
                    {
                        *(subneg_ptr++) = (char) IAC;
                        d->subneg_len++;
                    }
                    d->telstate = TELST_SB;
                    break;
                }
                else if ( buf[i] == SE )
                {
                    d->telstate = TELST_NOMINAL;
                    x_subneg(d);
                    break;
                }

                logmesg("Should this happen?");
                d->telstate = TELST_NOMINAL;
                break;

            default:
                /* Something fucky happened.  Oh well. */
                logmesg
                    ("Something fucky happened (state=%d; buf[%d]=%d)",
                     d->telstate, i, buf[i]);
                d->telstate = TELST_NOMINAL;
                *(inptr++) = buf[i];
                break;
        }
    }

    *inptr = '\0';
}


static inline void
x_optneg (struct descriptor_data *d, unsigned char mode, char opt)
{
    char t[MAX_SB_LEN];
    int begin_compression(struct descriptor_data *);

    switch (mode)
    {
        case WILL:
            SET_TELOPT(d, opt, RECV_WILL);

            if ( !HAS_TELOPT(d, opt, SENT_DONT ) &&
                !HAS_TELOPT(d, opt, SENT_DO))
            {
                switch (opt)
                {
                    case TELOPT_TTYPE:
                    case TELOPT_NAWS:
                    case TELOPT_LFLOW:
                        IAC_option(d, DO, opt);
                        break;
                    default:
                        IAC_option(d, DONT, opt);
                        break;
                }

                break;
            }

            if ( opt == TELOPT_TTYPE )
            {
                t[0] = (char) TELQUAL_SEND;
                IAC_subneg(d, TELOPT_TTYPE, t, 1);
            }

            break;

        case WONT:
            /* If it's NOT a response to IAC DONT, confirm their WONT. */
            SET_TELOPT(d, opt, RECV_WONT);

            /* FIXME: Mindless agreement. */
            if ( !HAS_TELOPT(d, opt, SENT_DONT) )
            {
                IAC_option(d, DONT, opt);
                break;
            }

            break;

        case DO:
            SET_TELOPT(d, opt, RECV_DO);

            if ( !HAS_TELOPT(d, opt, SENT_WILL ) &&
                !HAS_TELOPT(d, opt, SENT_WONT))
            {
                switch (opt)
                {
                    case TELOPT_COMPRESS2:
                        IAC_option(d, WILL, opt);
                        break;
                    default:
                        IAC_option(d, WONT, opt);
                        break;
                }

                break;
            }

            switch (opt)
            {
                case TELOPT_COMPRESS2:
                    begin_compression(d);
                    break;
            }

            break;

        case DONT:
            SET_TELOPT(d, opt, RECV_DONT);

            /* FIXME: Mindless agreement. */
            if ( !HAS_TELOPT(d, opt, SENT_WILL) )
            {
                IAC_option(d, WONT, opt);
                break;
            }

            break;
    }
}


static inline void
x_subneg (struct descriptor_data *d)
{
    register char *sb = d->subneg_buf;
    register int i;
    int opt;
    char t;

    if ( d->subneg_len <= 0 )
        return;

    opt = *(sb++);
    d->subneg_len--;

    switch (opt)
    {
        case TELOPT_TTYPE:
            if ( d->subneg_len-- <= 0 || *(sb++) != TELQUAL_IS )
                break;

            if ( !strn_cmp(sb, d->ttype_recvd, d->subneg_len) )
            {
                d->ttype_recvd[0] = '\0';
                break;
            }

            strncpy(d->ttype_recvd, sb, d->subneg_len);
            d->ttype_recvd[d->subneg_len] = '\0';

            for ( i = 0; ttypes[i]; i++ )
                if ( !str_cmp(d->ttype_recvd, ttypes[i]) )
                    break;

            if ( ttypes[i] && i < d->ttype )
                d->ttype = i;

            t = TELQUAL_SEND;
            IAC_subneg(d, TELOPT_TTYPE, (const char *) &t, 1);
            break;
        case TELOPT_NAWS:
            if ( d->subneg_len < 4 )
                break;
            d->columns =
                (((unsigned char) sb[0]) * 256) + ((unsigned char) sb[1]);
            d->lines =
                (((unsigned char) sb[2]) * 256) + ((unsigned char) sb[3]);
            break;
        default:
            logmesg("Unrecognized subnegotiation type (%d)", opt);
            break;
    }

    d->subneg_len = 0;
}


/*
 * Utilities.
 *
 * These do things like subvert our output processing to send TELNET option
 * negotiation stuff through to the client without buffering, etc.  They're
 * most often wrapped by macros in "telnet.h", but occasionally used
 * directly by the code.  -satori
 */

void
IAC_option (struct descriptor_data *d, int wwdd, int opt)
{
    char str[3];

    switch (wwdd)
    {
        case WILL:
            if ( HAS_TELOPT(d, opt, SENT_WILL) )
                return;
            ZERO_TELOPT(d, opt);
            SET_TELOPT(d, opt, SENT_WILL);
            break;
        case WONT:
            if ( HAS_TELOPT(d, opt, SENT_WONT) )
                return;
            ZERO_TELOPT(d, opt);
            SET_TELOPT(d, opt, SENT_WONT);
            break;
        case DO:
            if ( HAS_TELOPT(d, opt, SENT_DO) )
                return;
            ZERO_TELOPT(d, opt);
            SET_TELOPT(d, opt, SENT_DO);
            break;
        case DONT:
            if ( HAS_TELOPT(d, opt, SENT_DONT) )
                return;
            ZERO_TELOPT(d, opt);
            SET_TELOPT(d, opt, SENT_DONT);
            break;
    }

    str[0] = (char) IAC;
    str[1] = (char) wwdd;
    str[2] = (char) opt;

    write_to_descriptor(d, str, 3, FALSE);
}


void
IAC_subneg (struct descriptor_data *d, int opt, const char *sb, int len)
{
    char str[MAX_SB_LEN + 5];
    char *ptr = str;
    int bytes;

    if ( sb && len > MAX_SB_LEN )
    {
        len = MAX_SB_LEN;
        logmesg("Truncated subnegotation string (IAC SB %.*s) to %db.",
                   MAX_SB_LEN, sb, MAX_SB_LEN);
    }

    bytes = 5 + len;

    *(ptr++) = (char) IAC;
    *(ptr++) = (char) SB;
    *(ptr++) = (char) opt;

    while ( sb && len-- > 0 )
    {
        if ( *sb == (char) IAC )  /* Double IACs. */
            *(ptr++) = *sb;
        *(ptr++) = *(sb++);
    }

    *(ptr++) = (char) IAC;
    *(ptr++) = (char) SE;
    write_to_descriptor(d, str, bytes, FALSE);
}
