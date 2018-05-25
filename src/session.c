
#include "config.h"

#include "session.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "screen.h"

Session *MakeSession(void)
{
	char *cptr;
	long host_name_max = 0;
	Session *session;

	session = calloc(1, sizeof(Session));
	if (!session)
		Panic(0, "%s", strnomem);

	host_name_max = sysconf(_SC_HOST_NAME_MAX);
	session->s_hostname = calloc(host_name_max + 1, sizeof(char));
	if (!session->s_hostname)
		Panic(0, "%s", strnomem);
	(void)gethostname(session->s_hostname, host_name_max + 1);
	session->s_hostname[host_name_max] = '\0';
	if ((cptr = strchr(session->s_hostname, '.')) != NULL)
		*cptr = '\0';

	return session;
}

void FreeSession(Session *session)
{
	free(session->s_hostname);

	free(session);

	return;
}
