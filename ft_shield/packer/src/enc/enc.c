#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "logging.h"
#include "enc.h"

// TODO: make this unsigned char?
char		*xor_encrypt(char *input, size_t input_len, t_key *key)
{
	size_t		i;
	size_t		j;
	char		*encrypt;

	if (!(encrypt = malloc(sizeof(char) * input_len)))
    {
        error("xor_encrypt: malloc error");
		return 0;
    }
	j = 0;
	for (i = 0; i < input_len; i++)
	{
		encrypt[i] = input[i] ^ key->buffer[j];
		j++;
		if (j == key->size)
			j = 0;
	}
	return encrypt;
}


t_key *generate_key(int size)
{
    t_key *res = malloc(sizeof(t_key));
    res->buffer = malloc(size);
    res->size = size;
    char *buffer = res->buffer;

    if (size <= 0 || res == NULL) {
        error("generate_key: malloc fail or size invalid");
        return 0;
    }

    int fd = open("/dev/random", O_RDONLY);
    if (fd < 0) {
        error("generate_key: open /dev/random failed");
        return 0;
    }

    int total_read = 0;

    while (total_read < size) {
        ssize_t bytes_read = read(fd, buffer + total_read, size - total_read);
        if (bytes_read < 0) {
            error("generate_key: read /dev/random failed");
            close(fd);
            return 0;
        }
        total_read += bytes_read;
    }

    close(fd);
    return res;
}

void free_key(t_key *key)
{
    free(key->buffer);
    free(key);
}