#include <pwd.h>
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <stdbool.h>

#include "config.h"
#include "screen.h"
#include "extern.h"

#include "attacher.h"

bool CheckPassword() {
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
		return true;
	}
	return false;
}

void Authenticate() {
	while (1) {
		errno = 0;

		if(CheckPassword()) {
			break;
		}

	}
}
