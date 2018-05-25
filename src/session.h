
#ifndef SCREEN_SESSION_H
#define SCREEN_SESSION_H

typedef struct Session Session;
struct Session {
	char	*s_hostname;
};

Session *MakeSession(void);
void FreeSession(Session *);

#endif /* SCREEN_SESSION_H */
