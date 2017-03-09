#ifndef __T_UTILS_H__
#define __T_UTILS_H__

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
}timer_base;

unsigned int get_now_time(void);
void add_timer(int timeout, fn callback, void *arg);
void run_timer();

#define SWAP(a, b)			\
	do {				\
		timer_entry *tmp = a;	\
		a = b;			\
		b = tmp;		\
	} while(0);

#endif /* __T_UTILS_H__ */
