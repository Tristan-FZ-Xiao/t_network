#include <sys/time.h>
#include <stdio.h>

unsigned int get_now_time(void)
{
	struct timeval tv;
	
	gettimeofday(&tv, NULL);
	return (unsigned int)(tv.tv_usec / 1000 + tv.tv_sec * 1000);
}
