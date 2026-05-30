#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

// TODO: generate password here and put in logfile, but assume will send to remote server
	
int main(int argc, char const *argv[])
{
	// if (argc != 2)
	// {
	// 	printf("wrong argc \n");
	// 	return 0;
	// }
	
	// FILE *f;

	// f = fopen(argv[1], "r");
	// fseek(f, 0L, SEEK_END);
	// int size = ftell(f); // get current file pointer
	// printf("hello payload: size %d\n", size);
	// fseek(f, 0, SEEK_SET);

	printf("service init 24!!!!\n");
	fflush(stdout);
	// char *str = "hello !!!!\n";
	// write(1, str, strlen(str));

	while (1) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        printf("%02d:%02d:%02d\n",
               t->tm_hour,
               t->tm_min,
               t->tm_sec);
		fflush(stdout);

        sleep(1); // 1 second
    }

	
}
