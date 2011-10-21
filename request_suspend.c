/* Request suspend
 * Create the suspend-request file, then wait for it to be deleted.
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

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

static void catch(int sig)
{
	return;
}

main(int argc, char *argv[])
{
	int dirfd = open("/var/run/suspend", O_RDONLY);
	int fd_request = open("/var/run/suspend/request",
			      O_RDWR|O_CREAT, 0640);
	if (fd_request < 0)
		exit(1);


	if (dirfd < 0)
		exit(1);
	/* Wait for unlink */
	while (1) {
		int fd;
		struct stat stat;
		sigset_t set, oldset;
		sigemptyset(&set);
		sigaddset(&set, SIGIO);

		sigprocmask(SIG_BLOCK, &set, &oldset);
		signal(SIGIO, catch);

		fcntl(dirfd, F_NOTIFY, DN_DELETE);

		if (fstat(fd_request, &stat) != 0
		    || stat.st_nlink == 0)
			exit(0);
		fd = open("/var/run/suspend/disabled", O_RDONLY);
		if (fd >= 0) {
			if (flock(fd, LOCK_EX|LOCK_NB) != 0) {
				/* Someone is blocking suspend */
				unlink("/var/run/suspend/request");
				exit(1);
			}
			close(fd);
		}

		sigsuspend(&oldset);
		sigprocmask(SIG_UNBLOCK, &set, &oldset);
	}
}
