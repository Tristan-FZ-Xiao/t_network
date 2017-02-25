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
}cli_manage;

cli_manage echo_cli_manage;

void echo_cli_init(void)
{
	INIT_LIST_HEAD(&echo_cli_manage.free_list);
	INIT_LIST_HEAD(&echo_cli_manage.inuse_list);
}

cli_entry *get_cli(void)
{
	cli_entry *ptr = NULL;
	
	if (list_empty(&echo_cli_manage.free_list)) {
		ptr = (cli_entry *)malloc(sizeof(cli_entry));		
		if (ptr)
			memset(ptr, 0, sizeof(cli_entry));
		else
			return NULL;
	}
	else {
		ptr = echo_cli_manage.free_list.next;
		list_del(&ptr->entry);
		echo_cli_manage.free--;
	}
	list_add_tail(&ptr->entry, &echo_cli_manage.inuse_list);
	echo_cli_manage.inuse++;
	return ptr;
}

void free_cli(cli_entry *ptr)
{
	list_del(&ptr->entry);
	echo_cli_manage.inuse--;
	memset(ptr, 0, sizeof(cli_entry));
	list_add(&ptr->entry, &echo_cli_manage.free_list);
	echo_cli_manage.free++;
}

void cli_print(void)
{
	cli_entry *pos = NULL;
	int i = 0;
	
	printf("Client Info\n");
	printf("Free\t%d\n", echo_cli_manage.free);
	printf("No.\t\tfd\t\tfrom_addr\n");
	list_for_each_entry(pos, &echo_cli_manage.free_list, entry) {
		printf("%d\t\t%d\t\t%s:%d\n", i++, pos->fd, inet_ntoa(pos->from_addr.sin_addr), ntohs(pos->from_addr.sin_port));
	}
	printf("Inuse\t%d\n", echo_cli_manage.inuse);
	printf("No.\t\tfd\t\tfrom_addr\n");
	list_for_each_entry(pos, &echo_cli_manage.inuse_list, entry) {
		printf("%d\t\t%d\t\t%s:%d\n", i++, pos->fd, inet_ntoa(pos->from_addr.sin_addr), ntohs(pos->from_addr.sin_port));
		fflush(stdout);
	}
}

void unit_test_list(void)
{
	cli_entry *ptr = NULL;	
	int i = 0;
	
	echo_cli_init();
	for (; i < 10; i++) {
		ptr = get_cli();
		ptr->fd = i * 2;
		ptr->from_addr.sin_family = AF_INET;
		ptr->from_addr.sin_port = htons(i * i);
		ptr->from_addr.sin_addr.s_addr = htonl(0xc0a80000 + i + 1);
	}
	cli_print();
	free_cli(ptr);
	cli_print();
}
int main(int argc, char *argv[])
{
	unit_test_list();
	return 0;
}