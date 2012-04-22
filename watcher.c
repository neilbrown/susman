
/*
 * Use libevent to watch for suspends and take action.
 * The calling program must already have a libevent loop running.
 * One or two callbacks are registered with suspend_watch.
 * The first is required and gets called just before suspend.
 * It must return promptly but may call suspend_block first.
 * The second is options and will get called after resume.
 *
 *
 * Copyright (C) 2011 Neil Brown <neilb@suse.de>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <event.h>
#include <sys/stat.h>
#include <malloc.h>

struct cb {
	int (*will_suspend)(void *data);
	void (*did_resume)(void *data);
	void *data;
	int dirfd;
	int fd, nextfd;
	struct event ev;
};

static void checkdir(int efd, short ev, void *vp)
{
	struct cb *han = vp;
	struct stat stb;
	int fd;
	int rv;

	if (han->fd < 0)
		/* too early */
		return;

	if (han->nextfd >= 0) {
		/*suspended - maybe not any more */
		fstat(han->fd, &stb);
		if (stb.st_size <= 0)
			/*false alarm */
			return;
		/* back from resume */
		close(han->fd);
		han->fd = han->nextfd;
		han->nextfd = -1;
		if (han->did_resume)
			han->did_resume(han->data);
		/* Fall through incase suspend has started again */
	}

	/* not suspended yet */
	if (fstat(han->fd, &stb) == 0
	    && stb.st_size == 0)
		/* false alarm */
		return;

	/* We need to move on now. */
	fd = open("/run/suspend/watching-next", O_RDONLY|O_CLOEXEC);
	flock(fd, LOCK_SH);
	han->nextfd = fd;
	rv = han->will_suspend(han->data);
	if (rv)
		suspend_ok(han);
}

int suspend_ok(void *v)
{
	struct cb *han = v;
	flock(han->fd, LOCK_UN);
	return 1;
}

void *suspend_watch(int (*will_suspend)(void *data),
		    void (*did_resume)(void *data),
		    void *data)
{
	struct cb *han = malloc(sizeof(*han));
	struct stat stb;
	int fd = -1;
	if (!han)
		return NULL;

	han->data = data;
	han->will_suspend = will_suspend;
	han->did_resume = did_resume;
	han->fd = -1;
	han->nextfd = -1;
	signal_set(&han->ev, SIGIO, checkdir, han);
	signal_add(&han->ev, NULL);
	han->dirfd = open("/run/suspend", O_RDONLY|O_CLOEXEC);
	if (han->dirfd < 0)
		goto abort;
	fcntl(han->dirfd, F_NOTIFY, DN_MODIFY | DN_MULTISHOT);
again:
	fd = open("/run/suspend/watching", O_RDONLY|O_CLOEXEC);
	flock(fd, LOCK_SH);
	han->fd = fd;
	checkdir(0, 0, han);
	/* OK, he won't suspend until I say OK. */

	return han;
abort:
	signal_del(&han->ev);
	if (fd >= 0)
		close(fd);
	if (han->dirfd >= 0)
		close(han->dirfd);
	free(han);
}

void suspend_unwatch(void *v)
{
	struct cb *han = v;
	if (han->dirfd >= 0)
		close(han->dirfd);
	signal_del(&han->ev);
	if (han->fd >= 0)
		close(han->fd);
	if (han->nextfd >= 0)
		close(han->nextfd);
	free(han);
}

