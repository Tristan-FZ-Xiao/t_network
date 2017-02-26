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
	   // ÉèÖÃ recursive ÊôÐÔ
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

typedef struct {
	int fd;
	struct sockaddr_in src_addr;
	struct list_head entry;
} cli_entry;

typedef struct {
	struct list_head free_list;
	struct list_head inuse_list;
	int inuse, free;
} cli_manage;

typedef struct {
	struct sockaddr_in srv_addr;
	int stat_sendbuf;
	int stat_speed;
	int last_time;
	int duration;
	cli_manage cli;
	char *send_buf;
} cli_info;

cli_info concurrent_cli_info;

void cli_list_init(cli_manage *cli)
{
	INIT_LIST_HEAD(&cli->free_list);
	INIT_LIST_HEAD(&cli->inuse_list);
}

cli_entry *get_cli(cli_manage *cli)
{
	cli_entry *ptr = NULL;
	
	if (list_empty(&cli->free_list)) {
		ptr = (cli_entry *)malloc(sizeof(cli_entry));		
		if (ptr)
			memset(ptr, 0, sizeof(cli_entry));
		else
			return NULL;
	}
	else {
		ptr = list_entry(cli->free_list.next, typeof(cli_entry), entry);
		list_del(&ptr->entry);
		cli->free--;
	}
	list_add_tail(&ptr->entry, &cli->inuse_list);
	cli->inuse++;
	return ptr;
}

void free_cli(cli_entry *ptr, cli_manage *cli)
{
	close(ptr->fd);
	list_del(&ptr->entry);
	cli->inuse--;
	memset(ptr, 0, sizeof(cli_entry));
	list_add(&ptr->entry, &cli->free_list);
	cli->free++;
}

void cli_print(cli_manage *cli)
{
	cli_entry *pos = NULL;
	int i = 0;
	
	printf("Client Info\n");
	printf("Free\t%d\n", cli->free);
	printf("No.\t\tfd\t\tfrom_addr\n");
	list_for_each_entry(pos, &cli->free_list, entry) {
		printf("%d\t\t%d\t\t%s:%d\n", i++, pos->fd,
		       inet_ntoa(pos->src_addr.sin_addr), ntohs(pos->src_addr.sin_port));
	}
	printf("Inuse\t%d\n", cli->inuse);
	printf("No.\t\tfd\t\tfrom_addr\n");
	list_for_each_entry(pos, &cli->inuse_list, entry) {
		printf("%d\t\t%d\t\t%s:%d\n", i++, pos->fd,
		       inet_ntoa(pos->src_addr.sin_addr), ntohs(pos->src_addr.sin_port));
	}
	fflush(stdout);
}

void buffer_padding(char *buf, int len)
{
	memset(buf, 0x30, len);
}

#define MAX_CLI_CONN	20
#define SEND_BUF_SIZE	102400

void concurrent_cli_init(cli_info *cli)
{
	memset(cli, 0, sizeof(cli_info));
	cli_list_init(&cli->cli);
	inet_aton("127.0.0.1", &cli->srv_addr.sin_addr);
	cli->srv_addr.sin_family = AF_INET;
	cli->srv_addr.sin_port = htons(40000);
	cli->duration = 100 * 1000;	/* ms */
	cli->send_buf = malloc(SEND_BUF_SIZE);
	buffer_padding(cli->send_buf, SEND_BUF_SIZE);
}

void cli_loop(cli_info *cli)
{
	int seed = 50000;
	cli_entry *ptr = get_cli(&cli->cli);

	ptr->fd = Socket(AF_INET, SOCK_STREAM, 0);
	ptr->src_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	srand(seed);
	ptr->src_addr.sin_port = htons(rand_r(&seed));
	connect_nonb(ptr->fd, (const struct sockaddr *)&cli->srv_addr, sizeof(struct sockaddr_in), 5);
	while (1) {
		send_tcp(ptr->fd, cli->send_buf, SEND_BUF_SIZE);
	}	
	return;	
}

void cli_thread_init(cli_info *cli)
{
	int i = 0;
	for (; i < MAX_CLI_CONN; i++) {
		static pthread_t detect_pid;

		pthread_create(&detect_pid, NULL, cli_loop, cli);
		pthread_detach(detect_pid);
	}
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
	concurrent_cli_init(&concurrent_cli_info);
	cli_thread_init(&concurrent_cli_info);
	while (1) {
		sleep(1);
	}
#endif

}
