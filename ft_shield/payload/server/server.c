#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <pthread.h>

#include "server.h"
#include "commands.h"

#define GREETING "ft_shield terminal\n: "

// TODO: check if max_clients overflow
int server_run(char *key)
{
	int listen_fd, conn_fd, max_fd, activity, client_iter;

	int client[MAX_CLIENTS];
	int authenticated[MAX_CLIENTS];
	pthread_t client_threads[MAX_CLIENTS];
	command_worder_data_t client_thread_inputs[MAX_CLIENTS];


	struct sockaddr_in server_addr, client_addr;
	socklen_t addrlen = sizeof(client_addr);
	fd_set readfds, writefds;

	char buffer[BUF_SIZE];

	// init client array
	for (client_iter = 0; client_iter < MAX_CLIENTS; client_iter++)
	{
		client[client_iter] = 0;
		authenticated[client_iter] = 0;
		client_threads[client_iter] = 0;

	}

	// create socket
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0)
	{
		printf("server_run: socket creation error %d", errno);
		return 1;
	}

	int opt = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(PORT);

	if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		printf("server_run: socket bind error %d", errno);
		return 1;
	}

	if (listen(listen_fd, MAX_CLIENTS + 1) < 0)
	{
		printf("server_run: socket listen error %d", errno);
		return 1;
	}

	printf("Server listening on port %d\n", PORT);

	while (1)
	{
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		FD_SET(listen_fd, &readfds);
		max_fd = listen_fd;

		// add clients
		for (client_iter = 0; client_iter < MAX_CLIENTS; client_iter++)
		{
			conn_fd = client[client_iter];

			if (conn_fd > 0)
			{
				FD_SET(conn_fd, &readfds);
				FD_SET(conn_fd, &writefds);

				if (conn_fd > max_fd)
					max_fd = conn_fd;
			}
		}

		activity = select(max_fd + 1, &readfds, &writefds, NULL, NULL);

		if (activity < 0 && errno != EINTR)
		{
			perror("select");
			continue;
		}

		// new connection
		if (FD_ISSET(listen_fd, &readfds))
		{
			conn_fd = accept(listen_fd,
							 (struct sockaddr *)&client_addr,
							 &addrlen);

			if (conn_fd < 0)
			{
				perror("accept");
				continue;
			}

			printf("New connection: fd=%d\n", conn_fd);

			// add to client list
			for (client_iter = 0; client_iter < MAX_CLIENTS; client_iter++)
			{
				if (client[client_iter] == 0)
				{
					client[client_iter] = conn_fd;
					authenticated[client_iter] = 0;
					client_threads[client_iter] = 0;
					send(conn_fd, "Password: ", strlen("Password: "), 0);
					break;
				}
			}

			if (client_iter == MAX_CLIENTS)
			{
				const char *max_client_msg = "Max clients reached, please try again soon\n";
				send(conn_fd, max_client_msg, strlen(max_client_msg), 0);
				close(conn_fd);	
			}
		}

		// handle client IO
		for (client_iter = 0; client_iter < MAX_CLIENTS; client_iter++)
		{
			conn_fd = client[client_iter];
			pthread_t curr_thread = client_threads[client_iter];

			// if client_iter is running job, do not handle io
			if (curr_thread != 0)
				continue;

			if (conn_fd > 0 && FD_ISSET(conn_fd, &readfds))
			{
				int bytes = recv(conn_fd, buffer, sizeof(buffer) - 1, 0);

				if (bytes <= 0)
				{
					printf("Client disconnected fd=%d\n", conn_fd);
					close(conn_fd);
					client[client_iter] = 0;
					authenticated[client_iter] = 0;
					client_threads[client_iter] = 0;
					continue ;
				}
				if (!authenticated[client_iter])
				{
					buffer[bytes] = '\0';
					buffer[strcspn(buffer, "\r\n")] = '\0';
					if (strcmp(buffer, key) == 0)
					{
						authenticated[client_iter] = 1;
						send(conn_fd, GREETING, strlen(GREETING), 0);
						printf("Client fd=%d authenticated\n", conn_fd);
					}
					else
					{
						send(conn_fd, "Access denied: Wrong password\n", 28, 0);
						printf("Client fd=%d wrong password, disconnecting\n", conn_fd);
						close(conn_fd);
						client[client_iter]        = 0;
						authenticated[client_iter] = 0;
						client_threads[client_iter] = 0;
					}
				}
				else
				{
					// TODO: make dynamic here
					buffer[bytes] = '\0';
					printf("recv: %s\n", buffer);

					// quick exit handling without passing too much state to handler
					if (!strcmp(buffer, "exit\n"))
					{
						close(conn_fd);
						client[client_iter]        = 0;
						authenticated[client_iter] = 0;
						client_threads[client_iter] = 0;
						continue;
					}


					command_worder_data_t command_data;
					
					command_data.input = strdup(buffer);
					command_data.conn_fd = conn_fd;
					command_data.write_fds = &writefds;
					command_data.client_threads = client_threads; // NOTE: sus of race condition at pthread_create
					command_data.client_iter = client_iter;

					client_thread_inputs[client_iter] = command_data;

					// yes, might be sus because no mutex for client_threads here
					// but this is thread creation code, the job will only run after tid is created at uniqie position, no data race.
					pthread_create(
						&client_threads[client_iter],
						NULL,
						(void * (*)(void *))handle_command,
						&client_thread_inputs[client_iter]
					);


				}
			}
		}
	}

	close(listen_fd);

	return 0;
}