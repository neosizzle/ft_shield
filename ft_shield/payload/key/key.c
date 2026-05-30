
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "key.h"

int generate_key(char *in)
{
	char *char_list = "1234567890~!@#$^&*()qwertyuiop[]QWERTYUIOP{}ASDFGHJKL:asdfghjklZXCVBNM<>zxcvbnm,.";
	int fd = open("/dev/random", O_RDONLY);
    if (fd < 0) {
        printf("generate_key: open /dev/random failed");
        return 1;
    }

    int total_read = 0;

    while (total_read < PASS_SIZE) {
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
	return 0;
}