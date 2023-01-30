#include "globals.h"


/*** Set up socket ***/
void doConnect()
{
#ifdef IP6
	struct sockaddr_in6 bind_addr6;
	struct addrinfo hints;
	struct addrinfo *res;
	char ipnum[50];
#else
	struct hostent *hp;
	char **ptr;
#endif
	struct sockaddr_in bind_addr;
	struct sockaddr_un con_path;
	struct sockaddr *bind_ptr;
	char *proto;
	char *name;
	size_t bind_size;
	int ret;
	int on = 1;

	/* Set up the socket */
	if ((sock=socket(sockdomain,socktype,protocol)) == -1) 
	{
		perror("ERROR: socket()");
		doExit(1);
	}

	/* Check for unix domain */
	if (sockdomain == AF_UNIX) 
	{
		bzero(&con_path,sizeof(con_path));
		con_path.sun_family = AF_UNIX;
		strcpy(con_path.sun_path,unixpath);

		printf("Trying %s ...\n",unixpath);

		if (connect(sock,(struct sockaddr *)&con_path,sizeof(con_path)) == -1) 
		{
			perror("ERROR: connect()");
			doExit(1);
		}
		goto CONNECTED;
	}

	switch(protocol)
	{
	case IPPROTO_UDP:
		/* Do this or we can't send to a broadcast address. Don't 
		   check return value since if it doesn't work then screw it */
		if (setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&on,sizeof(on)) == -1)
			perror("WARNING: setsockopt(SO_BROADCAST)");
		break;

	case IPPROTO_TCP:
		/* Disable Nagle algorithm so outgoing data isn't buffered. 
		   Something similar probably needs to be done for SCTP but,
		   well, meh. */
		if (setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,&on,sizeof(on)) == -1)
			perror("WARNING: setsockopt(TCP_NODELAY)");

		/* Make sure TCP keepalive sent regardless of text keepalive
		   set or not */
		if (setsockopt(sock,SOL_SOCKET,SO_KEEPALIVE,&on,sizeof(on)) == -1)
			perror("WARNING: setsockopt(SO_KEEPALIVE)");
		break;
	}

	/* Bind to local port if required */
	if (lport) 
	{
		/* sockaddr_in6 and sockaddr_in are not interchangable so
		   need seperate code paths */
		if (sockdomain == AF_INET)
		{
			/* IP4 */
			bzero((char *)&bind_addr,sizeof(bind_addr));

			bind_addr.sin_family = AF_INET;
			bind_addr.sin_addr.s_addr = INADDR_ANY;
			bind_addr.sin_port = htons(lport);

			bind_ptr = (struct sockaddr *)&bind_addr;
			bind_size = sizeof(bind_addr);

			if (bind(sock,bind_ptr,bind_size) == -1)
			{
				perror("ERROR: bind()");
				doExit(1);
			}
		}
#ifdef IP6
		else
		{
			/* IP6 */
			bzero((char *)&bind_addr6,sizeof(bind_addr6));

			bind_addr6.sin6_family = AF_INET6;
			bind_addr6.sin6_flowinfo = 0;  /* Not used yet */
			bind_addr6.sin6_port = htons(lport);
			bind_addr6.sin6_addr = in6addr_any;

			bind_ptr = (struct sockaddr *)&bind_addr6;
			bind_size = sizeof(bind_addr6);

			if (bind(sock,bind_ptr,bind_size) == -1)
			{
				perror("ERROR: bind()");
				doExit(1);
			}
		}
#endif
	}

	proto = (protocol == IPPROTO_TCP ? "TCP" : 
	        (protocol == IPPROTO_UDP ? "UDP" : "SCTP"));
	ret = -1;

	/* Resolve address then do connect */
#ifdef IP6
	res = NULL;
	bzero((char *)&hints,sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST;
	hints.ai_family = sockdomain;
	hints.ai_socktype = socktype;
	if (getaddrinfo(host,portstr,&hints,&res)) 
	{
		hints.ai_flags = AI_CANONNAME;
		if (getaddrinfo(host,portstr,&hints,&res)) 
		{
			fprintf(stderr,"ERROR: Can't resolve hostname '%s'.\n",host);
			doExit(1);
		}
		name = res->ai_canonname ? res->ai_canonname : UNRESOLVED;

		/* Go through all addresses */
		for(;res;res=res->ai_next) 
		{
			printf("Trying %s (%s) %s port %s...\n",
				inet_ntop(
					sockdomain,
					sockdomain == AF_INET6 ? 
						(void *)&((struct sockaddr_in6 *)res->ai_addr)->sin6_addr : 
						(void *)&((struct sockaddr_in *)res->ai_addr)->sin_addr,
					ipnum,
					sizeof(ipnum)),
				name,proto,portstr);

			/* Just use the first for multicast and hope its ok */
			if (flags.join_multicast)
			{
				ret = 0;
				break;
			}

			if ((ret = connect(
				sock,res->ai_addr,res->ai_addrlen)) == -1)
			{
				/* Don't exit because we want to try the next */
				perror("ERROR: connect()"); 
			}
			else break;
		}
		if (ret == -1) doExit(1);
	}
	else 
	{
		/* Numeric address given */
		printf("Trying %s (%s) %s port %s...\n",
			inet_ntop(
				sockdomain,
				sockdomain == AF_INET6 ? 
					(void *)&((struct sockaddr_in6 *)res->ai_addr)->sin6_addr : 
					(void *)&((struct sockaddr_in *)res->ai_addr)->sin_addr,
				ipnum,
				sizeof(ipnum)),
			UNRESOLVED,proto,portstr);

		if (!flags.join_multicast && 
		    connect(sock,res->ai_addr,res->ai_addrlen) == -1) 
		{
			perror("ERROR: connect()");
			doExit(1); 
		}
	}
	if (flags.join_multicast)
	{
		memcpy(&con_addr,res->ai_addr,sizeof(con_addr));
		joinMulticastGroup();
	}
#else
	/* IP4 */
	bzero((char *)&con_addr,sizeof(con_addr));
	con_addr.sin_family = AF_INET;
	con_addr.sin_port = htons(port);

	if ((int)(con_addr.sin_addr.s_addr = inet_addr(host)) == -1) 
	{
		/* DNS name given */
		if (!(hp = gethostbyname(host))) 
		{
			fprintf(stderr,"ERROR: Can't resolve hostname '%s'.\n",host);
			doExit(1);
		}
		name = hp->h_name ? hp->h_name : UNRESOLVED;

		/* Try all addresses in turn */
		for(ptr=hp->h_addr_list;*ptr;++ptr) 
		{
			memcpy((char *)&con_addr.sin_addr.s_addr,*ptr,hp->h_length);
	
			printf("Trying %s (%s) %s port %d...\n",
				inet_ntoa(con_addr.sin_addr),name,proto,port);

			if (flags.join_multicast) 
			{
				ret = 0;
				break;
			}

			if ((ret=connect(
				sock,
				(struct sockaddr *)&con_addr,
				sizeof(con_addr))) == -1)
			{
				perror("ERROR: connect()");
			}
			else break;
		}
		if (ret == -1) doExit(1);
	}
	else 
	{
		/* Numeric address given */
		printf("Trying %s (%s) %s port %d...\n",
			inet_ntoa(con_addr.sin_addr),UNRESOLVED,proto,port);

		if (!flags.join_multicast && connect(
			sock,
			(struct sockaddr *)&con_addr,sizeof(con_addr)) == -1) 
		{
			perror("ERROR: connect()");
			doExit(1); 
		}
	}
	if (flags.join_multicast) joinMulticastGroup();
#endif

	CONNECTED:
	flags.connected = 1;
	puts("*** Connected ***");
}




/*** A lot of stuff needs to be set up. Have to bind to a port so we become
     a mini server ***/
void joinMulticastGroup()
{
	struct ip_mreq imreq;
	int on;

	bzero(&imreq,sizeof(imreq));
	imreq.imr_multiaddr.s_addr = con_addr.sin_addr.s_addr;
	imreq.imr_interface.s_addr = INADDR_ANY;

	/* Join group */
	if (setsockopt(
		sock,
		IPPROTO_IP,
		IP_ADD_MEMBERSHIP,&imreq,sizeof(imreq)) == -1)
	{
		perror("ERROR: setsockopt(IP_ADD_MEMBERSHIP)");
		doExit(1);
	}

	/* Make sure messages get looped back to local machine */
	on = 1;
        if (setsockopt(
                sock,
                IPPROTO_IP,IP_MULTICAST_LOOP,(char *)&on,sizeof(on)) == -1)
        {
                perror("setsockopt(IP_MULTICAST_LOOP)");
                doExit(1);
        }

	if (setsockopt(
		sock,SOL_SOCKET,SO_REUSEADDR,(char *)&on,sizeof(on)) == -1)
	{
		perror("setsockopt(SO_REUSEADDR)");
		doExit(1);
	}

	/* Bind */
	if (bind(
		sock,
		(struct sockaddr *)&con_addr,sizeof(struct sockaddr)) == -1)
	{
		perror("ERROR: bind()");
		doExit(1);
	}
#ifdef IP6
        printf("Listening on group %s:%s.\n",host,portstr);
#else
        printf("Listening on group %s:%d.\n",host,port);
#endif
}




void readSock()
{
	struct sockaddr_in from;
	socklen_t size;

#ifdef SSLYR
	/* flags.ssl can never be set if SSLYR not set */
	if (flags.ssl)
	{
		/* SSL can buffer data which we might not have read yet but 
		   won't come up in a select() hence SSL_pending() loop. 
		   Shouldn't need to do this as BUFFSIZE should be large enough
		   but just in case... */
		do
		{
			ERR_clear_error();

			if (!(bufflen = SSL_read(ssl_handle,buff,BUFFSIZE)))
				doExit(0);

			if (bufflen < 0)
			{
				switch(SSL_get_error(ssl_handle,bufflen))
				{
				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_WANT_WRITE:
					continue;
				}
				sslError("SSL_read");
			}
			printBuffer();

		} while(SSL_pending(ssl_handle));

		return;
	}
#endif
	if (flags.join_multicast)
	{
		size = sizeof(struct sockaddr_in);

		if ((bufflen = recvfrom(
			sock,buff,BUFFSIZE,0,
			(struct sockaddr *)&from,&size)) == -1)
		{
			perror("ERROR: recvfrom()");
			doExit(1);
		}

		printf("[%s:%d]\n",
			inet_ntoa(from.sin_addr),
			ntohs(from.sin_port));
	}
	else if ((bufflen = read(sock,buff,BUFFSIZE)) < 1) doExit(0);

	printBuffer();
}




void writeSock(u_char *data, int len)
{
	int total;
	int cnt;
	int wlen;

	if (len < 1) return;

	if (flags.log_writes) logIO(data,len,"TX");
	if (flags.hexdump) hexdump(data,len,0);
#ifdef SSLYR
	if (flags.ssl)
	{
		ERR_clear_error();

		do
		{
			if ((len = SSL_write(ssl_handle,data,len)) < 1)
			{
				switch(SSL_get_error(ssl_handle,len))
				{
				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_WANT_WRITE:
					continue;

				default:
					sslError("SSL_write");
					doExit(0);
				}
			}
		} while(len < 1);
		return;
	}
#endif
	for(total=0,cnt=0;total < len;)
	{
		if (flags.join_multicast)
		{
			wlen = sendto(
				sock,
				data + total,
				len - total,
				0,
				(struct sockaddr *)&con_addr,sizeof(con_addr));
		}
		else wlen = write(sock,data + total,len - total);

		if (wlen == -1)
		{
			/* Retry up to 10 times then forget it */
			if (cnt < 10 && (errno == EINTR || errno == EAGAIN))
			{
				/* Pause for a moments contemplation... */
				usleep(100000);
				++cnt;
				continue;
			}
			perror("ERROR: write()/sendto()");
			doExit(1);
		}
		total += wlen;
	}
}
