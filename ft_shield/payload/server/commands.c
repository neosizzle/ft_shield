#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

#include "commands.h"

void get_io_output(const char *cmd)
{
	unsigned long long disk_io = 0, net_io = 0;
    FILE *fp = popen(cmd, "r");
    if (!fp)
    {
        printf("Failed to run command\n");
        return;
    }

	if (fscanf(fp, "%llu %llu", &disk_io, &net_io) != 2) {
        fprintf(stderr, "Failed to read two numbers from command output\n");
        pclose(fp);
        return;
    }

    pclose(fp);
	printf("Disk I/O: %llu\n", disk_io);
    printf("Network I/O: %llu\n", net_io);
}

int retry_send(char *buf, int conn_fd, int size, fd_set* write_fds)
{
	// TODO: add max_retries here
	while (!FD_ISSET(conn_fd, write_fds))
		sleep(1);
	send(conn_fd, buf, size, MSG_NOSIGNAL);

	return 0;
}

static int write_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;

    while (len > 0)
    {
        ssize_t n = write(fd, p, len);

        if (n < 0)
            return -1;

        p += n;
        len -= n;
    }

    return 0;
}

int start_iomon_session(int conn_fd)
{
    int pid = fork();

	// child
	if (!pid)
	{
		dup2(conn_fd, 0);
		dup2(conn_fd, 1);
		dup2(conn_fd, 2);

		char buf[1];

		// set conn fd to nonblock
        int flags = fcntl(conn_fd, F_GETFL);
        fcntl(conn_fd, F_SETFL, flags | O_NONBLOCK);

		// every 1 second, get network and i/o data from the system
		// and print it out
        while (1)
        {
			// if i have received a read, terminate the child process
            ssize_t n = read(conn_fd, buf, sizeof(buf));

			// _exit to prevent double flushing.
			// _exit does not call libc cleanup code
            if (n > 0 || n == 0)
                _exit(0);

			// if i have received nothing yet, keep going
			
			#ifdef __linux__
				const char *command = "awk 'FNR==NR{d+=$6+$10; next} NR>2{n+=$3+$11} END{print d, n}' /proc/diskstats /proc/net/dev";
			#elif __APPLE__
				const char *command = "echo 0 0 ";
			#endif
			get_io_output(command);

            sleep(1);
        }

	}
	waitpid(pid, 0, 0);
	return 0;
}

int start_download_session(int conn_fd)
{
    int pid = fork();

    if (pid == 0)
    {
        dup2(conn_fd, STDOUT_FILENO);
        dup2(conn_fd, STDIN_FILENO);
        dup2(conn_fd, STDERR_FILENO);

        int recv_buf_sz = 1024;
        char recv_buf[recv_buf_sz];

        // ask client for filename to send
        printf("source filename: ");

        ssize_t recv_ret = read(STDIN_FILENO, recv_buf, recv_buf_sz - 1);
        if (recv_ret <= 0)
            _exit(1);

        recv_buf[recv_ret] = 0;
        recv_buf[strcspn(recv_buf, "\n")] = '\0';

        // open source file
        int fd = open(recv_buf, O_RDONLY);
        if (fd < 0)
        {
            perror("open source file");
            _exit(1);
        }

        // get file size
        off_t file_size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);

        if (file_size <= 0)
        {
            printf("invalid file size\n");
            close(fd);
            _exit(1);
        }

        // send file size first
        printf("%lld\n", (long long)file_size);
        fflush(stdout);

		printf("input anything to start download: ");

        recv_ret = read(STDIN_FILENO, recv_buf, recv_buf_sz - 1);
        if (recv_ret <= 0)
            _exit(1);

        ssize_t cum_sent = 0;
        while (cum_sent < file_size)
        {
            ssize_t to_read = (file_size - cum_sent) < recv_buf_sz ? (file_size - cum_sent) : recv_buf_sz;

            ssize_t n = read(fd, recv_buf, to_read);
            if (n <= 0)
            {
                perror("read source");
                break;
            }

            if (write_all(STDOUT_FILENO, recv_buf, n) < 0)
            {
                perror("write to socket");
                break;
            }

            cum_sent += n;
        }

        printf("download complete\n");
        close(fd);
        _exit(0);
    }

    // Parent waits for child to finish
    waitpid(pid, NULL, 0);
    return 0;
}

int start_upload_session(int conn_fd)
{
    int pid = fork();

	// child
	if (!pid)
	{
		int recv_ret;
		int recv_buf_sz = 1024;
		char recv_buf[recv_buf_sz];
		
		dup2(conn_fd, 0);
		dup2(conn_fd, 1);
		dup2(conn_fd, 2);

		// read source file size
		printf("source filesize: ");
		recv_ret = read(0, recv_buf, recv_buf_sz - 1);
		recv_buf[recv_ret] = 0;

		int source_fz = atoi(recv_buf);
		if (!source_fz)
		{
			printf("invalid source filesize, exiting..\n");
			_exit(0);
		}

		
		// read dest filename
		printf("dest filename: ");
		recv_ret = read(0, recv_buf, recv_buf_sz - 1);
		recv_buf[recv_ret] = 0;
		recv_buf[strcspn(recv_buf, "\n")] = '\0';

		// printf("source filesize is %d \n", source_fz);
		// printf("dest filename is %s \n", recv_buf);
		printf("server ready for upload\n");

		int fd = open(recv_buf, O_CREAT | O_WRONLY | O_TRUNC, 0644);

		if (fd < 0)
		{
			if (errno != EEXIST)
			{
				perror("open");
				_exit(0);
			}
		}

		int cum_read_bytes = 0;
		while (cum_read_bytes < source_fz)
		{
			size_t to_read = MIN(source_fz - cum_read_bytes, recv_buf_sz);

			ssize_t read_bytes = read(STDIN_FILENO, recv_buf, to_read);
			if (read_bytes <= 0)
				_exit(0);

			if (write_all(fd, recv_buf, read_bytes) < 0)
				_exit(0);

			cum_read_bytes += read_bytes;
		}
		
		
		printf("upload complete\n");
		close(fd);
		_exit(0);

	}
	waitpid(pid, 0, 0);
	return 0;
}

int start_shell_session(int conn_fd)
{
	int master_fd;
	// TODO: research this. If i just use fork(), sh will complain
	// about no tty
    int pid = forkpty(&master_fd, NULL, NULL, NULL);

	// child
	if (!pid)
	{
		char *name[3];
		dup2(conn_fd, 0);
		dup2(conn_fd, 1);
		dup2(conn_fd, 2);

		name[0] = "/bin/sh";
		name[1] = "-i";
		name[2] = NULL;
		execv(name[0], name);
		exit(1);
	}
	waitpid(pid, 0, 0);
	return 0;
}

void *handle_command(command_worder_data_t* data)
{
	char *input = data->input;
	int conn_fd = data->conn_fd;
	int client_iter = data->client_iter;
	pthread_t *client_threads = data->client_threads;
	fd_set *write_fds = data->write_fds;
	
	// NOTE: mutex for client_threads not needed as write position is unique 
	// for all threads, and there are no reading done
	if (!strcmp(input, "help\n"))
	{
		if (retry_send(HELP_MENU, conn_fd, strlen(HELP_MENU), write_fds))
			exit(1);
		// retry_send(": ", conn_fd, 2, write_fds);
		
	}
	if (!strcmp(input, "shell\n"))
	{
		if (start_shell_session(conn_fd))
			exit(1);
	}
	if (!strcmp(input, "iomon\n"))
	{
		if (start_iomon_session(conn_fd))
			exit(1);
	}
	if (!strcmp(input, "upload\n"))
	{
		if (start_upload_session(conn_fd))
			exit(1);
	}
	if (!strcmp(input, "download\n"))
	{
		if (start_download_session(conn_fd))
			exit(1);
	}
	retry_send(": ", conn_fd, 2, write_fds); // NOTE: this will break server if using client nc + ctrl+c
	client_threads[client_iter] = 0;
	free(input);
	return NULL;
}