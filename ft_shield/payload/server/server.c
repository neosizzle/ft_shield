#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "server.h"
#include "commands.h"

#define GREETING "ft_shield terminal\n: "

int server_run(char *key)
{
	int listen_fd, conn_fd, max_fd, activity, i;
	int client[MAX_CLIENTS];
	int authenticated[MAX_CLIENTS];
	struct sockaddr_in server_addr, client_addr;
	socklen_t addrlen = sizeof(client_addr);

	fd_set readfds, writefds;

	char buffer[BUF_SIZE];

	// init client array
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		client[i] = 0;
		authenticated[i] = 0;
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

	if (listen(listen_fd, MAX_CLIENTS) < 0)
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
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			conn_fd = client[i];

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

			// TODO: auth handshake

			// add to client list
			for (i = 0; i < MAX_CLIENTS; i++)
			{
				if (client[i] == 0)
				{
					client[i] = conn_fd;
					authenticated[i] = 0;
					// send greeting immediately
					send(conn_fd, "Password: ", strlen("Password: "), 0);
					break;
				}
			}
		}

		// handle client IO
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			conn_fd = client[i];

			if (conn_fd > 0 && FD_ISSET(conn_fd, &readfds))
			{
				int bytes = recv(conn_fd, buffer, sizeof(buffer) - 1, 0);

				if (bytes <= 0)
				{
					printf("Client disconnected fd=%d\n", conn_fd);
					close(conn_fd);
					client[i] = 0;
					authenticated[i] = 0;
					continue ;
				}
				if (!authenticated[i])
				{
					buffer[bytes] = '\0';
					buffer[strcspn(buffer, "\r\n")] = '\0';
					if (strcmp(buffer, key) == 0)
					{
						authenticated[i] = 1;
						send(conn_fd, GREETING, strlen(GREETING), 0);
						printf("Client fd=%d authenticated\n", conn_fd);
					}
					else
					{
						send(conn_fd, "Access denied: Wrong password\n", 28, 0);
						printf("Client fd=%d wrong password, disconnecting\n", conn_fd);
						close(conn_fd);
						client[i]        = 0;
						authenticated[i] = 0;
					}
				}
				else
				{
					// TODO: make dynamic here
					// TODO: make this use pthread
					buffer[bytes] = '\0';
					if(handle_command(buffer, conn_fd, &writefds))
						return 1;
					printf("recv: %s\n", buffer);

				}
			}
		}
	}

	close(listen_fd);
	return 0;
}