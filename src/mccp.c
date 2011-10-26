/* OOPS: Where's the original header for this? */

/**
 * Module:      mccp.c
 * Owner:       Satori
 * Checked-in:  04 Oct, 2000
 *
 * Mud Client Compression Protocol support.  These are really little more
 * than Zlib interfacing routines.
 */

#include    "ground0.h"
#include    "telnet.h"
#include    "mccp.h"


static inline void *
zlib_alloc (void *opaque, unsigned int nmemb, unsigned int size)
{
    return calloc(nmemb, size);
}


static inline void
zlib_free (void *opaque, void *address)
{
    free(address);
}


bool
begin_compression (struct descriptor_data *d)
{
    z_stream *zcs;

    /* Compression already on. */
    if ( d->outcom_stream )
        return TRUE;

    /* Allocate the stream and buffer. */
    zcs = alloc_mem(sizeof(z_stream));
    d->outcom = alloc_mem(MAX_COMPRESS_LEN);

    /* Fill in stream information. */
    zcs->next_in = NULL;
    zcs->avail_in = 0;
    zcs->next_out = d->outcom;
    zcs->avail_out = MAX_COMPRESS_LEN;
    zcs->zalloc = zlib_alloc;
    zcs->zfree = zlib_free;
    zcs->opaque = NULL;

    /* Setup zlib deflation algorithm for the stream. */
    if ( deflateInit(zcs, Z_BEST_COMPRESSION) != Z_OK )
    {
        free_mem(zcs, sizeof(z_stream));
        free_mem(d->outcom, MAX_COMPRESS_LEN);
        return FALSE;
    }

    IAC_subneg(d, TELOPT_COMPRESS2, NULL, 0);
    d->outcom_stream = zcs;
    return TRUE;
}


bool
end_compression (struct descriptor_data * d)
{
    unsigned char dummy[1];

    /* Compression already off. */
    if ( d->outcom_stream == NULL )
        return TRUE;

    /* Set it up so that we have no further input to deflate(). */
    d->outcom_stream->next_in = dummy;
    d->outcom_stream->avail_in = 0;

    /* Complete deflation and send extant compressed output. */
    if ( deflate(d->outcom_stream, Z_FINISH) != Z_STREAM_END )
        return FALSE;
    if ( !process_compressed_output(d) )
        return FALSE;

    /* Deinitialize deflation on the stream. */
    deflateEnd(d->outcom_stream);

    /* Free memory, set pointers to NULL to indicate they're dead. */
    free_mem(d->outcom_stream, sizeof(z_stream));
    free_mem(d->outcom, MAX_COMPRESS_LEN);
    d->outcom_stream = NULL;
    d->outcom = NULL;

    return TRUE;
}


bool
process_compressed_output (struct descriptor_data * d)
{
    register unsigned char *writept;
    int length;
    int bytes;
    int total;

    /* Compression is off -- how'd we get here? */
    if ( d->outcom_stream == NULL )
        return FALSE;

    /* Setup some useful shit. */
    length = d->outcom_stream->next_out - d->outcom;
    writept = d->outcom;
    total = 0;

    /* While we haven't written it all, write() some. */
    while ( total < length )
    {
        bytes = write(d->descriptor, writept, length - total);

        /* We may not to be able to write it all, break out if so. */
        if ( bytes == -1 )
        {
            if ( errno == EAGAIN || errno == EINTR )
                break;
            return FALSE;
        }
        else if ( bytes == 0 )
            break;

        /* Update for next pass, if necessary. */
        total += bytes;
        writept += bytes;
    }

    /* We wrote something, update outcom buffer and next_out. */
    if ( writept > d->outcom )
    {
        memmove(d->outcom, writept, length - total);
        d->outcom_stream->next_out = d->outcom + (length - total);
    }

    return TRUE;
}


bool
write_compressed (struct descriptor_data * d, const char *txt, int length)
{
    /* Setup the stream for deflate() to work on it. */
    d->outcom_stream->next_in = (unsigned char *) txt;
    d->outcom_stream->avail_in = length;

    /* While input for deflate() is still available. */
    while ( d->outcom_stream->avail_in )
    {
        d->outcom_stream->avail_out =
            MAX_COMPRESS_LEN - (d->outcom_stream->next_out - d->outcom);

        if (d->outcom_stream->avail_out &&
            deflate(d->outcom_stream, Z_SYNC_FLUSH) != Z_OK)
            return FALSE;
    }

    /* Write some. */
    if ( !process_compressed_output(d) )
        return FALSE;

    return TRUE;
}


void
do_compress (struct char_data *ch, char *argument)
{
    if ( !ch->desc )
        return;

    char arg[MAX_INPUT_LENGTH];

    one_argument(argument, arg);

    if ( ch->desc->outcom_stream == NULL )
    {
        if ( !g_forceAction && str_cmp(arg, "force") )
        {
            ZERO_TELOPT(ch->desc, TELOPT_COMPRESS2);
            IAC_option(ch->desc, WILL, TELOPT_COMPRESS2);
            send_to_char("Attempting to enable compression.\r\n"
                         "Use &WMCCP!&n to force it on if necessary.\r\n",
                         ch);
            return;
        }
        else if ( !begin_compression(ch->desc) )
        {
            send_to_char("Failed to enable compression.\r\n", ch);
            return;
        }

        send_to_char("Compression enabled.\r\n", ch);
        return;
    }

    if ( !end_compression(ch->desc) )
    {
        send_to_char("Failed to disable compression.\r\n", ch);
        return;
    }

    send_to_char("Compression disabled.\r\n", ch);
}
