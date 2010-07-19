/***************************************************************************

	watch.c

	(c) 2000-2009 Benoit Minisini <gambas@users.sourceforge.net>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

***************************************************************************/

#define __GBX_WATCH_C

#include "gb_common.h"
#include "gb_error.h"

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "gb_array.h"
#include "gbx_exec.h"
#include "gbx_event.h"
#include "gbx_date.h"
#include "gbx_c_timer.h"
#include "gbx_watch.h"


//#define DEBUG_TIMER
//#define DEBUG_WATCH

static fd_set read_fd;
static fd_set write_fd;

static WATCH_CALLBACK *watch_callback = NULL;
static int max_fd = 0;

static WATCH_TIMER *_timers = NULL;

static int _do_not_really_delete_callback = 0;
static bool _must_delete_callback = FALSE;

#ifdef DEBUG_TIMER
double _debug_time;
static double time_to_double(const struct timeval *t)
{
	return (double)t->tv_sec + t->tv_usec / 1E6 - _debug_time;
}
#endif

static void time_add(struct timeval *t1, const struct timeval *t2)
{
	t1->tv_sec += t2->tv_sec;
	t1->tv_usec += t2->tv_usec;
	if (t1->tv_usec > 1000000)
	{
		t1->tv_usec -= 1000000;
		t1->tv_sec++;
	}
}

static void time_sub(struct timeval *t1, const struct timeval *t2)
{
	t1->tv_sec -= t2->tv_sec;
	t1->tv_usec -= t2->tv_usec;
	if (t1->tv_usec < 0)
	{
		t1->tv_usec += 1000000;
		t1->tv_sec--;
	}
}

#define time_comp(t1, t2, op) ((t1)->tv_sec op (t2)->tv_sec || ((t1)->tv_sec == (t2)->tv_sec && (t1)->tv_usec op (t2)->tv_usec))
#define time_lower_than(t1, t2) time_comp(t1, t2, <)

static void time_from_ms(struct timeval *t, int ms)
{
	t->tv_sec = ms / 1000;
	t->tv_usec = (ms % 1000) * 1000;
}

static struct timeval *time_now(void)
{
	static struct timeval current;

	gettimeofday(&current, NULL);
	if (current.tv_usec < 0 || current.tv_usec >= 1000000)
		fprintf(stderr, "gettimeofday: tv_usec = %ld!\n", current.tv_usec);
	return &current;
}


static void add_timer(GB_TIMER *timer, const struct timeval *timeout)
{
	int i;
	WATCH_TIMER *wt;

	for (i = 0; i < ARRAY_count(_timers); i++)
	{
		//if (_timers[i].timeout > timeout)
		if (time_lower_than(timeout, &_timers[i].timeout))
			break;
	}

	wt = ARRAY_insert(&_timers, i);
	wt->timer = timer;
	wt->timeout = *timeout;

	#ifdef DEBUG_TIMER
	fprintf(stderr, "add_timer: %p at %i: %g\n", timer, i, time_to_double(timeout));
	#endif
}


static void remove_timer(GB_TIMER *timer)
{
	int i;

	for (i = 0; i < ARRAY_count(_timers); i++)
	{
		if (_timers[i].timer == timer)
		{
			ARRAY_remove(&_timers, i)
			#ifdef DEBUG_TIMER
			fprintf(stderr, "remove_timer: %p at %i\n", timer, i);
			#endif
			break;
		}
	}
}

static void raise_timers()
{
	struct timeval timeout;
	struct timeval delay;
	struct timeval *now;
	GB_TIMER *timer;

	now = time_now();

	while (ARRAY_count(_timers))
	{
		timeout = _timers[0].timeout;
		if (time_lower_than(now, &timeout))
			return;

		timer = _timers[0].timer;

		#ifdef DEBUG_TIMER
		fprintf(stderr, "raise_timers: %p has been triggered! now = %.7f timeout = %.7f\n", timer, time_to_double(now), time_to_double(&timeout));
		#endif

		time_from_ms(&delay, timer->delay);
		time_add(&timeout, &delay);

		if (time_lower_than(&timeout, now))
		{
			timeout = *now;
			time_add(&timeout, &delay);
		}

		remove_timer(timer);
		add_timer(timer, &timeout);

		CTIMER_raise(timer);
	}
}


static bool get_timeout(const struct timeval *wait, struct timeval *tv)
{
	struct timeval timeout;
	struct timeval *now;

	if (ARRAY_count(_timers) == 0 && !wait)
		return TRUE;

	now = time_now();

	#ifdef DEBUG_TIMER
	fprintf(stderr, "get_timeout: now = %.7g timeout = %.7g\n", time_to_double(now), time_to_double(&_timers[0].timeout));
	#endif

	if (ARRAY_count(_timers) == 0)
		timeout = *wait;
	else
	{
		timeout = _timers[0].timeout;
		if (wait && time_lower_than(wait, &timeout))
			timeout = *wait;
	}

	time_sub(&timeout, now);

	// HZ = 100 on Linux. If a timer must be triggered in less than 10 ms, then
	// we do a busy wait instead of calling the system.

	if (timeout.tv_sec == 0 && timeout.tv_usec < (1000000 / 100))
	{
		// busy loop!
		time_add(&timeout, now);
		for(;;)
		{
			now = time_now();
			if (!time_lower_than(now, &timeout))
				break;
		}
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
	}
	else if (timeout.tv_sec < 0)
	{
		#ifdef DEBUG_TIMER
		fprintf(stderr, "timeout < 0: %ld %ld\n", timeout.tv_sec, timeout.tv_usec);
		#endif
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
	}

	*tv = timeout;

	#ifdef DEBUG_TIMER
	fprintf(stderr, "timeout: %.7f %ld %ld\n", timeout.tv_sec + timeout.tv_usec / 1E6, tv->tv_sec, tv->tv_usec);
	#endif
	return FALSE;
}


void WATCH_init(void)
{
	FD_ZERO(&read_fd);
	FD_ZERO(&write_fd);

	ARRAY_create(&watch_callback);
	ARRAY_create(&_timers);

	#ifdef DEBUG_TIMER
	DATE_timer(&_debug_time, FALSE);
	#endif
}


void WATCH_exit(void)
{
	ARRAY_delete(&watch_callback);
	ARRAY_delete(&_timers);
}


static void watch_fd(int fd, int flag, bool watch)
{
	#if DEBUG_WATCH
	fprintf(stderr, "watch_fd: %d / %d: %d\n", fd, flag, watch);
	#endif

	if (flag == WATCH_READ)
	{
		if (watch)
			FD_SET(fd, &read_fd);
		else
			FD_CLR(fd, &read_fd);
	}
	else if (flag == WATCH_WRITE)
	{
		if (watch)
			FD_SET(fd, &write_fd);
		else
			FD_CLR(fd, &write_fd);
	}
}


static int watch_find_callback(int fd)
{
	int i;

	for (i = 0; i < ARRAY_count(watch_callback); i++)
	{
		if (fd == watch_callback[i].fd)
			return i;
	}

	return (-1);
}


static int find_max_fd(void)
{
	int i;
	int max = -1;

	for (i = 0; i < ARRAY_count(watch_callback); i++)
	{
		if (watch_callback[i].fd > max)
			max = watch_callback[i].fd;
	}

	return max;
}


static WATCH_CALLBACK *watch_create_callback(int fd)
{
	int pos;
	WATCH_CALLBACK *wcb;

	#if DEBUG_WATCH
	fprintf(stderr, "watch_create_callback: %d\n", fd);
	#endif

	pos = watch_find_callback(fd);
	if (pos < 0)
	{
		wcb = ARRAY_add_void(&watch_callback);
		wcb->fd = fd;
	}
	else
		wcb = &watch_callback[pos];

	if (fd > max_fd)
		max_fd = fd;
		
	return wcb;
}


static void watch_delete_callback(int fd)
{
	int pos;
	
	pos = watch_find_callback(fd);
	if (pos < 0)
		return;
		
	watch_callback[pos].fd = -1;
	watch_fd(fd, WATCH_READ, FALSE);
	watch_fd(fd, WATCH_WRITE, FALSE);
	max_fd = find_max_fd();
	
	if (_do_not_really_delete_callback)
	{
		_must_delete_callback = TRUE;
		return;
	}
		
	ARRAY_remove(&watch_callback, pos);
}


void WATCH_watch(int fd, int type, void *callback, intptr_t param)
{
	WATCH_CALLBACK *wcb;

	if (fd < 0 || fd > FD_SETSIZE)
	{
		fprintf(stderr, "WARNING: trying to watch fd %d\n", fd);
		return;
	}

	if (type == WATCH_NONE)
		watch_delete_callback(fd);
	else
	{
		wcb = watch_create_callback(fd);
		if (type == WATCH_READ)
		{
			wcb->callback_read = callback;
			wcb->param_read = param;
		}
		else
		{
			wcb->callback_write = callback;
			wcb->param_write = param;
		}
		if (!wcb->callback_read && !wcb->callback_write)
			watch_delete_callback(fd);
		else
		{
			watch_fd(fd, WATCH_READ, wcb->callback_read != NULL);
			watch_fd(fd, WATCH_WRITE, wcb->callback_write != NULL);
		}
		
		#if DEBUG_WATCH
		fprintf(stderr, "add watch: %d\n",  watch_find_callback(fd));
		#endif
	}
}


static void raise_callback(fd_set *rfd, fd_set *wfd)
{
	int i;
	WATCH_CALLBACK wcb;

	_do_not_really_delete_callback++;

	#if DEBUG_WATCH
	fprintf(stderr, "\nmax_fd = %d\n", max_fd);
	#endif
	for (i = 0; i < ARRAY_count(watch_callback); i++)
	{
		// We copy the callback structure, because the watch_callback array can change during the
		// execution of the callbacks.
		
		wcb = watch_callback[i];
		if (wcb.fd < 0)
			continue;
		
		if (FD_ISSET(wcb.fd, rfd))
		{
			if (wcb.callback_read)
				(*(wcb.callback_read))(wcb.fd, WATCH_READ, wcb.param_read);
		}

		if (FD_ISSET(wcb.fd, wfd))
		{
			if (wcb.callback_write)
				(*(wcb.callback_write))(wcb.fd, WATCH_WRITE, wcb.param_write);
		}
	}

	_do_not_really_delete_callback--;
	
	if (!_do_not_really_delete_callback && _must_delete_callback)
	{
		i = 0;
		while (i < ARRAY_count(watch_callback))
		{
			if (watch_callback[i].fd < 0)
			{
				ARRAY_remove(&watch_callback, i);
			}
			else
				i++;
		}		
		_must_delete_callback = FALSE;
	}
}


static int do_select(fd_set *rfd, fd_set *wfd, struct timeval *timeout)
{
	int fd;

	for (fd = max_fd; fd >= 0; fd--)
	{
		if (FD_ISSET(fd, &read_fd) || FD_ISSET(fd, &write_fd))
			break;
	}

	if (fd < 0 && !timeout)
		return 0;

	max_fd = fd;

	*rfd = read_fd;
	*wfd = write_fd;

	return select(max_fd + 1, rfd, wfd, NULL, timeout);
}

static bool do_loop(struct timeval *wait)
{
	int ret;
	struct timeval tv;
	fd_set rfd, wfd;
	bool something_done = FALSE;

	if (EVENT_check_post())
		something_done = TRUE;

	if (get_timeout(wait, &tv))
		ret = do_select(&rfd, &wfd, NULL);
	else
	{
		ret = do_select(&rfd, &wfd, &tv);
		something_done = TRUE;
	}

	#ifdef DEBUG_TIMER
	fprintf(stderr, "do_loop: now = %.7g\n", time_to_double(time_now()));
	#endif

	raise_timers();

	if (ret > 0)
	{
		raise_callback(&rfd, &wfd);
		something_done = TRUE;
	}
	else if (ret < 0)
	{
		if (errno != EINTR)
			THROW_SYSTEM(errno, NULL);
		something_done = TRUE;
	}

	//if (EVENT_check_post())
	//	something_done = TRUE;

	return something_done;
}


bool WATCH_one_loop(int wait)
{
	struct timeval timeout;
	
	if (wait == 0)
		return do_loop(NULL);
	else
	{
		time_from_ms(&timeout, wait);
		return do_loop(&timeout);
	}
}

void WATCH_loop(void)
{
	while (do_loop(NULL));
}


void WATCH_wait(int wait)
{
	struct timeval *now;
	struct timeval timeout;

	if (wait == 0)
	{
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		do_loop(&timeout);
	}
	else
	{
		now = time_now();
		time_from_ms(&timeout, wait);
		time_add(&timeout, now);

		for(;;)
		{
			now = time_now();
			if (time_lower_than(&timeout, now))
				break;
			do_loop(&timeout);
		}
	}
}

int WATCH_process(int fd_end, int fd_output)
{
	fd_set rfd;
	int ret, fd_max;

	fd_max = fd_end > fd_output ? fd_end : fd_output;

	for(;;)
	{
		FD_ZERO(&rfd);
		FD_SET(fd_end, &rfd);
		if (fd_output >= 0)
			FD_SET(fd_output, &rfd);

		ret = select(fd_max + 1, &rfd, NULL, NULL, NULL);

		if (ret > 0)
			break;
		if (errno != EINTR)
			break;
	}

	if (FD_ISSET(fd_end, &rfd))
		return fd_end;
	else if (FD_ISSET(fd_output, &rfd))
		return fd_output;
	else
		return -1;
}


void WATCH_timer(void *t, int on)
{
	GB_TIMER *timer = (GB_TIMER *)t;
	struct timeval timeout;

	if (on)
	{
		time_from_ms(&timeout, timer->delay);
		time_add(&timeout, time_now());
		add_timer(timer, &timeout);
		timer->id = (intptr_t)timer;
	}
	else
	{
		remove_timer(timer);
		timer->id = 0;
	}
}
