/* See LICENSE file for copyright and license details. */
#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "proc.h"
#include "queue.h"
#include "util.h"

struct {
	const char *name;
	int sig;
} sigs[] = {
#define SIG(n) { #n, SIG##n }
	SIG(ABRT), SIG(ALRM), SIG(BUS),  SIG(CHLD), SIG(CONT), SIG(FPE),  SIG(HUP),
	SIG(ILL),  SIG(INT),  SIG(KILL), SIG(PIPE), SIG(QUIT), SIG(SEGV), SIG(STOP),
	SIG(TERM), SIG(TSTP), SIG(TTIN), SIG(TTOU), SIG(USR1), SIG(USR2), SIG(URG),
#undef SIG
};

static void
usage(void)
{
	eprintf("usage: %s [-o pid1,pid2,..,pidN] [-s signal]\n", argv0);
}

struct pidentry {
	pid_t pid;
	TAILQ_ENTRY(pidentry) entry;
};

static TAILQ_HEAD(omitpid_head, pidentry) omitpid_head;

int
main(int argc, char *argv[])
{
	struct pidentry *pe, *tmp;
	int oflag = 0;
	char *p, *arg = NULL;
	DIR *dp;
	struct dirent *entry;
	char *end, *v;
	int sig = SIGTERM;
	pid_t pid;
	size_t i;

	ARGBEGIN {
	case 's':
		v = EARGF(usage());
		sig = strtol(v, &end, 0);
		if(*end == '\0')
			break;
		for(i = 0; i < LEN(sigs); i++) {
			if(strcasecmp(v, sigs[i].name) == 0) {
				sig = sigs[i].sig;
				break;
			}
		}
		if(i == LEN(sigs))
			eprintf("%s: unknown signal\n", v);
		break;
	case 'o':
		oflag = 1;
		arg = EARGF(usage());
		break;
	default:
		usage();
	} ARGEND;

	TAILQ_INIT(&omitpid_head);

	for (p = strtok(arg, ","); p; p = strtok(NULL, ",")) {
		pe = emalloc(sizeof(*pe));
		pe->pid = estrtol(p, 10);
		TAILQ_INSERT_TAIL(&omitpid_head, pe, entry);
	}

	if (sig != SIGSTOP && sig != SIGCONT)
		kill(-1, SIGSTOP);

	if (!(dp = opendir("/proc")))
		eprintf("opendir /proc:");
	while ((entry = readdir(dp))) {
		if (pidfile(entry->d_name) == 0)
			continue;
		pid = estrtol(entry->d_name, 10);
		if (pid == 1 || pid == getpid() ||
		    getsid(pid) == getsid(0) || getsid(pid) == 0)
			continue;
		if (oflag == 1) {
			TAILQ_FOREACH(pe, &omitpid_head, entry)
				if (pe->pid == pid)
					break;
			if (pe)
				continue;
		}
		kill(pid, sig);
	}
	closedir(dp);

	if (sig != SIGSTOP && sig != SIGCONT)
		kill(-1, SIGCONT);

	for (pe = TAILQ_FIRST(&omitpid_head); pe; pe = tmp) {
		tmp = TAILQ_NEXT(pe, entry);
		free(pe);
	}

	return EXIT_SUCCESS;
}
