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
 * Module:      ban.h
 * Synopsis:    Ban definitions and prototypes.
 * Author:      Satori
 * Created:     29 Mar 2002
 *
 * NOTES:
 */

#ifndef GZ2_BAN_H_
#define GZ2_BAN_H_

const char *GetNewbieBan(const char *);
const char *GetFullBan(const char *);
void LoadBans(FILE *);

#endif
