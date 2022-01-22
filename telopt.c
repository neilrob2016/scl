#include "globals.h"


/*** Main telopt function. If we don't support at least the recognition
     and negative replies to telopt requests then we can't connect to 
     a unix telnetd since it'll just sit and wait for replies forever. ***/
void telopt(u_char *s,int *i)
{
	u_char subopt = *(s+1);
	int opt;

	if (flags.teldump) 
	{
		opt = (int)*s - TELNET_SE;
		printf("TELOPT RECV: %u (%s) subopt %u (%s)\n",
			*s,
			opt < 0 || opt >= NUM_TELOPTS ? "?" : telopt_str[*s - TELNET_SE],
			subopt,suboptName(subopt));
	}

	flags.using_telopt = 1;

	switch(*s) 
	{
	case TELNET_SB:   teloptSubneg(s+1,i);  return;

	case TELNET_WILL: teloptWill(subopt);  break;
	case TELNET_WONT: teloptWont(subopt);  break;
	case TELNET_DO  : teloptDo(subopt);    break;
	case TELNET_DONT: teloptDont(subopt);  break;

	case TELNET_IAC: 
		/* Print char 255 */
		putchar(TELNET_IAC); 
		(*i) += 2;
		return;

	default:
		/* Ignore any unknown/unused codes */
		(*i) += 2;
		return;
	}
	(*i) += 3;
}




/*** WILL reply ***/
void teloptWill(u_char subopt)
{
	switch(subopt) 
	{
	case TELOPT_ECHO:
		if (flags.force_echo) 
		{
			sendOptReply(TELNET_DONT,subopt);
			return;
		}
		/* Switch echoing to screen off */
		echoOff();
		break;

	case TELOPT_SGA:
		if (flags.force_linemode) 
		{
			sendOptReply(TELNET_DONT,subopt);
			return;
		}
		/* Switch canonical mode off */
		canonicalOff();
		break;

	default:
		sendOptReply(TELNET_DONT,subopt);
		return;
	}
	sendOptReply(TELNET_DO,subopt);
}




/*** WONT reply ****/
void teloptWont(u_char subopt)
{
	switch(subopt) 
	{
	case TELOPT_ECHO:
		if (!flags.force_echo) echoOn();
		break;

	case TELOPT_SGA:
		if (!flags.force_linemode) canonicalOn();
	}
	sendOptReply(TELNET_DONT,subopt);
}




/*** DO reply ***/
void teloptDo(u_char subopt)
{
	switch(subopt) 
	{
	case TELOPT_TTYPE:
		/* Request to send terminal type comes in a sub option command 
		   from the server, here we just agree to do it */
		sendOptReply(TELNET_WILL,TELOPT_TTYPE);
		break;

	case TELOPT_XDISPLOC:
		/* As above only acknowledge (or not if X display not set) */
		sendOptReply(xdisplay ? TELNET_WILL : TELNET_WONT,TELOPT_XDISPLOC);
		break;

	case TELOPT_NAWS:
		/* Send window info immediately */
		sendNaws();
		break;

	case TELOPT_NEW_ENVIRON:
		sendEnviroment();
		break;

	default:	
		sendOptReply(TELNET_WONT,subopt);
	}
}




/*** Server says DONT so we reply WONT ***/
void teloptDont(u_char subopt)
{
	sendOptReply(TELNET_WONT,subopt);
}




/*** Print out any sub negotiation received from server unless its a
     request for terminal type ***/
void teloptSubneg(u_char *s, int *i)
{
	int cnt;
	int pos;
	u_char type;

	for(cnt=0,pos=0;*s != TELNET_SE && cnt < bufflen;++s,++cnt) 
	{
		switch(cnt) 
		{
		case 0:
			type = *s;
			switch(type)
			{
			case TELOPT_TTYPE:
			case TELOPT_XDISPLOC:
				++pos;
			}
			break;

		case 1:
			if (*s == TELQUAL_SEND) ++pos;
			break;

		case 2:
			if (*s == TELNET_IAC) ++pos;
		}
	}
	(*i) += (3 + cnt);
	if (pos != 3) return;

	/* If we got a terminal request then send it */
	switch(type)
	{
	case TELOPT_TTYPE:
		sendTerminal();
		break;

	case TELOPT_XDISPLOC:
		sendDisplay();
		break;
	}
}




/*** Send window size info ***/
void sendNaws()
{
	/* Send will reply them send window size info immediately after */
	sendOptReply(TELNET_WILL,TELOPT_NAWS);
	sendTermSize(0);
}




/*** Send terminal info. Just set to standard vt100 if env var not set ***/
void sendTerminal()
{
	char *term;

	if (!(term = getenv("TERM")) || !strlen(term)) term="vt100";

	PRINT_SUBSEND(TELNET_SB,TELOPT_TTYPE);

	sprintf((char *)optout,"%c%c%c%c%s%c%c",
		TELNET_IAC,
		TELNET_SB,TELOPT_TTYPE,TELQUAL_IS,term,TELNET_IAC,TELNET_SE);
	writeSock(optout,6+strlen(term));
}




/*** Send the X windows display ***/
void sendDisplay()
{
	/* Should never be request if null but just in case */
	if (!xdisplay) return;

	PRINT_SUBSEND(TELNET_SB,TELOPT_XDISPLOC);

	sprintf((char *)optout,"%c%c%c%c%s%c%c",
		TELNET_IAC,
		TELNET_SB,
		TELOPT_XDISPLOC,TELQUAL_IS,xdisplay,TELNET_IAC,TELNET_SE);
	writeSock(optout,6+strlen(xdisplay));
}




/*** Get and send the current terminal size ***/
void sendTermSize(int sig)
{
#ifdef TIOCGWINSZ
	struct winsize ws;
#endif
	uint16_t width;
	uint16_t height;
	u_char w1,w2;
	u_char h1,h2;
	u_char *p;

	/* This is the only asychronous telopt function so do this check */
	if (!flags.using_telopt) return;

	width = 80;
	height = 25;

#ifdef TIOCGWINSZ
	if (ioctl(1,TIOCGWINSZ,&ws)==-1) perror("ERROR: ioctl()");
	else 
	{
		width = ws.ws_col;  
		height = ws.ws_row;
	}
	signal(SIGWINCH,sendTermSize);
#endif
	PRINT_SUBSEND(TELNET_SB,TELOPT_NAWS);

	sprintf((char *)optout,"%c%c%c",TELNET_IAC,TELNET_SB,TELOPT_NAWS);

	/* Still need to double up 255s bizzarely given this is essentially
	   a fixed size packet. Hey ho... */
	w1 = (width & 0xFF00) >> 8;
	w2 =  width & 0x00FF;
	h1 = (height & 0xFF00) >> 8;
	h2 =  height & 0x00FF;

	p = optout + 3;
	*p++ = w1;
	if (w1 == 255) *p++ = 255;
	*p++ = w2;
	if (w2 == 255) *p++ = 255;
	*p++ = h1;
	if (h1 == 255) *p++ = 255;
	*p++ = h2;
	if (h2 == 255) *p++ = 255;
	*p++ = TELNET_IAC;
	*p++ = TELNET_SE;

	writeSock(optout,(int)(p - optout));
}




void sendEnviroment()
{
	char *text;
	int slen;
	int len;

	/* Username should never be NULL but you never know */
	if (!flags.send_enviroment || (!xdisplay && !username)) goto ERROR;

	sendOptReply(TELNET_WILL,TELOPT_NEW_ENVIRON);
	PRINT_SUBSEND(TELNET_SB,TELOPT_NEW_ENVIRON);

	sprintf((char *)optout,"%c%c%c%c",
		TELNET_IAC,TELNET_SB,TELOPT_NEW_ENVIRON,TELQUAL_IS);
	len = 4;
	if (xdisplay)
	{
		slen = asprintf(&text,"%cDISPLAY%c%s",ENV_USERVAR,NEW_ENV_VALUE,xdisplay);
		if (slen == -1 || len + slen >= OPTOUT_SIZE) goto ERROR;
		memcpy(optout+len,text,slen);
		len += slen;
	}
	if (username)
	{
		slen = asprintf(&text,"%cUSER%c%s",ENV_USERVAR,NEW_ENV_VALUE,username);
		if (slen == -1 || len + slen >= OPTOUT_SIZE) goto ERROR;
		memcpy(optout+len,text,slen);
		len += slen;
	}
	free(text);
	optout[len] = TELNET_IAC;
	optout[++len] = TELNET_SE;
	writeSock(optout,len+1);
	return;

	ERROR:
	sendOptReply(TELNET_WONT,TELOPT_NEW_ENVIRON);
}




/*** Send will/wont/do/dont type reply ***/
void sendOptReply(u_char opt,u_char subopt)
{
	PRINT_SUBSEND(opt,subopt);
	sprintf((char *)optout,"%c%c%c",TELNET_IAC,opt,subopt);
	writeSock(optout,3);
}




/*** Return name of the sub option ***/
char *suboptName(int subopt)
{
	switch(subopt)
	{
	case TELOPT_ECHO:
		return "ECHO";

	case TELOPT_SGA:
		return "SGA";

	case TELOPT_TTYPE:
		return "TTYPE";

	case TELOPT_NAWS:
		return "NAWS";

	case TELOPT_XDISPLOC:
		return "XDISPLOC";

	/* A few others not used here but some of which seem to get sent by 
	   the unix telnetd */
	case TELOPT_LINEMODE:
		return "LINEMODE";

	case TELOPT_OLD_ENVIRON:
		return "OLD_ENVIRON";

	case TELOPT_AUTHENTICATION:
		return "AUTH";

	case TELOPT_ENCRYPT:
		return "ENCRYPT";

	case TELOPT_NEW_ENVIRON:
		return "NEW_ENVIRON";
	}
	return "<unknown>";
}
