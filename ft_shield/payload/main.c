#include <stdio.h>
#include <unistd.h>

int main(int argc, char const *argv[])
{
	if (argc != 2)
	{
		printf("wrong argc \n");
		return 0;
	}
	
	FILE *f;

	f = fopen(argv[1], "r");
	fseek(f, 0L, SEEK_END);
	int size = ftell(f); // get current file pointer
	printf("hello payload: size %d\n", size);
	fseek(f, 0, SEEK_SET);
}
