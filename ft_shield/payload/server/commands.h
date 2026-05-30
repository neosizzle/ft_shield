#ifndef __COMMANDS__H__
#define __COMMANDS__H__
#include <sys/select.h>

#define  HELP_MENU "Commands\n\nshell \t\t spawn a shell\nhelp \t\t show this menu\nexit \t\t close the connection\ndownload [src_path] \t\t start download file procedure (WARNING: will block other clients)\nupload [dest_path] \t\t start upload file procedure (WARNING: will block other clients)\n"


int handle_command(char *input, int conn_fd, fd_set* write_fds);

#endif  //!__COMMANDS__H__