
/*
 * Library code to allow libevent app to register for a wake alarm
 * and register with wakealarmd to keep suspend at bay for the time.
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
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <event.h>
#include <fcntl.h>
#include <errno.h>
#include "libsus.h"

struct han {
	struct event	ev;
	int		sock;
	int		disable;
	void		(*fn)(int,short,void*);
	void		*data;
};

static void alarm_clock(int fd, short ev, void *data)
{
	char buf[20];
	int n;
	struct han *h = data;

	n = read(fd, buf, sizeof(buf)-1);
	if (n < 0 && errno == EAGAIN)
		return;
	if (n <= 0 ||
	    strncmp(buf, "Now", 3) == 0) {
		h->fn(-1, ev, h->data);
		wakealarm_destroy(&h->ev);
	}
	/* Some other message, keep waiting */
}

struct event *wakealarm_set(time_t when, void(*fn)(int, short, void*),
			    void *data)
{
	struct sockaddr_un addr;
	struct han *h = malloc(sizeof(*h));
	char buf[20];

	if (!h)
		return NULL;

	h->fn = fn;
	h->data = data;
	h->disable = suspend_open();
	h->sock = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (h->sock < 0 || h->disable < 0)
		goto abort;

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/var/run/suspend/wakealarm");
	if (connect(h->sock, (struct sockaddr*)&addr, sizeof(addr)) != 0)
		goto abort;

	fcntl(h->sock, F_SETFL, fcntl(h->sock, F_GETFL, 0) | O_NONBLOCK);
	sprintf(buf, "%lld\n", (long long)when);
	write(h->sock, buf, strlen(buf));

	event_set(&h->ev, h->sock, EV_READ|EV_PERSIST, alarm_clock, h);
	event_add(&h->ev, NULL);

	return &h->ev;

abort:
	suspend_close(h->disable);
	if (h->sock >= 0)
		close(h->sock);
	free(h);
	return NULL;
}

void wakealarm_destroy(struct event *ev)
{
	struct han *h = (struct han *)ev;
	event_del(&h->ev);
	close(h->sock);
	suspend_close(h->disable);
	free(h);
}
