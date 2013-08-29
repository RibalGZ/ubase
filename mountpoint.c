/* See LICENSE file for copyright and license details. */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grabmntinfo.h"
#include "util.h"

static void
usage(void)
{
	eprintf("usage: %s [-q] target\n", argv0);
}

int
main(int argc, char *argv[])
{
	int i;
	int qflag = 0;
	struct mntinfo *minfo = NULL;
	int siz;
	int ret = 0;
	struct stat st;

	ARGBEGIN {
	case 'q':
		qflag = 1;
		break;
	default:
		usage();
	} ARGEND;

	if (argc < 1)
		usage();

	if (stat(argv[0], &st) < 0)
		eprintf("stat %s:", argv[0]);

	if (!S_ISDIR(st.st_mode))
		eprintf("lstat %s: not a directory\n", argv[0]);

	siz = grabmntinfo(&minfo);
	if (!siz)
		eprintf("grabmntinfo:");
	for (i = 0; i < siz; i++)
		if (!strcmp(minfo[i].mntdir, argv[0]))
			break;
	free(minfo);

	if (i == siz)
		ret = 1;

	if (!qflag)
		printf("%s %s a mountpoint\n", argv[0],
		       !ret ? "is" : "is not");

	return ret;
}