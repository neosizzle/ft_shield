#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include "legit.h"

void run_legit(unsigned char *input, int size)
{
	(void) size;
	struct passwd *pw = getpwuid(getuid());
	printf("%s\n", pw->pw_name);

	for (size_t i = 0; i < 10; i++)
	{
		printf("%x, ", input[i]);
	}
	printf("\n");
}