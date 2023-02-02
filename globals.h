#ifdef __linux__
#define _GNU_SOURCE  /* For asprintf() */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <netdb.h>
#include <time.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <uuid/uuid.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#ifdef SSLYR
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#define VERSION "20230131"

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

#define STDIN  0
#define STDOUT 1

#define BUFFSIZE         17000  /* Enough for entire SSL read block */
#define OPTOUT_SIZE      1000
#define DEFAULT_PORTSTR "23"
#define DEFAULT_PORT     23
#define UNRESOLVED       (char *)"<unresolved>"
#define CONTROL_RSQB     29 /* Right square bracket */
#define KASECS           60
#define LOG_REOPEN_CNT   10
#define NUM_NL_TYPES     3
#define NLTYPE           1

/* SE, IP etc defined in arpa/telnet.h */
#define TELNET_SE   SE
#define TELNET_IP   IP
#define TELNET_AO   AO
#define TELNET_AYT  AYT
#define TELNET_EC   EC
#define TELNET_EL   EL
#define TELNET_GA   GA
#define TELNET_SB   SB
#define TELNET_WILL WILL
#define TELNET_WONT WONT
#define TELNET_DO   DO
#define TELNET_DONT DONT
#define TELNET_IAC  IAC

#define PRINT_SUBSEND(O,S) \
	if (flags.teldump) \
	{ \
		printf("TELOPT SEND: %u (%s) subopt %u (%s)\n", \
			O,telopt_str[O - TELNET_SE],S,suboptName(S)); \
	}

#define NUM_TELOPTS 16

#ifdef MAINFILE
#define EXTERN
#else
#define EXTERN extern
#endif

#ifdef MAINFILE
char *telopt_str[NUM_TELOPTS] =
{
	"SE",
	"NOP",
	"MARK",
	"BRK",
	"IP",
	"AO",
	"AYT",
	"EC",
	"EL",
	"GA",
	"SB",
	"WILL",
	"WONT",
	"DO",
	"DONT",
	"IAC"
};

char newline[NUM_NL_TYPES] = "\0\n\r";
#else
extern char *telopt_str[NUM_TELOPTS];
extern char newline[NUM_NL_TYPES];
#endif

struct st_flags
{
	/* Cmd line flags */
	unsigned ssl               : 1;
	unsigned force_echo        : 1;
	unsigned force_linemode    : 1;
	unsigned log_writes        : 1;
	unsigned log_timestamp     : 1;
	unsigned raw               : 1;
	unsigned hexdump           : 1;
	unsigned teldump           : 1;
	unsigned quit_on_stdin_eof : 1;
	unsigned join_multicast    : 1;
	unsigned send_telenv       : 1;

	/* Runtime flags */
	unsigned using_telopt      : 1;
	unsigned select_stdin      : 1;
	unsigned echo              : 1;
	unsigned connected         : 1;
};

/* main.c */
void doExit(int sig);

/* network.c */
void doConnect();
void joinMulticastGroup();
void readSock();
void writeSock(u_char *data, int len);

/* io.c */
void sendBuffer();
void printBuffer();
void logIO(u_char *data, int len, char *dir);
void hexdump(u_char *buff, int len, int recvd);

/* ssl.c */
void sslConnect();
void sslError(char *func);

/* telopt.c */
void telopt(u_char *s,int *i);
void teloptWill(u_char subopt);
void teloptWont(u_char subopt);
void teloptDo(u_char subopt);
void teloptDont(u_char subopt);
void teloptSubneg(u_char *s,int *i);
void sendNaws();
void sendTerminal();
void sendDisplay();
void sendTermSize(int sig);
void sendEnviroment();
void sendOptReply(u_char opt,u_char subopt);
char *suboptName(int subopt);

/* term.c */
void rawOn();
void echoOn();
void echoOff();
void canonicalOn();
void canonicalOff();

/* Global defs */
EXTERN struct sockaddr_in con_addr;
EXTERN struct termios term_settings;
EXTERN struct st_flags flags;
EXTERN u_char quit_char;
EXTERN char unixpath[110];
EXTERN char *host;
EXTERN char *kastr;
EXTERN char *xdisplay;
EXTERN char *username;
EXTERN char *log_filename;
EXTERN int sock;
EXTERN int bufflen;
EXTERN int sockdomain;
EXTERN int socktype;
EXTERN int protocol;
EXTERN int lport;
EXTERN int kasecs;
EXTERN int kalen;
EXTERN int nltype;
EXTERN int log_file_cnt;
EXTERN int log_file_fd;
EXTERN int log_open_flags;
EXTERN u_char optout[OPTOUT_SIZE];
EXTERN u_char buff[BUFFSIZE*2+1];

#ifdef IP6
EXTERN char *portstr;
#else
EXTERN int port;
#endif

#ifdef SSLYR
EXTERN SSL *ssl_handle;
EXTERN SSL_CTX *ssl_context;
#endif
