
/*
 * test handling wakeup events on fd.
 * You will need to strace things and watch to get any real
 * feedback as this only does anything interesting when there
 * is a race.
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

#include <stdlib.h>
#include <event.h>
#include <stdio.h>
#include <fcntl.h>
#include "libsus.h"

void read_event(int fd, short ev, void *data)
{
	char buf[80];
	int n;
	int i;

	printf("Can read now .. give it a moment though\n");
	sleep(5);
	n = read(fd, buf, 80);
	if (n < 0)
		exit(1);

	for (i = 0; i < n ; i++)
		printf(" %02x", buf[i] & 0xff);
	printf("\n");
}

main(int argc, char *argv[])
{
	int fd;
	struct event *ev;

	if (argc != 2) {
		fprintf(stderr, "Usage: event_test devicename\n");
		exit(1);
	}
	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[1]);
		exit(1);
	}

	event_init();
	event_priority_init(3);
	ev = wake_set(fd, read_event, NULL, 1);

	event_loop(0);
	exit(0);
}
