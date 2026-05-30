#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "key.h"
#include "server.h"

int main()
{
	// remove buffering for stdout
	setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

	// generate key
	char key[PASS_SIZE];

	generate_key(key);
	if (send_key(key))
		return 1;
	// write(1, key, PASS_SIZE);

	// start server, should be blocking
	server_run(key);

	return 1;
}
