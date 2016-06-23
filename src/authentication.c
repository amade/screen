#include "config.h"

#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>

#include "screen.h"

#include "attacher.h"

#if ENABLE_PAM
#include <security/pam_appl.h>
#else
#include <shadow.h>
#endif


#if ENABLE_PAM
int screen_conv(int num_msg, const struct pam_message **msg,
		struct pam_response **resp, void *data)
{
	struct pam_response *reply;
	int i;

	(void)data; /* unused */

	if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG)
		return PAM_CONV_ERR;

	if ((reply = calloc(num_msg, sizeof(*reply))) == NULL)
		return PAM_BUF_ERR;

	for (i = 0; i < num_msg; ++i) {
		reply[i].resp_retcode = 0;
		reply[i].resp = NULL;
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
			reply[i].resp = strdup(getpass(msg[i]->msg));
			if (reply[i].resp == NULL)
				goto fail;
			break;
		case PAM_TEXT_INFO:
			/* ignore */
			break;
		case PAM_PROMPT_ECHO_ON:
			/* user name given to PAM already */
			/* fall through */
		case PAM_ERROR_MSG:
		default:
			goto fail;
		}
	}
	*resp = reply;
	return PAM_SUCCESS;
 fail:
        for (i = 0; i < num_msg; ++i) {
                if (reply[i].resp != NULL) {
                        memset(reply[i].resp, 0, strlen(reply[i].resp));
                        free(reply[i].resp);
                }
        }
        memset(reply, 0, num_msg * sizeof *reply);
	free(reply);
	*resp = NULL;
	return PAM_CONV_ERR;
}

static bool CheckPassword() {
	bool ret = false;

	pam_handle_t *pamh = 0;
	struct pam_conv pamc;
	int pam_ret;
	char *tty_name;

	pamc.conv = &screen_conv; 
	pamc.appdata_ptr = NULL;
	pam_ret= pam_start("screen", ppp->pw_name, &pamc, &pamh);
	if (pam_ret!= PAM_SUCCESS) {
		AttacherFinit(0);  /* goodbye */
	}

	if (strncmp(attach_tty, "/dev/", 5) == 0) {
		tty_name = attach_tty + 5;
	} else {
		tty_name = attach_tty;
	}
	pam_ret = pam_set_item(pamh, PAM_TTY, tty_name);
	if (pam_ret != PAM_SUCCESS) {
		AttacherFinit(0);  /* goodbye */
	}

	printf("\ascreen used by %s%s<%s> on %s.\n",
     		ppp->pw_gecos, ppp->pw_gecos[0] ? " " : "", ppp->pw_name, HostName);
	pam_ret = pam_authenticate(pamh, 0);
	pam_end(pamh, pam_ret);

	if (pam_ret == PAM_MAXTRIES) {
		fprintf(stderr, "maximum number of tries exceeded\n");
		AttacherFinit(0);  /* goodbye */
	} else if (pam_ret == PAM_ABORT) {
		fprintf(stderr, "abort requested by PAM\n");
		AttacherFinit(0);  /* goodbye */
	} else if (pam_ret == PAM_SUCCESS) {
		ret = true;
	}

	return ret;
}

#else /* ENABLE_PAM */

static bool CheckPassword() {
	bool ret = false;
	struct spwd *p;
	char *passwd = 0;
	gid_t gid;
	uid_t uid;

	uid = geteuid();
	gid = getegid();
	seteuid(0);
	setegid(0);
	p = getspnam(ppp->pw_name);
	seteuid(uid);
	setegid(gid);
	if (p == NULL)
		fprintf(stderr, "can't open passwd file\n");

	printf("\ascreen used by %s%s<%s> on %s.\n",
		ppp->pw_gecos, ppp->pw_gecos[0] ? " " : "", ppp->pw_name, HostName);
	passwd = crypt(getpass("Password:"), p->sp_pwdp);

	ret = (strcmp(passwd, p->sp_pwdp) == 0);

	free(p);
	free(passwd);

	return ret;
}
#endif /* ENABLE_PAM */

void Authenticate() {
	uint8_t tries = 0;
	while (tries < 3) {
		if (CheckPassword())
			return;
		tries++;
	}
	AttacherFinit(0);  /* goodbye */
}
