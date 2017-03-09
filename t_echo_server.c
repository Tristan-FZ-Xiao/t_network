#include "t_unp.h"
#include "t_utils.h"
#include "t_list.h"

//#define USE_SELECT	1
#define USE_EPOLL	1
typedef struct {
	int fd;
	struct sockaddr_in from_addr;
	struct list_head entry;
} cli_entry;

typedef struct {
	struct list_head free_list;
	struct list_head inuse_list;
	int inuse, free;
} cli_manage;

cli_manage echo_cli_manage;

#define EPOLLEVENTS	10000
#define FDSIZE		1024

typedef struct {
	int listen_port;
	int listen_fd;
	int max_backlog;
	int stat_recvbuf;
	int stat_speed;
	unsigned int last_time;
	cli_manage cli;
	fd_set	rset;

	int epollfd;
	struct	epoll_event events[EPOLLEVENTS];
} server_info;

server_info echo_server_info;

void echo_cli_init(cli_manage *cli)
{
	INIT_LIST_HEAD(&cli->free_list);
	INIT_LIST_HEAD(&cli->inuse_list);
}

cli_entry *new_cli(cli_manage *cli)
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

cli_entry *get_cli_entry(server_info *echo_server, int fd)
{
	cli_entry *pos = NULL;

	list_for_each_entry(pos, &echo_server->cli.inuse_list, entry) {
		if (fd == pos->fd) {
			return pos;
		}
	}
	return pos;
}

void cli_print(cli_manage *cli)
{
	cli_entry *pos = NULL;
	int i = 0;
	
	printf("Client Info\n");
	printf("Free\t%d\n", cli->free);
	printf("No.\t\tfd\t\tfrom_addr\n");
	list_for_each_entry(pos, &cli->free_list, entry) {
		printf("%d\t\t%d\t\t%s:%d\n", i++, pos->fd, inet_ntoa(pos->from_addr.sin_addr), ntohs(pos->from_addr.sin_port));
	}
	printf("Inuse\t%d\n", cli->inuse);
	printf("No.\t\tfd\t\tfrom_addr\n");
	list_for_each_entry(pos, &cli->inuse_list, entry) {
		printf("%d\t\t%d\t\t%s:%d\n", i++, pos->fd, inet_ntoa(pos->from_addr.sin_addr), ntohs(pos->from_addr.sin_port));
	}
	fflush(stdout);
}

void unit_test_list(void)
{
	cli_entry *ptr = NULL;	
	int i = 0;
	
	echo_cli_init(&echo_cli_manage);
	for (; i < 10; i++) {
		ptr = new_cli(&echo_cli_manage);
		ptr->fd = i * 2;
		ptr->from_addr.sin_family = AF_INET;
		ptr->from_addr.sin_port = htons(i * i);
		ptr->from_addr.sin_addr.s_addr = htonl(0xc0a80000 + i + 1);
	}
	cli_print(&echo_cli_manage);
	free_cli(ptr, &echo_cli_manage);
	cli_print(&echo_cli_manage);
}

void t_echo_server_init(server_info *echo_server_info)
{
	memset(echo_server_info, 0, sizeof(server_info));
	echo_server_info->listen_port = 40000;
	echo_server_info->max_backlog = 1024;
	echo_cli_init(&echo_server_info->cli);
}

void do_accept(server_info *echo_server)
{
	int da_len = 0;
	cli_entry *ptr = new_cli(&echo_server->cli);

	if (ptr) {
		ptr->fd = Accept(echo_server->listen_fd, (struct sockaddr *)&ptr->from_addr, &da_len);
		if (ptr->fd == -1) {
			err_sys("Accept error");
		}
		set_non_block(ptr->fd);
#ifdef USE_EPOLL
		add_event(echo_server->epollfd, ptr->fd, EPOLLIN);
#endif
	}
}

int do_read(server_info *echo_server, int fd)
{
	char	buf[MAXLINE];
	int	n = 0;

	memset(buf, 0, MAXLINE);
	if ((n = readn(fd, (void *)buf, MAXLINE)) != -1) {
		unsigned int current_time = get_now_time();

		echo_server->stat_recvbuf += strlen(buf);
		if ((current_time - echo_server->last_time) > 5000) {
			echo_server->last_time = current_time;
		}
		else if ((current_time - echo_server->last_time) > 1000) {
			/*Speed: KB */
			echo_server->stat_speed = echo_server->stat_recvbuf / (current_time - echo_server->last_time);
			echo_server->last_time = current_time;
			echo_server->stat_recvbuf = 0;
			printf("Speed %d KB\n", echo_server->stat_speed);
			fflush(stdout);
		}
	}
	else { /* Peer disconnected */
		return -1;
	}
	return 0;
}

void do_select(server_info *echo_server)
{
	for (;;) {
		cli_entry *pos, *tmp;
		int max = 0;

		FD_ZERO(&echo_server->rset);
		FD_SET(echo_server->listen_fd, &echo_server->rset);
		max = max > echo_server->listen_fd ? max : echo_server->listen_fd;
		list_for_each_entry_safe(pos, tmp, &echo_server->cli.inuse_list, entry) {
			if (!check_fd_stat(pos->fd)) {
				FD_SET(pos->fd, &echo_server->rset);
				max = max > pos->fd ? max : pos->fd;
			}
			else {
				free_cli(pos, &echo_server->cli);
			}
		}

		Select(max + 1, &echo_server->rset, NULL, NULL, NULL);
		if (FD_ISSET(echo_server->listen_fd, &echo_server->rset)) {	/* socket is readable */
			do_accept(echo_server);
		}
		
		list_for_each_entry_safe(pos, tmp, &echo_server->cli.inuse_list, entry) {
			if (FD_ISSET(pos->fd, &echo_server->rset)) {
				if (-1 == do_read(echo_server, pos->fd)) {
					free_cli(pos, &echo_server->cli);
				}
			}
		}
	}
}

void handle_events(server_info *echo_server, int num)
{
	int i = 0;
	int fd = -1;

	for (; i < num; i++) {
		fd = echo_server->events[i].data.fd;
		if ((fd == echo_server->listen_fd) && (echo_server->events[i].events & EPOLLIN)) {
			do_accept(echo_server);
		}
		else if (echo_server->events[i].events & EPOLLIN) {
			if (-1 == do_read(echo_server, fd)) {
				cli_entry *pos = get_cli_entry(echo_server, fd);
				free_cli(pos, &echo_server->cli);
				delete_event(echo_server->epollfd, fd, EPOLLIN);
			}
			else {
				modify_event(echo_server->epollfd, fd, EPOLLIN);
			}
		}
		else if (echo_server->events[i].events & EPOLLOUT) {
			/* Do write */	
		}
	}
}

void do_epoll(server_info *echo_server)
{
	int ret = 0;
	echo_server->epollfd = epoll_create(FDSIZE);
	add_event(echo_server->epollfd, echo_server->listen_fd, EPOLLIN);

	for(; ;) {
		ret = epoll_wait(echo_server->epollfd, echo_server->events, EPOLLEVENTS, -1);
		handle_events(echo_server, ret);
	}
	close(echo_server->epollfd);
}

void t_echo_server_loop(server_info *echo_server)
{
	int ret = 0;
	struct sockaddr_in sa, da;
	char	buf[MAXLINE];
	int	n;
 
	bzero(&sa, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(echo_server->listen_port);

	echo_server->listen_fd = Socket(AF_INET, SOCK_STREAM, 0);
	set_reuse(echo_server->listen_fd);
	set_non_block(echo_server->listen_fd);
	Bind(echo_server->listen_fd, (struct sockaddr *)(&sa), sizeof(sa));
	Listen(echo_server->listen_fd, echo_server->max_backlog);

#ifdef USE_SELECT
	do_select(echo_server);
#endif
#ifdef USE_EPOLL
	do_epoll(echo_server);
#endif
	close(echo_server->listen_fd);
}

char *(*a)[3][5];

int unit_test_echo_server(void)
{
	//unit_test_list();
	t_echo_server_init(&echo_server_info);
	t_echo_server_loop(&echo_server_info);
	return 0;
}

void unit_test_tmp(void)
{
	int i = 100;
	int b = (int)(((int *)0) + 4);

	printf("!i %d !!i %d b %d\n", (!i), (!! i), b);
	printf("%ld %ld\n", sizeof(a), sizeof(*a));
}

void timer_1(void *arg)
{
	printf("\nhello Timer %d\n", (*((int *)arg)));
	add_timer(get_now_time() + 1000 * (1 + 2 * (*((int *)arg))), timer_1, arg);
}

int main(int argc, char *argv [])
{
	int i;
	int a[128] = {};

	for (i = 0; i < 2; i++) {
		a[i] = i;
		add_timer(get_now_time() + (2 * i + 1) * 1000, timer_1, &a[i]);
	}
	run_timer();
	//unit_test_echo_server();
}
