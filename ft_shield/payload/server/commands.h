#ifndef __COMMANDS__H__
#define __COMMANDS__H__
#include <sys/select.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define  HELP_MENU "Commands\n\nshell \t\t spawn a shell\nhelp \t\t show this menu\nexit \t\t close the connection\ndownload \t\t start download file procedure (WARNING: will block)\nupload \t\t start upload file procedure (WARNING: will block)\niomon \t\t monitor io data\n"

typedef struct {
	char *input;
	int conn_fd;
	fd_set* write_fds;
	pthread_t *client_threads;
	int client_iter;
} command_worder_data_t;

void *handle_command(command_worder_data_t* data);


#endif  //!__COMMANDS__H__