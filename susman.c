/*
 * susman - manage suspend
 * This daemon forks and runs three processes
 * - one which manages suspend based on files in /var/run/suspend
 * - one which listens on a socket and handles suspend requests that way,
 * - one which provides a wakeup service using the RTC alarm.
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

int lsusd(int argc, char *argv[]);
int lsused(int argc, char *argv[]);
int wakealarmd(int argc, char *argv[]);

void runone(int (*fun)(int argc, char *argv[]))
{
	int pfd[2];
	char c;

	if (pipe(pfd) < 0)
		exit(2);
	switch (fork()) {
	case -1:
		exit (2);
	default:
		break;
	case 0:
		close(pfd[0]);
		dup2(pfd[1], 0);
		close(pfd[1]);
		(*fun)(0, NULL);
		exit(1);
	}
	close(pfd[1]);
	/* Block for lsused to start up */
	read(pfd[0], &c, 1);
	close(pfd[0]);
}

int main(int argc, char *argv[])
{
	int pfd[2];
	char c;

	runone(lsusd);
	runone(lsused);
	wakealarmd(0, NULL);
	exit(0);
}
