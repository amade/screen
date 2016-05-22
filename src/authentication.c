#include "config.h"

#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>

#include "screen.h"

#include "attacher.h"

#if ENABLE_PAM
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#else
#include <shadow.h>
#endif

static bool CheckPassword() {
	bool ret = false;
#if ENABLE_PAM
	pam_handle_t *pamh = 0;
	struct pam_conv pamc;
	int pam_error;
	char *tty_name;

	pamc.conv = &misc_conv; 
	pamc.appdata_ptr = NULL;
	pam_error = pam_start("screen", ppp->pw_name, &pamc, &pamh);
	if (pam_error != PAM_SUCCESS) {
		AttacherFinit(0);  /* goodbye */
	}

	if (strncmp(attach_tty, "/dev/", 5) == 0) {
		tty_name = attach_tty + 5;
	} else {
		tty_name = attach_tty;
	}
	pam_error = pam_set_item(pamh, PAM_TTY, tty_name);
	if (pam_error != PAM_SUCCESS) {
		AttacherFinit(0);  /* goodbye */
	}

	printf("\aScreen used by %s%s<%s> on %s.\n",
     		ppp->pw_gecos, ppp->pw_gecos[0] ? " " : "", ppp->pw_name, HostName);
	pam_error = pam_authenticate(pamh, 0);
	pam_end(pamh, pam_error);
	if (pam_error == PAM_SUCCESS) {
		ret = true;
	}
#else
	struct spwd *p;
	char *passwd = 0;

	p = getspnam(ppp->pw_name);

	printf("\aScreen used by %s%s<%s> on %s.\n",
		ppp->pw_gecos, ppp->pw_gecos[0] ? " " : "", ppp->pw_name, HostName);
	passwd = crypt(getpass("Password:"), p->sp_pwdp);

	ret = (strcmp(provided, p->sp_pwdp) == 0);

	free(p);
	free(passwd);
#endif
	return ret;
}

void Authenticate() {
	uint8_t tries = 0;
	while (1) {
		if(CheckPassword()) {
			break;
		}
		if (tries < 3)
			tries++;
		else
			sleep(1); /* after 3 failures limit tries per minute */

	}
}
