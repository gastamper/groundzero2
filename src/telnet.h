
#define TELOPT_COMPRESS2      86

#define SENT_DO   (1 << 0)
#define SENT_WILL (1 << 1)
#define RECV_WONT (1 << 2)
#define RECV_DONT (1 << 3)
#define SENT_WONT (1 << 4)
#define SENT_DONT (1 << 5)
#define RECV_WILL (1 << 6)
#define RECV_DO   (1 << 7)

#define T_O(_i, _o)             (_i)->telopt[(int) (_o)]
#define HAS_TELOPT(_i, _o, _b)  IS_SET(T_O((_i), (_o)), (_b))
#define SET_TELOPT(_i, _o, _b)  SET_BIT(T_O((_i), (_o)), (_b))
#define CLR_TELOPT(_i, _o, _b)  REMOVE_BIT(T_O((_i), (_o)), (_b))
#define ZERO_TELOPT(_i, _o)     T_O((_i), (_o)) = 0

#define ECHO_OFF(_)     IAC_option((_), WILL, TELOPT_ECHO)
#define ECHO_ON(_)      IAC_option((_), WONT, TELOPT_ECHO)
#define TTYPE_DO(_)     IAC_option((_), DO, TELOPT_TTYPE)
#define NAWS_DO(_)      IAC_option((_), DO, TELOPT_NAWS)
#define MCP2_DO(_)      IAC_option((_), DO, TELOPT_COMPRESS2)
#define MCP1_DO(_)      IAC_option((_), DO, TELOPT_COMPRESS)

enum
{
    TELST_NOMINAL,
    TELST_IAC,
    TELST_WILL,
    TELST_WONT,
    TELST_DO,
    TELST_DONT,
    TELST_SB,
    TELST_SB_IAC
};

void process_telnet(struct descriptor_data *, const unsigned char *, int);
void IAC_option(struct descriptor_data *, int, int);
void IAC_subneg(struct descriptor_data *, int, const char *, int);
