#ifndef __T_UTILS_H__
#define __T_UTILS_H__

#include <pthread.h>

typedef void(*fn)(void *);

typedef struct {
	fn cb;
	void *arg;
	unsigned int timestamp;
} timer_entry;

typedef struct {
	timer_entry **timer_list;
	int cnt;
	int max;
	int fd;
	fd_set rfds;
	int fdmax;
	pthread_mutex_t lock;
	pthread_mutexattr_t attr;
}timer_base;

unsigned int get_now_time(void);
void add_timer(int timeout, fn callback, void *arg);
void run_timer();
void init_timer(void);

extern timer_base t_timer_base;

#define SWAP(a, b)			\
	do {				\
		timer_entry *tmp = a;	\
		a = b;			\
		b = tmp;		\
	} while(0);


#endif /* __T_UTILS_H__ */
