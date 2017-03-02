#include "t_unp.h"

/* include Socket */
int Socket(int family, int type, int protocol)
{
	int	n;

	if ((n = socket(family, type, protocol)) < 0)
		err_sys("socket error");
	return (n);
}
/* end Socket */

void Bind(int fd, const struct sockaddr *sa, socklen_t salen)
{
	if (bind(fd, sa, salen) < 0)
		err_sys("bind error");
}

void Listen(int fd, int backlog)
{
	char	*ptr;

	/*4can override 2nd argument with environment variable */
	if ((ptr = getenv("LISTENQ")) != NULL)
		backlog = atoi(ptr);

	if (listen(fd, backlog) < 0)
		err_sys("listen error");
}

ssize_t Recv(int fd, void *ptr, size_t nbytes, int flags)
{
	ssize_t		n;

	if ((n = recv(fd, ptr, nbytes, flags)) < 0)
		err_sys("recv error");
	return (n);
}

ssize_t Recvfrom(int fd, void *ptr, size_t nbytes,
	int flags, struct sockaddr *sa, socklen_t *salenptr)
{
	ssize_t		n;

	if ((n = recvfrom(fd, ptr, nbytes, flags, sa, salenptr)) < 0)
		err_sys("recvfrom error");
	return (n);
}

ssize_t Recvmsg(int fd, struct msghdr *msg, int flags)
{
	ssize_t		n;

	if ((n = recvmsg(fd, msg, flags)) < 0)
		err_sys("recvmsg error");
	return (n);
}

int Select(int nfds,
	fd_set *readfds,
	fd_set *writefds,
	fd_set *exceptfds,
	struct timeval *timeout)
{
	int		n;

	if ((n = select(nfds, readfds, writefds, exceptfds, timeout)) < 0)
		err_sys("select error");
	return (n);		/* can return 0 on timeout */
}

void Send(int fd, const void *ptr, size_t nbytes, int flags)
{
	if (send(fd, ptr, nbytes, flags) != (ssize_t)nbytes)
		err_sys("send error");
}

void Sendto(int fd,
	const void *ptr,
	size_t nbytes,
	int flags,
	const struct sockaddr *sa,
	socklen_t salen)
{
	if (sendto(fd, ptr, nbytes, flags, sa, salen) != (ssize_t)nbytes)
		err_sys("sendto error");
}


int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr)
{
	int	n;

again:
	if ((n = accept(fd, sa, salenptr)) < 0) {
#ifdef	EPROTO
		if (errno == EPROTO || errno == ECONNABORTED)
#else
			if (errno == ECONNABORTED)
#endif
				goto again;
			else
				err_sys("accept error");
	}
	return (n);
}


ssize_t					/* Read "n" bytes from a descriptor. */
readn(int fd, void *vptr, size_t n)
{
	size_t	nleft;
	ssize_t	nread;
	char	*ptr;

	ptr = (char *)vptr;
	nleft = n;
	while (nleft > 0) {
		if ((nread = read(fd, ptr, nleft)) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				nread = 0;		/* and call read() again */
			else
				return (-1);
		}
		else if (nread == 0)
			break;				/* EOF */

		nleft -= nread;
		ptr   += nread;
	}
	return (n - nleft);		/* return >= 0 */
}
/* end readn */

int connect_nonb(int sockfd, const struct sockaddr *saptr, socklen_t salen, int nsec)
{
	int				flags, n, error;
	socklen_t		len;
	fd_set			rset, wset;
	struct timeval	tval;

	flags = Fcntl(sockfd, F_GETFL, 0);
	Fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

	error = 0;
	if ((n = connect(sockfd, saptr, salen)) < 0)
		if (errno != EINPROGRESS)
			return (-1);

	/* Do whatever we want while the connect is taking place. */

	if (n == 0)
		goto done;	/* connect completed immediately */

	FD_ZERO(&rset);
	FD_SET(sockfd, &rset);
	wset = rset;
	tval.tv_sec = nsec;
	tval.tv_usec = 0;

	if ((n = Select(sockfd+1,
		&rset,
		&wset,
		NULL,
		nsec ? &tval : NULL)) == 0) {
		close(sockfd);		/* timeout */
		errno = ETIMEDOUT;
		return (-1);
	}

	if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) {
		len = sizeof(error);
		if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
			return (-1);			/* Solaris pending error */
	}
	else
		err_quit("select error: sockfd not set");

done:
	Fcntl(sockfd, F_SETFL, flags);	/* restore file status flags */

	if (error) {
		close(sockfd);		/* just in case */
		errno = error;
		return (-1);
	}
	return (0);
}

int Fcntl(int fd, int cmd, int arg)
{
	int	n;

	if ((n = fcntl(fd, cmd, arg)) == -1)
		err_sys("fcntl error");
	return (n);
}

void Connect(int fd, const struct sockaddr *sa, socklen_t salen)
{
	if (connect(fd, sa, salen) < 0)
		err_sys("connect error");
}

void set_non_block(int fd)
{
	int val = Fcntl(fd, F_GETFL, 0);
	Fcntl(fd, F_SETFL, val | O_NONBLOCK);
}

void set_block(int fd)
{
	int val = Fcntl(fd, F_GETFL, 0);
	Fcntl(fd, F_SETFL, val & (~O_NONBLOCK));
}

void set_reuse(int fd)
{
	int on = 1;  

	if ((setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0) {
		perror("setsockopt failed");
		exit(EXIT_FAILURE);
	}
}

int check_fd_stat(int fd)
{
	struct stat st;
	int re;
	
	re = fstat(fd, &st);
	if (re != 0)
	{
		printf("Fd[%d] stat error: %s", fd, strerror(errno));
	}

	return re;
}
int send_tcp(int fd, const char *buf, int len)
{
	int send_len;
	int sum = 0;
	int n = 0;

	while (sum < len)
	{
		send_len = send(fd, buf + sum, len - sum, 0);

		if (send_len < 0)
		{
			if (errno == EINTR)
			{
				return sum;
			}
			else if (errno == EAGAIN)
			{
				n++;
				if (n > 2)
				{
					return sum;
				}
				continue;
			}
			else
			{
				return -1;
			}
		}
		else if (send_len == 0)
		{
			return -1;
		}

		sum = sum + send_len;
	}

	return sum;
}

void add_event(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

void delete_event(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
}

void modify_event(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}
