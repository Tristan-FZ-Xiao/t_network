#include <pthread.h>

#include "t_list.h"
#include "t_unp.h"
#include "t_utils.h"

#ifdef THREAD_PARAM_RETURN
void debug_thread_loop1(void *a)
{
	int ret = *((int *)a) * 2;

	printf("ret %d a %d\n", ret, *(int *)a);
	*(int *)a = ret;
	pthread_exit((void *)a);
}

pthread_t debug_thread1(int *input)
{
	static pthread_t detect_pid;

	pthread_create(&detect_pid, NULL, debug_thread_loop1, input);
	return detect_pid;
}

/* Unit test of verification return parameters from thread */
void ut_thread_ret_value(void)
{	
	int input = 5;
	int *p = NULL;

	pthread_t p1 = debug_thread1(&input);
	pthread_join(p1, (void **)&p);
	printf("p = %x %d\n", p, *p);
}
#endif	/* THREAD_PARAM_RETURN */

//#define THREAD_MUTEX_VERIFICATION	1

#ifdef THREAD_MUTEX_VERIFICATION

pthread_mutexattr_t attr;
pthread_mutex_t mutex;
int i = 0;

void product_loop(void)
{
	while (1)
	{
		pthread_mutex_lock(&mutex);	
		if (i == 0)
		{
			i = 1;
			printf("producter turn\n");
		}
		pthread_mutex_unlock(&mutex);
	}
}

void comsumer_loop(void)
{
	while (1)
	{
		pthread_mutex_lock(&mutex);	
		if (i == 1)
		{
			i = 0;
			printf("comsumer turn\n");
		}
		pthread_mutex_unlock(&mutex);
	}
}

void ut_thread_mutex(void)
{
	pthread_t product_id;
	pthread_t comsumer_id;

	pthread_mutexattr_init(&attr); 
	   // …Ë÷√ recursive  Ù–‘
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP); 
	pthread_mutex_init(&mutex, &attr); 
	
	pthread_create(&product_id, NULL, product_loop, NULL);
	pthread_detach(product_id);
	pthread_create(&comsumer_id, NULL, comsumer_loop, NULL);
	pthread_detach(comsumer_id);
	while (1)
	{
		sleep(1);	
	}

	return;
}
#endif /* THREAD_MUTEX_VERIFICATION */

#define NETWORK_LIB
#ifdef NETWORK_LIB

#define MAX_CLI_CONN	1024

typedef struct {
	int fd;
	struct sockaddr_in src_addr;
	int used;
} cli_entry;

typedef struct {
	cli_entry cli_list[MAX_CLI_CONN];
	int cnt, free;		/* cnt: current used count;
				 */
} cli_head;

cli_head cli_list;


int cli_count = MAX_CLI_CONN;
const char server_addr[] = "192.168.11.230";
const int server_port = 40000;
int duration = 100 * 1000; /* test time: ms */
#define SEND_BUF_SIZE	102400


void buffer_pad(char *buf, int len)
{
	memset(buf, 0x30, len);
}

cli_entry *get_free_cli_entry(void)
{
	if (cli_list.cnt < MAX_CLI_CONN)
	{
		return &cli_list.cli_list[cli_list.cnt ++];
	}
	return NULL;
}

void cli_loop(void)
{
	int i = 0;
	int seed = 50000;
	char send_buf[SEND_BUF_SIZE] = {'\0'};
	struct sockaddr_in srv_addr;
	cli_entry *ptr = get_free_cli_entry();
	int start_time, item_time, cur_time;
	int stat_sendbuf = 0;
	
	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;	
	inet_aton(server_addr, &srv_addr.sin_addr);
	srv_addr.sin_port = htons(server_port);

	ptr->fd = Socket(AF_INET, SOCK_STREAM, 0);
	//set_non_block(ptr->fd);
	ptr->src_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	srand(seed);
	ptr->src_addr.sin_port = htons(rand_r(&seed));
	connect_nonb(ptr->fd, (const struct sockaddr *)&srv_addr, sizeof(srv_addr), 5);
	//snprintf(send_buf, "hello I'm the No.%04d client\n", i);
	buffer_pad(send_buf, SEND_BUF_SIZE);
	
	start_time = item_time = get_now_time();
	do {
		Send(ptr->fd, send_buf, SEND_BUF_SIZE, 0);
		stat_sendbuf += SEND_BUF_SIZE;
		cur_time = get_now_time();

		if ((cur_time - item_time) > 1000) {
			printf("(Time: %d ms - %d ms) Send Speed %d KB\n", item_time, cur_time, stat_sendbuf / (cur_time - item_time));
			item_time = cur_time;
			stat_sendbuf = 0;
		}
	} while ((cur_time - start_time) < duration);
	return;	
}
	
#endif /* NETWORK_LIB */


int main(int argc, char *argv[])
{
#ifdef THREAD_PARAM_RETURN
	ut_thread_ret_value();
#endif	/* THREAD_PARAM_RETURN */

#ifdef THREAD_MUTEX_VERIFICATION
	ut_thread_mutex();
#endif /* THREAD_MUTEX_VERIFICATION */

#ifdef NETWORK_LIB
	cli_loop();	
#endif

}
