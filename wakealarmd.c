
/* wake alarm service.
 * We provide a wakeup service to use the rtc wakealarm
 * to ensure the system is running at given times and to
 * alert clients.
 *
 * Client can connect and register a time as seconds since epoch
 * We echo back the time and then when the time comes we echo "Now".
 * We keep system awake until another time is written, or until
 * connection is closed.
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
#include <event.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include "libsus.h"

struct conn {
	struct event	ev;
	struct event	tev;
	time_t		stamp;	/* When to wake */
	int		active; /* stamp has passed */
	struct conn	*next;	/* sorted by 'stamp' */
	struct state	*state;
};

struct state {
	struct event	ev;
	int		disablefd;
	int		disabled;
	void		*watcher;
	struct conn	*conns;
	int		active_count;
};

static void add_han(struct conn *han)
{
	struct state *state = han->state;
	struct conn **hanp = &state->conns;
	struct timeval tv;
	time_t now;

	while (*hanp &&
	       (*hanp)->stamp < han->stamp)
		hanp = &(*hanp)->next;
	han->next = *hanp;
	*hanp = han;

	time(&now);
	if (now < han->stamp) {
		tv.tv_sec = han->stamp - now;
		tv.tv_usec = 0;
		evtimer_add(&han->tev, &tv);
		han->active = 0;
	} else {
		if (!han->active) {
			han->active = 1;
			state->active_count++;
		}
	}
}

static void del_han(struct conn *han)
{
	struct state *state = han->state;
	struct conn **hanp = &state->conns;

	while (*hanp &&
	       *hanp != han)
		hanp = &(*hanp)->next;
	if (*hanp == han) {
		*hanp = han->next;
		if (han->active) {
			state->active_count--;
			if (state->active_count == 0
			    && state->disabled) {
				suspend_allow(state->disablefd);
				state->disabled = 0;
			}
		}
	}
}

static void destroy_han(struct conn *han)
{
	event_del(&han->ev);
	event_del(&han->tev);
	free(han);
}

static void do_read(int fd, short ev, void *data)
{
	struct conn *han = data;
	char buf[20];
	int n;

	n = read(fd, buf, sizeof(buf)-1);
	if (n < 0 && errno == EAGAIN)
		return;
	if (n <= 0) {
		del_han(han);
		destroy_han(han);
		return;
	}
	del_han(han);
	han->stamp = atol(buf);
	add_han(han);
	sprintf(buf, "%lld\n", (long long)han->stamp);
	write(fd, buf, strlen(buf));
}

static void do_timeout(int fd, short ev, void *data)
{
	struct conn *han = data;

	if (!han->active) {
		han->active = 1;
		han->state->active_count++;
	}
	write(EVENT_FD(&han->ev), "Now\n", 4);
}

static void do_accept(int fd, short ev, void *data)
{
	struct state *state = data;
	struct conn *han;
	int newfd = accept4(fd, NULL, NULL, SOCK_NONBLOCK|SOCK_CLOEXEC);

	if (newfd < 0)
		return;
	han = malloc(sizeof(*han));
	if (!han) {
		close(newfd);
		return;
	}
	han->state = state;
	han->stamp = 0;
	han->active = 1;
	state->active_count++;
	han->next = state->conns;
	state->conns = han;

	evtimer_set(&han->tev, do_timeout, han);
	event_set(&han->ev, newfd, EV_READ | EV_PERSIST, do_read, han);
	event_add(&han->ev, NULL);
	write(newfd, "0\n", 2);
}

static int do_suspend(void *data)
{
	struct state *state = data;
	time_t now;

	time(&now);

	/* active_count must be zero */
	if (state->conns == NULL)
		return 1;

	if (state->conns->stamp > now + 4) {
		int fd = open("/sys/class/rtc/rtc0/wakealarm", O_WRONLY);
		if (fd >= 0) {
			char buf[20];
			sprintf(buf, "%lld\n",
				(long long)state->conns->stamp - 2);
			write(fd, buf, strlen(buf));
			close(fd);
		}
		return 1;
	}
	/* too close to next wakeup */
	if (state->disabled) {
		suspend_block(state->disablefd);
		state->disabled = 1;
	}
	return 1;
}

int main(int argc, char *argv[])
{
	struct state st;
	struct sockaddr_un addr;
	int s;

	st.disablefd = suspend_open();
	st.disabled = 0;
	st.conns = NULL;
	st.active_count = 0;

	s = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/var/run/suspend/wakealarm");
	unlink("/var/run/suspend/wakealarm");
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		exit(2);
	listen(s, 20);

	event_init();
	st.watcher = suspend_watch(do_suspend, NULL, &st);
	event_set(&st.ev, s, EV_READ | EV_PERSIST, do_accept, &st);
	event_add(&st.ev, NULL);

	event_loop(0);
	exit(0);
}
