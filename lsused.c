
/*
 * lsused - Linux SUSpend Event monitoring Daemon
 *
 * apps and services can send fds to this daemon.
 * We will not allow suspend to happen if any fd is readable.
 * A client should also notice that it is readable, take a
 * shared lock on the suspend/disabled file and then read the event.
 *
 * The client opens connects on a unix domain socket to
 * /var/run/suspend/registration
 * It sets 'W' with some fds attached to be watched.
 * On notification if any fds are readable we send by 'S' to say
 * Suspend Soon and wait for 'R' to say 'Ready'.
 * We don't bother checking the fds again until the next suspend
 * attempt.
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
#include <event.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include "libsus.h"


struct handle {
	struct event	ev;
	int		sent;		/* 'S' has been sent */
	int		suspending;	/* ... 'R' hasn't been received yet */
	struct handle	*next;
	struct state	*state;
};

struct state {
	int		waiting;	/* Number of replies waiting for */
	struct handle	*handles;	/* linked list of handles */
	struct pollfd	*fds;		/* for 'poll' */
	struct handle	**hans;		/* aligned with fds */
	int		nfds;		/* number of active 'fds' */
	int		fdsize;		/* allocated size of fds array */
	void		*sus;		/* handle from suspend_watch */
};

static void del_fd(struct state *state, int i)
{
	state->fds[i] = state->fds[state->nfds - 1];
	state->hans[i] = state->hans[state->nfds - 1];
	state->nfds--;
}

static void add_fd(struct state *state, struct handle *han,
		   int fd, short events)
{
	int n = state->nfds;
	if (state->nfds >= state->fdsize) {
		/*need to make bigger */
		int need = 16;
		while (need < n+1)
			need *= 2;
		state->fds = realloc(state->fds,
				     need * sizeof(struct pollfd));
		state->hans = realloc(state->hans,
				      need * sizeof(struct handle *));
	}
	state->hans[n] = han;
	state->fds[n].fd = fd;
	state->fds[n].events = events;
	state->nfds++;
}

static void add_han(struct handle *han, struct state *state)
{

	han->next = state->handles;
	state->handles = han;
}

static void del_han(struct handle *han)
{
	struct state *state = han->state;
	struct handle **hanp = &state->handles;
	struct handle *h;
	int i;

	/* First remove the fds; */
	for (i = 0; i < state->nfds ; i++)
		if (state->hans[i] == han) {
			del_fd(state, i);
			i--;
		}

	/* Then remove the han */
	for (h = *hanp; h; hanp = &h->next, h = *hanp) {
		if (h == han) {
			*hanp = h->next;
			break;
		}
	}
}

static void do_read(int fd, short ev, void *data)
{
	struct handle *han = data;
	char buf;
	struct msghdr msg;
	struct cmsghdr *cm;
	struct iovec iov;
	char mbuf[100];

	buf = 0;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	iov.iov_base = &buf;
	iov.iov_len = 1;
	msg.msg_control = mbuf;
	msg.msg_controllen = sizeof(mbuf);
	msg.msg_flags = 0;

	if (recvmsg(fd, &msg, MSG_CMSG_CLOEXEC|MSG_DONTWAIT) < 0
	    && errno == EAGAIN)
		return;

	switch (buf) {
	case 'W':
		for (cm = CMSG_FIRSTHDR(&msg);
		     cm != NULL;
		     cm = CMSG_NXTHDR(&msg, cm))
			if (cm->cmsg_level == SOL_SOCKET &&
			    cm->cmsg_type == SCM_RIGHTS) {
				int *fdptr = (int*)CMSG_DATA(cm);
				int n = (cm->cmsg_len -
					 CMSG_ALIGN(sizeof(struct cmsghdr)))
					/ sizeof(int);
				int i;
				for (i = 0; i < n; i++)
					add_fd(han->state, han, fdptr[i],
						POLLIN|POLLPRI);
			}
		write(fd, "A", 1);
		break;

	case 'R':
		if (han->suspending) {
			han->suspending = 0;
			han->state->waiting--;
			if (han->state->waiting == 0)
				suspend_ok(han->state->sus);
		}
		break;

	default:
		event_del(&han->ev);
		del_han(han);
		close(fd);
	}
}

static void do_accept(int fd, short ev, void *data)
{
	struct state *state = data;
	struct handle *han;
	int newfd = accept4(fd, NULL, NULL, SOCK_NONBLOCK|SOCK_CLOEXEC);
	if (newfd < 0)
		return;

	han = malloc(sizeof(*han));
	if (!han) {
		close(newfd);
		return;
	}
	han->sent = 0;
	han->suspending = 0;
	han->state = state;
	add_han(han, state);
	event_set(&han->ev, newfd, EV_READ | EV_PERSIST, do_read, han);
	event_add(&han->ev, NULL);
	write(newfd, "A", 1);
}

static int do_suspend(void *data)
{
	struct state *state = data;
	struct handle *han;
	int n;
	int i;

	n = poll(state->fds, state->nfds, 0);
	if (n == 0)
		/* nothing happening */
		return 1;
	for (han = state->handles ; han ; han = han->next)
		han->sent = 0;
	state->waiting = 1;
	for (i = 0; i < state->nfds; i++)
		if (state->fds[i].revents) {
			han = state->hans[i];
			if (!han->sent) {
				han->sent = 1;
				han->suspending = 1;
				write(EVENT_FD(&han->ev), "S", 1);
				state->waiting++;
			}
		}
	state->waiting--;
	return (state->waiting == 0);
}

static void did_resume(void *data)
{
	struct state *state = data;
	struct handle *han;

	for (han = state->handles ; han ; han = han->next)
		if (han->sent)
			write(EVENT_FD(&han->ev), "A", 1);
}

main(int argc, char *argv[])
{
	struct sockaddr_un addr;
	struct state state;
	struct event ev;
	int s;

	memset(&state, 0, sizeof(state));

	s = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/var/run/suspend/registration");
	unlink("/var/run/suspend/registration");
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		exit(1);
	listen(s, 20);

	event_init();

	state.sus = suspend_watch(do_suspend, did_resume, &state);
	event_set(&ev, s, EV_READ | EV_PERSIST, do_accept, &state);
	event_add(&ev, NULL);

	event_loop(0);
	exit(0);
}
