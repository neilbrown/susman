
/* Test watching for suspend notifications
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
#include "libsus.h"

static int suspend(void *m)
{
	char *msg = m;
	printf("Suspend: %s\n", msg);
	printf("Hang on while I tie my shoe laces...\n");
	sleep(3);
	printf("... OK, Done\n");
	return 1;
}

static void resume(void *m)
{
	char *msg = m;
	printf("Resume: %s\n", msg);
}


main(int argc, char *argv[])
{

	event_init();

	suspend_watch(suspend, resume, "I saw that");

	event_loop(0);
	exit(0);
}
