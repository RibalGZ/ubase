/* See LICENSE file for copyright and license details. */
#include <sys/ioctl.h>
#include <sys/types.h>

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#include "config.h"
#include "passwd.h"
#include "util.h"

static int dologin(struct passwd *, int);

static void
usage(void)
{
	eprintf("usage: %s [-p] username\n", argv0);
}

int
main(int argc, char *argv[])
{
	struct passwd *pw;
	struct utmp usr;
	FILE *fp;
	char *pass;
	char *tty;
	uid_t uid;
	gid_t gid;
	int pflag = 0;

	ARGBEGIN {
	case 'p':
		pflag = 1;
		break;
	default:
		usage();
	} ARGEND;

	if (argc < 1)
		usage();

	if (isatty(STDIN_FILENO) == 0)
		eprintf("stdin is not a tty\n");

	errno = 0;
	pw = getpwnam(argv[0]);
	if (errno)
		eprintf("getpwnam: %s:", argv[0]);
	else if (!pw)
		eprintf("who are you?\n");

	uid = pw->pw_uid;
	gid = pw->pw_gid;

	/* Flush pending input */
	ioctl(STDIN_FILENO, TCFLSH, (void *)0);

	pass = getpass("Password: "); putchar('\n');
	if (!pass)
		eprintf("getpass:");
	if (pw_check(pw, pass) <= 0)
		exit(EXIT_FAILURE);

	if (initgroups(argv[0], gid) < 0)
		eprintf("initgroups:");
	if (setgid(gid) < 0)
		eprintf("setgid:");
	if (setuid(uid) < 0)
		eprintf("setuid:");

	/* Write utmp entry */
	memset(&usr, 0, sizeof(usr));

	tty = ttyname(STDIN_FILENO);
	if (!tty)
		tty = "?";
	usr.ut_type = USER_PROCESS;
	usr.ut_pid = getpid();
	strlcpy(usr.ut_user, argv[0], sizeof(usr.ut_user));
	strlcpy(usr.ut_line, tty, sizeof(usr.ut_line));
	usr.ut_tv.tv_sec = time(NULL);

	fp = fopen("/var/run/utmp", "a");
	if (!fp)
		weprintf("fopen %s:", "/var/run/utmp");
	fwrite(&usr, sizeof(usr), 1, fp);
	fclose(fp);

	return dologin(pw, pflag);
}

static int
dologin(struct passwd *pw, int preserve)
{
	char *shell = pw->pw_shell[0] == '\0' ? "/bin/sh" : pw->pw_shell;

	if (preserve == 0)
		clearenv();
	setenv("HOME", pw->pw_dir, 1);
	setenv("SHELL", shell, 1);
	setenv("USER", pw->pw_name, 1);
	setenv("LOGNAME", pw->pw_name, 1);
	setenv("PATH", ENV_PATH, 1);
	if (chdir(pw->pw_dir) < 0)
		eprintf("chdir %s:", pw->pw_dir);
	execlp(shell, shell, "-l", NULL);
	weprintf("execlp %s:", shell);
	return (errno == ENOENT) ? 127 : 126;
}
