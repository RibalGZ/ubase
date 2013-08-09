/* See LICENSE file for copyright and license details. */
#include <sys/mount.h>
#include <stdio.h>
#include "../ubase.h"
#include "../util.h"

int
do_umount(const char *target, int opts)
{
	int flags = 0;

	if (opts & UBASE_MNT_FORCE)
		flags |= MNT_FORCE;
	if (opts & UBASE_MNT_DETACH)
		flags |= MNT_DETACH;
	return umount2(target, flags);
}