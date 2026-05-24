#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "logging.h"
#include "io.h"

int read_file(const char* file_name, t_file_info *file){
	int fd;
	int size;
	void* addr;

	fd = open(file_name, O_RDONLY);
	if (fd < 0)
	{
		error("read_file: open failed with error %d", errno);
		return (errno);
	}
	
	size = lseek(fd, (size_t)0, SEEK_END); // magically seek to the end, returning file size
	if (size < 0)
	{
		close(fd);
		if (!errno)
			errno = 1;
		error("read_file: lseek failed with error %d", errno);
		return (errno);
	}
	addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0); // NOTE: need to free this using munmap
	if (addr == MAP_FAILED)
	{
		error("read_file: mmap failed with error %d", errno);
		close(fd);
		return (errno);
	}
	file->fd = fd;
	file->size = size;
	file->contents = addr;
	return 0;
}

int write_file(unsigned char *stub_buffer, long size_stub, const char* out_file_path)
{
	int fd = open(out_file_path, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	int ret = write(fd, stub_buffer, size_stub);
	if (ret < 1)
	{
		error("write_file: write error");
		return ret;
	}
	
	ret = close(fd);
	return ret;
}

int free_file(t_file_info *file)
{
	int ret = munmap(file->contents, file->size);
	if (ret)
	{
		error("free_file: munmap error");
		return ret;
	}
	
	ret = close(file->fd);
	if (ret)
	{
		error("free_file: close error");
		return ret;
	}
	
	return 0;
}

int determine_exec_file_type(unsigned char *buffer, long buffer_size)
{
	// auto return error if buffer is too small for magic number scanning
	if (buffer_size < 10)
	{
		error("io.determine_exec_file_type: file too small");
		return -1;
	}

	// check for ELF magic number
	if (buffer[0] == 0x7F && buffer[1] == 'E' && buffer[2] == 'L' && buffer[3] == 'F')
	{
		if (buffer[5] == 0x02)
		{
			error("io.determine_exec_file_type: unsupported endianess");
			return -1;	
		}

		// check for 32 or 64 bit
		if (buffer[4] == 1)
			return FILE_TYPE_ELF_32;
		return FILE_TYPE_ELF_64;
	}

	// check for macho magic number
	if (buffer[0] == 0xcf && buffer[1] == 0xfa && buffer[2] == 0xed && buffer[3] == 0xfe)
		return FILE_TYPE_MACHO;

	error("io.determine_exec_file_type: unsupported file");
	return -1;
}