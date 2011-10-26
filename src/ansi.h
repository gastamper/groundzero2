#ifndef _ANSI_
#define _ANSI_
/* ANSI control sequences, to be include */
#define ESCAPE      ""
#define ANSI_NORMAL "[0m"
#define ANSI_BOLD   "[1m"
#define ANSI_UNDERL "[4m"
#define ANSI_BLINK  "[5m"
#define ANSI_INVERS "[7m"

#define ANSI_BLACK  "[30m"
#define ANSI_RED    "[31m"
#define ANSI_GREEN  "[32m"
#define ANSI_YELLOW "[33m"
#define ANSI_BLUE   "[34m"
#define ANSI_PURPLE "[35m"
#define ANSI_CYAN   "[36m"
#define ANSI_WHITE  "[37m"
#define ANSI_BOLD_RED    "[1;31m"
#define ANSI_BOLD_GREEN  "[1;32m"
#define ANSI_BOLD_YELLOW "[1;33m"
#define ANSI_BOLD_BLUE   "[1;34m"
#define ANSI_BOLD_PURPLE "[1;35m"
#define ANSI_BOLD_CYAN   "[1;36m"
#define ANSI_BOLD_WHITE  "[1;37m"

#define ANSI_CLS    "[2J"
#define ANSI_HOME   "[1;1H"
#endif

char *color_table[] = {
    "[30m",                    /* 0x0 */
    "[31m",
    "[32m",
    "[33m",
    "[34m",
    "[35m",
    "[36m",                    /*0x6 */
    "[37m",                    /*0x7 white */
    "[1;30m",
    "[1;31m",
    "[1;32m",
    "[1;33m",
    "[1;34m",                  /*0xc */
    "[1;35m",
    "[1;36m",
    "[1;37m",                  /*0xf */
    "[0;37m",                  /*0x6 white */
    "`",
    0
};
