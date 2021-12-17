#include "globals.h"

#ifdef SSLYR

void sslConnect()
{
	ssl_handle = NULL;
	ssl_context = NULL;

	puts("Trying SSL negotiation...");
	SSL_library_init();
	SSL_load_error_strings();

	/* Create context and handle */
	if (!(ssl_context = SSL_CTX_new(SSLv23_client_method())))
		sslError("SSL_CTX_new");
	if (!(ssl_handle = SSL_new(ssl_context)))
		sslError("SSL_new");

	/* Link to socket */
	if (!SSL_set_fd(ssl_handle,sock)) sslError("SSL_set_fd");

	/* Do initialisation handshake */
	if (SSL_connect(ssl_handle) != 1) sslError("SSL_connect");

	puts("*** SSL OK ***");
}




void sslError(char *func)
{
	char *estr;
	int err;

	/* For some reason a remote close doesn't come up as an error */
	if (!(err = ERR_get_error()))
		estr = "Connection closed";
	else
		estr = (char *)ERR_reason_error_string(err);
	printf("ERROR %s(): %s",func,estr);
	while((err = ERR_get_error()))
		printf(",%s",ERR_reason_error_string(err));
	putchar('\n');

	close(sock);
	if (ssl_handle)
	{
		SSL_shutdown(ssl_handle);
		SSL_free(ssl_handle);
	}
	if (ssl_context) SSL_CTX_free(ssl_context);

	doExit(0);
}

#endif
