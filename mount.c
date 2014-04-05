/* See LICENSE file for copyright and license details. */
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "util.h"

struct {
	const char *opt;
	const char *notopt;
	unsigned long v;
} optnames[] = {
	{ "defaults",   NULL,           0              },
	{ "remount",    NULL,           MS_REMOUNT     },
	{ "ro",         "rw",           MS_RDONLY      },
	{ "sync",       "async",        MS_SYNCHRONOUS },
	{ "dirsync",    NULL,           MS_DIRSYNC     },
	{ "nodev",      "dev",          MS_NODEV       },
	{ "noatime",    "atime",        MS_NOATIME     },
	{ "nodiratime", "diratime",     MS_NODIRATIME  },
	{ "noexec",     "exec",         MS_NOEXEC      },
	{ "nosuid",     "suid",         MS_NOSUID      },
	{ "mand",       "nomand",       MS_MANDLOCK    },
	{ "relatime",   "norelatime",   MS_RELATIME    },
	{ "bind",       NULL,           MS_BIND        },
	{ NULL,         NULL,           0              }
};

static void
parseopts(char *popts, unsigned long *flags, char *data, size_t datasiz)
{
	unsigned int i, validopt;
	size_t optlen, dlen = 0;
	char *name;

	data[0] = '\0';
	for(name = strtok(popts, ","); name; name = strtok(NULL, ",")) {
		validopt = 0;
		for(i = 0; optnames[i].opt; i++) {
			if(optnames[i].opt && strcmp(name, optnames[i].opt) == 0) {
				*flags |= optnames[i].v;
				validopt = 1;
				break;
			}
			if(optnames[i].notopt && strcmp(name, optnames[i].notopt) == 0) {
				*flags &= ~optnames[i].v;
				validopt = 1;
				break;
			}
		}
		if(!validopt) {
			/* unknown option, pass as data option to mount() */
			if((optlen = strlen(name))) {
				if(dlen + optlen + 2 >= datasiz)
					return; /* prevent overflow */
				if(dlen)
					data[dlen++] = ',';
				memcpy(&data[dlen], name, optlen);
				dlen += optlen;
				data[dlen] = '\0';
			}
		}
	}
}

static int
mounted(const char *dir)
{
	FILE *fp;
	struct mntent *me;
	struct stat st1, st2;

	if (stat(dir, &st1) < 0) {
		 weprintf("stat %s:", dir);
		 return 0;
	}
	fp = setmntent("/proc/mounts", "r");
	if (!fp)
		eprintf("setmntent %s:", "/proc/mounts");
	while ((me = getmntent(fp)) != NULL) {
		if (stat(me->mnt_dir, &st2) < 0) {
			 weprintf("stat %s:", me->mnt_dir);
			 continue;
		}
		if (st1.st_dev == st2.st_dev &&
		    st1.st_ino == st2.st_ino)
			return 1;
	}
	endmntent(fp);
	return 0;
}

static void
usage(void)
{
	eprintf("usage: %s [-BMRan] [-t fstype] [-o options] [source] [target]\n",
		argv0);
}

static int
catfile(FILE *in, FILE *out)
{
	char buf[BUFSIZ];
	size_t bytesread;

	while(1) {
		if(feof(in))
			break;
		bytesread = fread(buf, 1, sizeof(buf), in);
		if(ferror(in))
			return 0;
		fwrite(buf, 1, bytesread, out);
	}
	return 1;
}

int
main(int argc, char *argv[])
{
	int aflag = 0, oflag = 0, status = EXIT_SUCCESS, i;
	unsigned long flags = 0;
	char *types = NULL, data[512] = "";
	char *files[] = { "/proc/mounts", "/etc/fstab", NULL };
	size_t datasiz = sizeof(data);
	const char *source, *target;
	struct stat st;
	struct mntent *me = NULL;
	FILE *fp;

	ARGBEGIN {
	case 'B':
		flags |= MS_BIND;
		break;
	case 'M':
		flags |= MS_MOVE;
		break;
	case 'R':
		flags |= MS_REC;
		break;
	case 'a':
		aflag = 1;
		break;
	case 'o':
		oflag = 1;
		parseopts(EARGF(usage()), &flags, data, datasiz);
		break;
	case 't':
		types = EARGF(usage());
		break;
	case 'n':
		break;
	default:
		usage();
	} ARGEND;

	if(argc < 1 && aflag == 0) {
		if(!(fp = fopen(files[0], "r")))
			eprintf("fopen %s:", files[0]);
		if(catfile(fp, stdout) != 1) {
			weprintf("error while reading %s:", files[0]);
			status = EXIT_FAILURE;
		}
		fclose(fp);
		return status;
	}

	if(aflag == 1)
		goto mountall;

	source = argv[0];
	target = argv[1];

	if(!target) {
		target = argv[0];
		source = NULL;
		if(stat(target, &st) < 0)
			eprintf("stat %s:", target);
	}

	for(i = 0; files[i]; i++) {
		if(!(fp = setmntent(files[i], "r"))) {
			if (strcmp(files[i], "/proc/mounts") != 0)
				weprintf("setmntent %s:", files[i]);
			continue;
		}
		while((me = getmntent(fp))) {
			if(strcmp(me->mnt_dir, target) == 0 ||
			   strcmp(me->mnt_fsname, target) == 0 ||
			   (source && strcmp(me->mnt_dir, source) == 0) ||
			   (source && strcmp(me->mnt_fsname, source) == 0)) {
				if(!source) {
					target = me->mnt_dir;
					source = me->mnt_fsname;
				}
				if(!oflag)
					parseopts(me->mnt_opts, &flags, data, datasiz);
				if(!types)
					types = me->mnt_type;
				goto mountsingle;
			}
		}
		endmntent(fp);
		fp = NULL;
	}
	if(!source)
		eprintf("can't find %s in /etc/fstab\n", target);

mountsingle:
	if(mount(source, target, types, flags, data) < 0) {
		weprintf("mount: %s:", source);
		status = EXIT_FAILURE;
	}
	if(fp)
		endmntent(fp);
	return status;

mountall:
	if(!(fp = setmntent("/etc/fstab", "r")))
		eprintf("setmntent %s:", "/etc/fstab");
	while((me = getmntent(fp))) {
		flags = 0;
		parseopts(me->mnt_opts, &flags, data, datasiz);
		if(mount(me->mnt_fsname, me->mnt_dir, me->mnt_type, flags, data) < 0) {
			if(mounted(me->mnt_dir) == 0) {
				weprintf("mount: %s:", me->mnt_fsname);
				status = EXIT_FAILURE;
			}
		}
	}
	endmntent(fp);

	return status;
}
