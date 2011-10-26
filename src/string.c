/*
 * string.c : Miscellaneous replacement string functions.
 */

#include    "ground0.h"


int ColorTranslate(char);


int
str_len (const char *str)
{
    if ( !str )
        return 0;

    int cnt = 0;

    while ( *str )
    {
        if ( *str == TCON_TOK && *(str + 1) )
        {
            switch (ColorTranslate(*(++str)))
            {
                case -1:
                case 257:
                    cnt += 2;
                    break;
                case 256:
                case 261:
                    cnt++;
                    break;
                case 260:
                    str++;      // Skip _ in &u_.
                    break;
                default:
                    break;
            }
        }
        else
            cnt++;
        str++;
    }

    return cnt;
}


void
strn_cpy (char *dest, const char *src, int maxVisible)
{
    while ( maxVisible > 0 && *src )
    {
        if ( *src == TCON_TOK && *(src + 1) )
        {
            switch (ColorTranslate(*(++src)))
            {
                case -1:
                    if ( maxVisible > 2 )
                    {
                        *(dest++) = TCON_TOK;
                        *(dest++) = *src;
                        maxVisible -= 2;
                    }
                    break;
                case 257:
                    if ( maxVisible > 2 )
                    {
                        *(dest++) = TCON_TOK;
                        *(dest++) = *src;
                        maxVisible -= 2;
                    }
                    break;
                case 256:
                    if ( maxVisible > 2 )
                    {
                        *(dest++) = TCON_TOK;
                        *(dest++) = TCON_TOK;
                        maxVisible -= 2;
                    }
                    break;
                case 260:
                    if ( *(src + 1) )
                    {
                        *(dest++) = TCON_TOK;
                        *(dest++) = *(src++);
                        *(dest++) = *src;
                    }
                    break;
                case 261:
                    *(dest++) = TCON_TOK;
                    *(dest++) = *src;
                    maxVisible--;
                    break;
                default:
                    *(dest++) = TCON_TOK;
                    *(dest++) = *src;
                    break;
            }
        }
        else
        {
            *(dest++) = *src;
            maxVisible--;
        }

        src++;
    }

    *dest = '\0';
}



char *
without_colors (const char *str)
{
    static char buf[MAX_STRING_LENGTH + 1];

    ProcessColors(NULL, TRUE, str, strlen(str), buf,
                  MAX_STRING_LENGTH + 1);
    return buf;
}


/* Is 'a' a suffix of 'b'? */
int
str_suffix (const char *a, const char *b)
{
    size_t alen = strlen(a);
    size_t blen = strlen(b);

    if ( alen > blen )
        return -1;

    return str_cmp(a, b + (blen - alen));
}


void
smash_tilde (char *str)
{
    for ( ; *str != '\0'; str++ )
        if ( *str == '~' )
            *str = '-';
}


int
str_cmp (const char *a, const char *b)
{
    int diff;
    int i;

    if ( !a || !b )
    {
        logmesg("a (0x%p) or b (0x%p): null", a, b);
        return -1;
    }

    for ( i = 0; a[i] || b[i]; i++ )
        if ( (diff = LOWER(a[i]) - LOWER(b[i])) != 0 )
            return diff;

    return 0;
}


int
strn_cmp (const char *a, const char *b, size_t n)
{
    int diff;
    int i;

    if ( !a || !b )
    {
        logmesg("a (0x%p) or b (0x%p): null", a, b);
        return -1;
    }

    for ( i = 0; (a[i] || b[i]) && i < n; i++ )
        if ( (diff = LOWER(a[i]) - LOWER(b[i])) != 0 )
            return diff;

    return 0;
}


/*
 * Returns an initial-capped string.
 */
char *
capitalize (const char *str)
{
    static char strcap[MAX_STRING_LENGTH];
    int i;

    for ( i = 0; str[i] != '\0'; i++ )
        strcap[i] = LOWER(str[i]);
    strcap[i] = '\0';
    strcap[0] = UPPER(strcap[0]);
    return strcap;
}
