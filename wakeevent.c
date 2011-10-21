
/* Register an fd which produces wake events with
 * eventlib.
 * Whenever the fd is readable, we block suspend,
 * call the handler, then allow suspend.
 * Meanwhile we open a socket to the event daemon passing
 * it the same fd.
 * At a lower priority, when we read 'S' from the daemon we reply
 * with 'R'.
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
#include <event.h>
#include <fcntl.h>
#include <errno.h>
#include "libsus.h"

struct han {
	struct event	ev;
	struct event	sev;
	int		sock;
	int		disable;
	void		(*fn)(int,short,void*);
	void		*data;
};

static void wakeup_call(int fd, short ev, void *data)
{
	/* A (potential) wakeup event can be read from this fd.
	 * We won't go to sleep because we haven't replied to
	 * 'S' yet as that is handle with a lower priority.
	 */
	struct han *han = data;
	han->fn(fd, ev, han->data);
}

static void wakeup_sock(int fd, short ev, void *data)
{
	char buf;
	struct han *han = data;
	int n = read(fd, &buf, 1);

	if (n < 0 && errno == EAGAIN)
		return;
	if (n != 1) {
		/* How do I signal an error ?*/
		event_del(&han->sev);
		return;
	}
	if (buf == 'S')
		/* As we are at a lower priority (higher number)
		 * than the main event, we must have handled everything
		 */
		write(fd, "R", 1);
}

static void send_fd(int sock, int fd)
{
	struct msghdr msg = {0};
	struct iovec iov;
	struct cmsghdr *cmsg;
	int myfds[1];
	char buf[CMSG_SPACE(sizeof myfds)];
	int *fdptr;

	msg.msg_control = buf;
	msg.msg_controllen = sizeof buf;
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	fdptr = (int*)CMSG_DATA(cmsg);
	fdptr[0] = fd;
	msg.msg_controllen = cmsg->cmsg_len;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	iov.iov_base = "W";
	iov.iov_len = 1;
	sendmsg(sock, &msg, 0);
}

struct event *wake_set(int fd, void(*fn)(int,short,void*), void *data, int prio)
{
	struct sockaddr_un addr;
	struct han *h = malloc(sizeof(*h));

	if (!h)
		return NULL;

	h->fn = fn;
	h->data = data;
	h->disable = suspend_open();
	h->sock = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (h->sock < 0 || h->disable < 0)
		goto abort;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/var/run/suspend/registration");
	if (connect(h->sock, (struct sockaddr*)&addr, sizeof(addr)) != 0)
		goto abort;

	fcntl(h->sock, F_SETFL, fcntl(h->sock, F_GETFL, 0) | O_NONBLOCK);

	send_fd(h->sock, fd);

	event_set(&h->ev, fd, EV_READ|EV_PERSIST, wakeup_call, h);
	event_set(&h->sev, h->sock, EV_READ|EV_PERSIST, wakeup_sock, h);
	event_priority_set(&h->ev, prio);
	event_priority_set(&h->sev, prio+1);
	event_add(&h->ev, NULL);
	event_add(&h->sev, NULL);

	return &h->ev;

abort:
	suspend_close(h->disable);
	if (h->sock >= 0)
		close(h->sock);
	free(h);
	return NULL;
}

void wake_destroy(struct event *ev)
{
	struct han *h = (struct han *)ev;
	event_del(&h->ev);
	event_del(&h->sev);
	close(h->sock);
	suspend_close(h->disable);
	free(h);
}
