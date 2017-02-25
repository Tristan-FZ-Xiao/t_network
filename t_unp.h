#ifndef __T_UNP_H__
#define __T_UNP_H__
#undef	MAXLINE
#define	MAXLINE	20		/* to see datagram truncation */

#include	<arpa/inet.h>
#include	<errno.h>		/* for definition of errno */
#include	<fcntl.h>
#include	<netinet/in.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<stdarg.h>		/* ANSI C header file */
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
#include	<unistd.h>
static void	err_doit(int, const char *, va_list);

int Socket(int family, int type, int protocol);
void Bind(int fd, const struct sockaddr *sa, socklen_t salen);
void Listen(int fd, int backlog);
ssize_t Recv(int fd, void *ptr, size_t nbytes, int flags);
ssize_t Recvfrom(int fd,
	void *ptr,
	size_t nbytes,
	int flags,
	struct sockaddr *sa,
	socklen_t *salenptr);
ssize_t Recvmsg(int fd, struct msghdr *msg, int flags);
int Select(int nfds,
	fd_set *readfds,
	fd_set *writefds,
	fd_set *exceptfds,
	struct timeval *timeout);
void Send(int fd, const void *ptr, size_t nbytes, int flags);
void Sendto(int fd,
	const void *ptr,
	size_t nbytes,
	int flags,
	const struct sockaddr *sa,
	socklen_t salen);
int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr);
ssize_t					/* Read "n" bytes from a descriptor. */
readn(int fd, void *vptr, size_t n);
int connect_nonb(int sockfd, const struct sockaddr *saptr, socklen_t salen, int nsec);
int Fcntl(int fd, int cmd, int arg);
void Connect(int fd, const struct sockaddr *sa, socklen_t salen);
void set_non_block(int fd);
void set_block(int fd);

void err_ret(const char *fmt, ...);
void err_sys(const char *fmt, ...);
void err_dump(const char *fmt, ...);
void err_msg(const char *fmt, ...);
void err_quit(const char *fmt, ...);

#endif /* __T_UNP_H__ */