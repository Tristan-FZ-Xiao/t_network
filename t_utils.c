#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "t_utils.h"

unsigned int get_now_time(void)
{
	struct timeval tv;
	
	gettimeofday(&tv, NULL);
	return (unsigned int)(tv.tv_usec / 1000 + tv.tv_sec * 1000);
}

timer_base t_timer_base;

timer_entry *get_timer_entry(timer_base *base, unsigned int timestamp)
{
	int parent, cnt;

	if (!base) {
		return NULL;
	}

	if (base->max == 0 || base->max <= base->cnt) {
		int i;

		if (base->max == 0) {
			base->max = 16;
		}
		else {
			base->max = base->max << 1;
		}
		base->timer_list = (timer_entry **)realloc(base->timer_list, sizeof(timer_entry *) * base->max);
		for (i = base->cnt; i < base->max; i++) {
			base->timer_list[i] = malloc(sizeof(timer_entry));
			memset(base->timer_list[i], 0, sizeof(timer_entry));
		}
	}
	base->timer_list[base->cnt++]->timestamp = timestamp;
	cnt = base->cnt;
	if (cnt > 1) {
		parent = cnt / 2;
		while (base->timer_list[parent - 1]->timestamp > base->timer_list[cnt - 1]->timestamp) {
			SWAP(base->timer_list[parent - 1], base->timer_list[cnt - 1]);
			cnt = parent;
			parent = cnt / 2;
		}
	}
	return base->timer_list[cnt - 1];
}


static timer_entry *get_min(timer_base *base)
{
	if (base && base->cnt != 0) {
		return base->timer_list[0];
	}
	return NULL;
}

void min_heap(timer_base *base, int cur)
{
	int left, right;
	left = cur * 2;
	right = cur * 2 + 1;
	int min = cur;

	if (!base) {
		return;	
	}
	if (left > base->cnt) {
		return;
	}
	else if (right > base->cnt) {
		fflush(stdout);
		if (base->timer_list[left - 1]->timestamp < base->timer_list[cur - 1]->timestamp) {
			SWAP(base->timer_list[left - 1], base->timer_list[cur - 1]);
		}
		return;
	}

	if (base->timer_list[left - 1]->timestamp < base->timer_list[min - 1]->timestamp)
		min = left;
	if (base->timer_list[right - 1]->timestamp < base->timer_list[min - 1]->timestamp)
		min = right;
	if (min != cur) {
		SWAP(base->timer_list[min - 1], base->timer_list[cur - 1]);
		min_heap(base, min);
	}
}

static void build_min_heap(timer_base *base)
{
	int i;
	if (!base) {
		return;
	}

	for (i = base->cnt / 2; i > 0; i--)
		min_heap(base, i);
}

static void extrac_min(timer_base *base)
{
	if (base && base->cnt != 0) {
		timer_entry *tmp = base->timer_list[0];

		base->timer_list[0] = base->timer_list[base->cnt - 1];
		base->timer_list[base->cnt - 1] = tmp;
		memset(tmp, 0, sizeof(timer_entry));
		base->cnt--;
		min_heap(base, 1);
	}
}

/*
 *	Need to guarantee thread-safe between add_timer() and run_timer() 
 */
void add_timer(int timestamp, fn cb, void *arg)
{
	pthread_mutex_lock(&t_timer_base.lock);
	timer_entry *ptr = get_timer_entry(&t_timer_base, timestamp);

	if (ptr) {
		ptr->cb = cb;
		ptr->arg = arg;
	}
	pthread_mutex_unlock(&t_timer_base.lock);
}

void run_timer(void)
{
	timer_base *base = &t_timer_base;
	int cur_time = 0;	/* In ms */

	base->fd = mkfifo("/tmp/1.txt", O_CREAT | O_RDONLY);	
 
	while (1) {
		cur_time = get_now_time();

		pthread_mutex_lock(&base->lock);
		timer_entry *ptr = get_min(base);
		if (ptr)
		{
			if (cur_time > ptr->timestamp)
			{
				pthread_mutex_unlock(&base->lock);
				if (ptr->cb)
					ptr->cb(ptr->arg);
				pthread_mutex_lock(&base->lock);
				extrac_min(base);
				pthread_mutex_unlock(&base->lock);
			}
			else
			{
				struct timeval tv;
				
				tv.tv_sec = (ptr->timestamp - cur_time) / 1000;
				tv.tv_usec = ((ptr->timestamp - cur_time) - tv.tv_sec * 1000) * 1000;
				pthread_mutex_unlock(&base->lock);

				FD_ZERO(&base->rfds);
				FD_SET(base->fd, &base->rfds);
				base->fdmax > base->fd ? base->fdmax : base->fd + 1; 
				select(base->fdmax, &base->rfds, NULL, NULL, &tv);
				if (FD_ISSET(base->fd, &base->rfds))
				{
					printf("now new add_timer() is arrivalled\n");
				}
			}
		}
		else {
			pthread_mutex_unlock(&base->lock);
		}
	}
}

void init_timer(void)
{
	timer_base *base = &t_timer_base;
	pthread_mutex_init(&base->lock, NULL);
	pthread_mutexattr_init(&base->attr);
}
