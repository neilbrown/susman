/*
 * lsusd - Linus SUSpend daemon.
 * This daemon enters suspend when required and allows clients
 * to block suspend, request suspend, or be notified of suspend.
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
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>

static void alert_watchers(void)
{
	int fd;
	char zero = 0;

	fd = open("/run/suspend/watching-next",
		  O_RDWR|O_CREAT|O_TRUNC, 0640);
	if (fd < 0)
		return;
	close(fd);
	fd = open("/run/suspend/watching",
		  O_RDWR|O_CREAT|O_TRUNC, 0640);
	if (fd < 0)
		return;
	if (write(fd, &zero, 1) != 1)
		return;
	flock(fd, LOCK_EX);
	/* all watches must have moved to next file */
	close(fd);
}

static void cycle_watchers(void)
{
	int fd;
	char zero[2];

	fd = open("/run/suspend/watching", O_RDWR|O_CREAT, 0640);
	if (fd < 0)
		return;
	rename("/run/suspend/watching-next",
	       "/run/suspend/watching");
	zero[0] = zero[1] = 0;
	write(fd, zero, 2);
	close(fd);
}

static int read_wakeup_count()
{
	int fd;
	int n;
	char buf[20];

	fd = open("/sys/power/wakeup_count", O_RDONLY);
	if (fd < 0)
		return -1;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if (n < 0)
		return -1;
	buf[n] = 0;
	return atoi(buf);
}

static int set_wakeup_count(int count)
{
	int fd;
	char buf[20];
	int n;

	if (count < 0)
		return 1; /* Something wrong - just suspend */

	fd = open("/sys/power/wakeup_count", O_RDWR);
	if (fd < 0)
		return 1;

	snprintf(buf, sizeof(buf), "%d", count);
	n = write(fd, buf, strlen(buf));
	close(fd);
	if (n < 0)
		return 0;
	return 1;
}

static void catch(int sig)
{
	return;
}

static void wait_request(int dirfd)
{
	int found_immediate = 0;
	int found_request = 0;

	do {
		DIR *dir;
		struct dirent *de;
		sigset_t set, oldset;
		sigemptyset(&set);
		sigaddset(&set, SIGIO);

		sigprocmask(SIG_BLOCK, &set, &oldset);
		signal(SIGIO, catch);

		fcntl(dirfd, F_NOTIFY, DN_CREATE);

		lseek(dirfd, 0L, 0);
		dir = fdopendir(dup(dirfd));
		while ((de = readdir(dir)) != NULL) {
			if (strcmp(de->d_name, "immediate") == 0)
				found_immediate = 1;
			if (strcmp(de->d_name, "request") == 0)
				found_request = 1;
		}

		if (!found_request && !found_immediate)
			sigsuspend(&oldset);
		closedir(dir);
		signal(SIGIO, SIG_DFL);
		sigprocmask(SIG_UNBLOCK, &set, &oldset);
	} while (!found_immediate && !found_request);
}

static int request_valid()
{
	/* check if the request to suspend is still valid.
	 * If the 'immediate' file is not locked, we remove
	 * and ignore it as the requesting process has died
	 */
	int fd = open("/run/suspend/immediate", O_RDWR);
	if (fd >= 0) {
		if (flock(fd, LOCK_EX|LOCK_NB) == 0) {
			/* we got the lock, so owner must have died */
			unlink("/run/suspend/immediate");
			close(fd);
		} else {
			/* Still valid */
			close(fd);
			return 1;
		}
	}
	fd = open("/run/suspend/request", O_RDONLY);
	if (fd < 0)
		return 0;
	close(fd);
	return 1;
}

static void do_suspend(void)
{
	int fd = open("/sys/power/state", O_RDWR);
	if (fd >= 0) {
		write(fd, "mem\n", 4);
		close(fd);
	} else
		sleep(5);
}

main(int argc, char *argv)
{
	int dir;
	int disable;

	mkdir("/run/suspend", 0770);

	dir = open("/run/suspend", O_RDONLY);
	disable = open("/run/suspend/disabled", O_RDWR|O_CREAT, 0640);

	if (dir < 0 || disable < 0)
		exit(1);

	/* Create the initial files */
	alert_watchers();
	cycle_watchers();

	close(0);

	while (1) {
		int count;
		struct timespec ts;
		struct stat stb;

		/* Don't accept an old request */
		unlink("/run/suspend/request");
		wait_request(dir);
		if (flock(disable, LOCK_EX|LOCK_NB) != 0) {
			flock(disable, LOCK_EX);
			flock(disable, LOCK_UN);
			unlink("/run/suspend/request");
			/* blocked - so need to ensure request still valid */
			continue;
		}
		flock(disable, LOCK_UN);;
		/* we got that without blocking but are not holding it */

		/* Next two might block, but that doesn't abort suspend */
		count = read_wakeup_count();
		fstat(disable, &stb);
		ts = stb.st_atim;
		alert_watchers();

		fstat(disable, &stb);
		if (flock(disable, LOCK_EX|LOCK_NB) == 0
		    && request_valid()
		    && ts.tv_sec == stb.st_atim.tv_sec
		    && ts.tv_nsec == stb.st_atim.tv_nsec
		    && set_wakeup_count(count))
			do_suspend();
		flock(disable, LOCK_UN);
		cycle_watchers();
	}
}
