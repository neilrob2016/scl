#include "globals.h"

/*** Read off stdin and write out to buffer ***/
void sendBuffer()
{
	u_char *s;
	u_char *s2;

	/* If length is zero then stdin has closed. This can happen if we're
	   piping or redirecting something into stdin on the command line.
	   If qseof option set then exit otherwise we carry on listening
	   for data from remote host. */
	if ((bufflen = read(STDIN,buff,BUFFSIZE)) < 1) 
	{
		if (flags.quit_on_stdin_eof) doExit(1);
		flags.select_stdin = 0;
		return;
	}
	buff[bufflen]='\0';

	for(s=buff;s < buff+bufflen;++s) 
	{
		if (flags.raw && *s == quit_char) doExit(1);

		/* Find and replace them with newline_str */
		if (*s == '\r' || *s == '\n')
		{
			if (nltype) *s = (u_char)newline[nltype];
			else
			{
				/* Shift data up */
				for(s2=s;*(s2+1);++s2) *s2 = *(s2+1);
				*s2 = '\0';
				--bufflen;
			}
		}
	}
	if (bufflen > 0) writeSock(buff,bufflen);
}




/*** Print out the buffer or call the telopt function ***/
void printBuffer()
{
	int i;

	logIO(buff,bufflen,"RX");

	if (flags.hexdump) hexdump(buff,bufflen,1);

	for(i=0;i < bufflen;) 
	{
		if (buff[i] == TELNET_IAC && bufflen > 2) 
			telopt(buff+i+1,&i);
		else 
		{
			if (!flags.hexdump) write(STDOUT,buff+i,1); 
			++i;
		}
	}
}




/*** Write the data out to the log file ***/
void logIO(u_char *data, int len, char *dir)
{
	time_t now;
	char tstr[30];

	if (!log_filename) return;

	/* Close and re-open file every given number of calls in case it has
	   been deleted or moved */
	if (!log_file_cnt)
	{
		if (log_file_fd != -1) close(log_file_fd);

		/* Try to open file */
		if ((log_file_fd = open(log_filename,log_open_flags,0666)) == -1)
		{
			fprintf(stderr,
				"WARNING: Can't open() '%s' to write: %s\n",
				log_filename,strerror(errno));

			/* Make sure we don't keep trying to re-open it */
			log_filename = NULL;
			log_file_fd = -1;
			return;
		}
	}

	if (flags.log_timestamp)
	{
		time(&now);
		strftime(tstr,sizeof(tstr),"%F %T, ",localtime(&now));
		write(log_file_fd,tstr,strlen(tstr));
		write(log_file_fd,dir,2);
		write(log_file_fd,": ",2);
	}

	/* Write data, inc counter */
	write(log_file_fd,data,len);
	log_file_cnt = (log_file_cnt + 1) % LOG_REOPEN_CNT;

	/* Always make sure there's a newline if timestamping or it will be
	   a mess */
	if (flags.log_timestamp && len && data[len-1] != '\n')
		write(log_file_fd,"\n",1);
}




/*** Print out the data we've received from the client ***/
void hexdump(u_char *buff, int len, int recvd)
{
	int start,i;

	printf("\n%s %d bytes\n",recvd ? "RECV" : "SEND",len);
	puts("---------------------------- BEGIN ----------------------------");
	puts("             0  1  2  3  4  5  6  7  8  9   0 1 2 3 4 5 6 7 8 9");
	for(start=0;start < len;start=i) 
	{
		printf("%04X %05d: ",start,start);

		/* Print hex part of line */
		for(i=start;i < start+10;++i) 
		{
			if (i < len) printf("%02X ",buff[i]);
			else printf("   ");
		}
		/* Print ascii part */
		printf(": ");
		for(i=start;i < start+10 && i < len;++i)
			printf("%c ",(buff[i] < 32 || buff[i] > 127) ? '.' : buff[i]);

		putchar('\n');
	}
	puts("----------------------------- END -----------------------------");
}
