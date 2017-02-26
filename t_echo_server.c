#include "t_unp.h"
#include "t_utils.h"
#include "t_list.h"

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

typedef struct {
	int listen_port;
	int listen_fd;
	int max_backlog;
	int stat_recvbuf;
	int stat_speed;
	int last_time;
	cli_manage cli;
} server_info;

server_info echo_server_info;

void echo_cli_init(cli_manage *cli)
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
		ptr = get_cli(&echo_cli_manage);
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

void t_echo_server_loop(server_info *echo_server)
{
	int ret = 0;
	struct sockaddr_in sa, da;
	fd_set	rset;
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

	for (;;) {
		cli_entry *pos, *tmp;
		int max = 0;

		FD_ZERO(&rset);
		FD_SET(echo_server->listen_fd, &rset);
		max = max > echo_server->listen_fd ? max : echo_server->listen_fd;
		list_for_each_entry_safe(pos, tmp, &echo_server->cli.inuse_list, entry) {
			if (!check_fd_stat(pos->fd)) {
				FD_SET(pos->fd, &rset);
				max = max > pos->fd ? max : pos->fd;
			}
			else {
				free_cli(pos, &echo_server->cli);
			}
		}

		Select(max + 1, &rset, NULL, NULL, NULL);
		if (FD_ISSET(echo_server->listen_fd, &rset)) {	/* socket is readable */
			int da_len = 0;
			cli_entry *ptr = get_cli(&echo_server->cli);

			if (ptr) {
				ptr->fd = Accept(echo_server->listen_fd, (struct sockaddr *)&ptr->from_addr, &da_len);
				set_non_block(ptr->fd);
			}
		}
		
		list_for_each_entry_safe(pos, tmp, &echo_server->cli.inuse_list, entry) {
			if (FD_ISSET(pos->fd, &rset)) {
				memset(buf, 0, MAXLINE);
				if ((n = readn(pos->fd, (void *)buf, MAXLINE)) != 0) {
					int current_time = get_now_time();

					echo_server->stat_recvbuf += strlen(buf);
					if ((current_time - echo_server->last_time) > 5000)
					{
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
					/* Echo back */
					//Send(pos->fd, buf, strlen(buf), 0);
				}
				else { /* Peer disconnected */
					printf("Now peer disconnected(fd: %d, address: %s:%d)\n", pos->fd,
						inet_ntoa(pos->from_addr.sin_addr), ntohs(pos->from_addr.sin_port));
					fflush(stdout);
					free_cli(pos, &echo_server->cli);
				}
			}
		}
	}
	close(echo_server->listen_fd);
}

int main(int argc, char *argv[])
{
	//unit_test_list();
	t_echo_server_init(&echo_server_info);
	t_echo_server_loop(&echo_server_info);
	return 0;
}
