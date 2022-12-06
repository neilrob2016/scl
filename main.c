/*****************************************************************************
  SCL: A simple client

  This is a simple TCP/UDP/SCTP client for IP4 and IP6 and it also supports
  unix domain sockets and SSL. It might not work properly when connecting to 
  some login shells as it does not support key processing negotiation. Default 
  protocol is TCP. 

  Original version written in 2003.
 *****************************************************************************/
#define MAINFILE
#include "globals.h"
#include "build_date.h"

void init();
void parseCmdLine(int argc, char **argv);
void afterConnect();
void mainloop();


int main(int argc,char **argv)
{
	parseCmdLine(argc,argv);
	init();
	echoOn();
	doConnect();
#ifdef SSLYR
	if (flags.ssl) sslConnect();
#endif
	afterConnect();
	if (flags.raw) rawOn();
	if (flags.force_linemode) canonicalOn();
	mainloop();
	return 0;
}




/*** Read stuff on command line ***/
void parseCmdLine(int argc, char **argv)
{
	int i,o;
	char *opt[] = 
	{
		/* 0 */
		"ip6",
		"tcp",
		"udp",
		"sctp",
		"uxs",

		/* 5 */
		"uxd",
		"ssl",
		"fe",
		"flm",
		"noenv",

		/* 10 */
		"user",
		"lp",
		"kasecs",
		"kastr",
		"log",

		/* 15 */
		"lr",
		"la",
		"lt",
		"qc",
		"noraw",

		/* 20 */
		"nltype",
		"hexdump",
		"teldump",
		"qseof",
		"mcast",

		/* 25 */
		"ver"
	};
	enum 
	{
		/* 0 */
		OPT_IP6,
		OPT_TCP,
		OPT_UDP,
		OPT_SCTP,
		OPT_UXS,

		/* 5 */
		OPT_UXD,
		OPT_SSL,
		OPT_FE,
		OPT_FLM,
		OPT_NOENV,

		/* 10 */
		OPT_USER,
		OPT_LP,
		OPT_KASECS,
		OPT_KASTR,
		OPT_LOG,

		/* 15 */
		OPT_LR,
		OPT_LA,
		OPT_LT,
		OPT_QC,
		OPT_NORAW,

		/* 20 */
		OPT_NLTYPE,
		OPT_HEXDUMP,
		OPT_TELDUMP,
		OPT_QSEOF,
		OPT_MCAST,

		/* 25 */
		OPT_VER,

		NUM_OPTS
	};

	if (argc < 2) goto USAGE; 

#ifdef IP6
	portstr = DEFAULT_PORTSTR;
#else
	port = DEFAULT_PORT;
#endif
	bzero(&flags,sizeof(flags));
	flags.raw = 1;
	flags.log_writes = 1;
	flags.select_stdin = 1;
	flags.send_telenv = 1;

	lport = 0;
	host = NULL;
	sockdomain = AF_INET;
	socktype = SOCK_STREAM;
	protocol = IPPROTO_TCP;
	unixpath[0] = '\0';
	kasecs = 0;
	kalen = 0;
	kastr = NULL;
	quit_char = CONTROL_RSQB;
	flags.quit_on_stdin_eof = 0;
	log_filename = NULL;
	log_open_flags = O_WRONLY | O_CREAT | O_TRUNC;
	nltype = NLTYPE;
	username = NULL;

	/* Go through arguments */
	for(i=1;i < argc;++i) 
	{
		if (argv[i][0] != '-') 
		{
			if (!host) host = argv[i];
			else 
			{
#ifdef IP6
				/* If port already set... */
				if (portstr != (char *)DEFAULT_PORTSTR)
				{
					fprintf(stderr,"ERROR: Unexpected argument '%s'.\n",argv[i]);
					exit(1);
				}	
				portstr = argv[i];
#else
				if (port != DEFAULT_PORT) 
				{
					fprintf(stderr,"ERROR: Unexpected argument '%s'.\n",argv[i]);
					exit(1);
				}	
				if ((port = atoi(argv[i])) < 1 || port > 65535) 
				{
					fputs("ERROR: Invalid port number.\n",stderr);
					exit(1);
				}
#endif
			}
			continue;
		}

		for(o=0;o < NUM_OPTS;++o) if (!strcmp(argv[i]+1,opt[o])) break;
		if (o == NUM_OPTS) goto USAGE;

		switch(o) 
		{
		case OPT_IP6:
#ifdef IP6
			sockdomain = AF_INET6;
			break;
#else
			fputs("ERROR: IP6 is not supported in this build\n",stderr);
			exit(1);
#endif

		case OPT_TCP:
			protocol = IPPROTO_TCP; 
			break;

		case OPT_UDP:
			socktype = SOCK_DGRAM;
			protocol = IPPROTO_UDP;
			break;

		case OPT_SCTP:
#ifdef SCTP
			protocol = IPPROTO_SCTP;
			break;
#else
			fputs("ERROR: SCTP is not supported in this build.\n",stderr);
			exit(1);
#endif

		case OPT_UXS:
		case OPT_UXD:
			if (++i == argc) goto USAGE;
			strcpy(unixpath,argv[i]);
			sockdomain = AF_UNIX;
			socktype = (o == OPT_UXS ? SOCK_STREAM : SOCK_DGRAM);
			protocol = 0;
			break;

		case OPT_SSL:
#ifdef SSLYR
			flags.ssl = 1;
			break;
#else
			fputs("ERROR: Secure Sockets Layer is not supported in this build.\n",stderr);
			exit(1);
#endif

		case OPT_FE:
			flags.force_echo = 1;
			break;

		case OPT_FLM:
			flags.force_linemode = 1;
			break;

		case OPT_NOENV:
			flags.send_telenv = 0;
			break;

		case OPT_USER:
			if (++i == argc) goto USAGE;
			username = strdup(argv[i]);
			break;

		case OPT_LP:
			if (++i == argc) goto USAGE;
			if ((lport = atoi(argv[i])) < 1 || lport > 0xFFFF) 
			{
				fputs("ERROR: Invalid local port number.\n",stderr);
				exit(1);
			}
			break;

		case OPT_KASECS:
			if (++i == argc) goto USAGE;
			if ((kasecs = atoi(argv[i])) < 1) 
			{
				fputs("ERROR: Invalid keepalive value.\n",stderr);
				exit(1);
			}
			break;

		case OPT_KASTR:
			if (++i == argc) goto USAGE;
			kastr = argv[i];
			if ((kalen = strlen(kastr)) < 1) 
			{
				fputs("ERROR: Invalid keepalive string.\n",stderr);
				exit(1);
			}
			break;

		case OPT_LOG:
			if (++i == argc) goto USAGE;
			log_filename = argv[i];
			break;

		case OPT_LR:
			flags.log_writes = 0;
			break;

		case OPT_LA:
			log_open_flags ^= O_TRUNC;
			log_open_flags |= O_APPEND;
			break;

		case OPT_LT:
			flags.log_timestamp = 1;
			break;

		case OPT_QC:
			if (++i == argc) goto USAGE;
			quit_char = atoi(argv[i]);
			if (!quit_char)
			{
				fputs("ERROR: Invalid quit character.\n",stderr);
				exit(1);
			}
			
		case OPT_NORAW:
			flags.raw = 0;
			break;

		case OPT_NLTYPE:
			if (++i == argc) goto USAGE;
			nltype = atoi(argv[i]);
			if (nltype < 0 || nltype >= NUM_NL_TYPES) 
			{
				fputs("ERROR: Invalid nltype.\n",stderr);
				exit(1);
			}
			break;

		case OPT_HEXDUMP:
			flags.hexdump = 1;
			break;

		case OPT_TELDUMP:
			flags.teldump = 1;
			break;
 
		case OPT_QSEOF:
			flags.quit_on_stdin_eof = 1;
			break;

		case OPT_MCAST:
			flags.join_multicast = 1;
			socktype = SOCK_DGRAM;
			protocol = IPPROTO_UDP;
			break;

		case OPT_VER:
			puts("\n*** Simple Client ***\n");
			printf("Version   : %s\n",VERSION);
			printf("Build date: %s\n\n",BUILD_DATE);
			exit(0);

		default:
			goto USAGE; 
		}
	}
	if (socktype == -1 ||
	    (!host && sockdomain != AF_UNIX) || 
	    (host && sockdomain == AF_UNIX)) goto USAGE;

	if (sockdomain == AF_UNIX && lport)
	{
		fputs("ERROR: Unix sockets do not support local ports.\n",stderr);
		exit(1);
	}

	if (protocol == SOCK_DGRAM && (flags.force_echo || flags.force_linemode)) 
	{
		fputs("ERROR: The UDP and force echo or force linemode options are mutually exclusive.\n",stderr);
		exit(1);
	}

	if (kastr && !kasecs) kasecs = KASECS;
	return;

	USAGE:
	printf("Usage: %s\n"
	       "       <host> | -tcp/-udp/-sctp <host> | -uxs/-uxd <path>\n"
	       "                         : TCP, UDP, SCTP, UNIX domain stream/datagram socket.\n"
	       "                           Default is TCP and host must be specified\n"
	       "       [<port>]          : Remote port. Not for unix sockets. Default is %d.\n"
	       "       [-ip6]            : Use IPv6.\n"
	       "       [-ssl]            : Use the Secure Sockets Layer.\n"
	       "       [-fe]             : Force echoing.\n"
	       "       [-flm]            : Force line mode.\n"
	       "       [-noenv]          : Don't send enviroment information terminal type,\n"
	       "                           username, X display and window size to a telnet\n"
	       "                           server.\n"
	       "       [-user <name>]    : Send this username instead of actual username when\n"
	       "                           logging into a telnet server that requests\n"
	       "                           enviroment variables.\n"
	       "       [-lp <port>]      : Local port. Not for unix sockets.\n"
	       "       [-kasecs <secs>]  : Send keepalive string every <secs> seconds after\n"
	       "                           you last sent data. Useful for some MUDs and chat\n"
	       "                           systems. Default = %d secs if kastr set.\n" 
	       "       [-kastr <string>] : Sets keepalive string. Default = '\\n'.\n"
	       "       [-log <filename>] : Log I/O to this file (doesn't affect stdout)\n"
	       "       [-lr]             : Only write received data into the log file\n"
	       "       [-la]             : Append to the log file if it already exists\n"
	       "       [-lt]             : Prepend each line of the log with a timestamp plus\n"
	       "                           TX or RX for client transmitting or receiving\n"
	       "       [-qc <1 - 255>]   : Sets quit character ascii code. Default = %d\n"
	       "                           (Control-])\n"
	       "       [-noraw]          : Don't put terminal in raw mode (^C etc will kill\n"
	       "                           the client instead of being sent to the server).\n"
	       "       [-nltype <0-2>]   : Newline type to send. 0 = '', 1 = '\\n',\n"
	       "                           2 = '\\r'. Default = %d.\n"
	       "       [-hexdump]        : Hexdump all data received.\n"
	       "       [-teldump]        : Dump telnet opcodes sent and received.\n"
	       "       [-qseof]          : Quit if we receive EOF on stdin. This is useful\n"
	       "                           when piping or redirecting files into the program.\n"
	       "       [-mcast]          : Join IP4 UDP multicast group defined by <host>.\n"
	       "                           Sets UDP mode automatically so -udp not required.\n"
	       "       [-ver]            : Print version and build date then exit.\n",
		argv[0],DEFAULT_PORT,KASECS,CONTROL_RSQB,NLTYPE);
	exit(1);
}




/*** Get started ***/
void init()
{
	struct passwd *pwd;

	log_file_cnt = 0;
	log_file_fd = -1;
	flags.using_telopt = 0;
	flags.connected = 0;

	xdisplay = getenv("DISPLAY");
	if (xdisplay && !strlen(xdisplay)) xdisplay = NULL;

	if (!username)
	{
		pwd = getpwuid(getuid());
		username = pwd ? strdup(pwd->pw_name) : NULL;
	}

	/* Store current keyboard settings */
	if (isatty(STDIN)) tcgetattr(STDIN,&term_settings);
}




/*** Print escape message and set up signals ***/
void afterConnect()
{
	if (flags.raw)
	{
		if (quit_char < 32)
			printf("Quit character is '^%c' (plus newline if in line mode).\n",quit_char + 'A' - 1);
		else
			printf("Quit character is '%c' (plus newline if in line mode).\n",quit_char);
	}
	/* Signals */
	signal(SIGINT,doExit);
	signal(SIGQUIT,doExit);
	signal(SIGTERM,doExit);
#ifdef TIOCGWINSZ
	signal(SIGWINCH,sendTermSize);
#endif
}




/*** Do the business ***/
void mainloop()
{
	struct timeval tv1;
	struct timeval tv2;
	struct timeval tvs;
	fd_set mask;

	/* Set up keepalive stuff if required */
	if (kasecs) 
	{
		tvs.tv_sec = kasecs;
		tvs.tv_usec = 0;
		gettimeofday(&tv1,NULL);
		tv1.tv_sec += kasecs;
	}

	/** Main loop **/
	while(1) 
	{
		/* select on stdin (bit 0) and the socket */
		FD_ZERO(&mask);

		if (flags.select_stdin) FD_SET(STDIN,&mask);
		FD_SET(sock,&mask);

		switch(select(FD_SETSIZE,&mask,0,0,kasecs ? &tvs : NULL)) 
		{
		case -1: continue;

		case 0:
			if (kasecs) 
			{
				tvs.tv_sec = kasecs;
				tvs.tv_usec = 0;
				gettimeofday(&tv1,NULL);
				tv1.tv_sec += kasecs;
				if (kastr) writeSock((u_char *)kastr,kalen);
			}
			if (nltype) writeSock((u_char *)&newline[nltype],1);
			if (flags.echo && kasecs)
			{
				if (kastr)
					write(STDOUT,kastr,strlen(kastr));
				if (nltype) write(STDOUT,&newline[nltype],1);
			}
			continue;
		}

		/* See if data on stdin. */
		if (FD_ISSET(STDIN,&mask)) 
		{
			sendBuffer();

			/* Reset keepalive */
			if (kasecs) 
			{
				gettimeofday(&tv1,NULL);
				tv1.tv_sec += kasecs;
			}
		}
		else if (kasecs) 
		{
			/* If doing keepalive see how many seconds would 
			   be left */
			gettimeofday(&tv2,NULL);
			tvs.tv_sec = tv1.tv_sec - tv2.tv_sec;
			if (tvs.tv_sec < 0) tvs.tv_sec = 0;
			tvs.tv_usec = 0;
		}

		/* See if data on socket */
		if (FD_ISSET(sock,&mask)) readSock();
	}
}




/*** Set echoing back on and quit ***/
void doExit(int sig)
{
	/* Reset terminal to start state and exit */
	if (isatty(STDIN)) tcsetattr(0,TCSANOW,&term_settings);

	if (flags.connected)
	{
		printf("\n*** Connection closed by %s ***\n",
			sig ? "client" : "server");
	}
	if (log_file_fd != -1) close(log_file_fd);
	exit(0);
}
