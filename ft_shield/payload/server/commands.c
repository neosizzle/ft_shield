#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <pty.h>
#include <stdio.h>

#include "commands.h"

int retry_send(char *buf, int conn_fd, int size, fd_set* write_fds)
{
	// TODO: add max_retries here
	while (!FD_ISSET(conn_fd, write_fds))
		sleep(1);
	send(conn_fd, buf, size, 0);

	return 0;
}

int start_shell_session(int conn_fd)
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

void *handle_command(command_worder_data_t* data)
{
	char *input = data->input;
	int conn_fd = data->conn_fd;
	int client_iter = data->client_iter;
	pthread_t *client_threads = data->client_threads;
	fd_set *write_fds = data->write_fds;
	
	// NOTE: mutex for client_threads not needed as write position is unique 
	// for all threads, and there are no reading done
	if (!strcmp(input, "help\n"))
	{
		if (retry_send(HELP_MENU, conn_fd, strlen(HELP_MENU), write_fds))
			exit(1);
		retry_send(": ", conn_fd, 2, write_fds);
		
	}
	if (!strcmp(input, "shell\n"))
	{
		if (start_shell_session(conn_fd))
			exit(1);
		retry_send(": ", conn_fd, 2, write_fds); // NOTE: this will break server if using client nc + ctrl+c
	}
	client_threads[client_iter] = 0;
	free(input);
	return NULL;
}