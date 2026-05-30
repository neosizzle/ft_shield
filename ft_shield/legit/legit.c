#include <stdio.h>
#include "legit.h"

void run_legit(unsigned char *input, int size)
{
	(void) size;
	printf("jng\n");

	for (size_t i = 0; i < 10; i++)
	{
		printf("%x, ", input[i]);
	}
	printf("\n");
}