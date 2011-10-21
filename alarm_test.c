

/* Test wakealarm.
 * argv is a number of seconds to wait for.
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
#include <stdio.h>
#include "libsus.h"

void callback(int fd, short ev, void *data)
{
	printf("Ping - got the event\n");
	suspend_block(-1);
	event_loopbreak();
}

int main(int argc, char *argv[])
{
	time_t now;
	time_t then;
	int diff;
	struct event *ev;

	if (argc != 2) {
		fprintf(stderr, "Usage: alarm_test seconds\n");
		exit(1);
	}
	diff = atoi(argv[1]);
	time(&now);
	then = now + diff;
	event_init();

	ev = wakealarm_set(then, callback, NULL);

	event_loop(0);
	printf("Hold off suspend for a while...\n");
	sleep(4);
	printf("OK - done\n");
	exit(0);
}
