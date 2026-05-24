#include <stdio.h>
#include <stdlib.h>

#include "logging.h"
#include "io.h"
#include "ft_elf.h"
#include "shellcode.h"
#include "enc.h"
#include "types.h"

#define MAX_FILE_SIZE (size_t) 1024 * 1024 * 1024 * 5

int validate_args(int argc, char const *argv[])
{
	(void) argv;
	if (argc > 4 || argc < 2)
	{
		error("usage: woody <file_name> <output_path> <key_size>\n");
		return 1;
	}
	return 0;
}

int create_stub(t_key *key, t_file_info *guest_file, t_elf_info *elf_info, const char* out_file_path)
{
	// check stub requirements (?)

	// create stub buffer
	long size_stub = guest_file->size + (((SHELLCODE_SIZE + key->size) / PAGE_SIZE) + 1) * PAGE_SIZE;
	unsigned char *stub_buffer;
	if (!(stub_buffer = malloc(size_stub)))
	{
		error("create_stub: failed to allocate stub buffer");
		return (1);
	}
	unsigned char *stub_buffer_snapshot = stub_buffer;
	
	// write binary to stub buffer
	debug("stub buffer init %p", stub_buffer_snapshot);
	int ret = write_binary(&stub_buffer, key, guest_file, elf_info);
	if (ret)
		return ret;

	// write stub buffer to file
	ret = write_file(stub_buffer_snapshot, size_stub, out_file_path);
	if (ret)
		return ret;
	
	free(stub_buffer_snapshot);
	return 0;
}

int main(int argc, char const *argv[])
{
	if (validate_args(argc, argv))
		return 1;

	// for (size_t i = 0; i < SHELLCODE_SIZE; i++)
	// {
	// 	printf("%x ", SHELLCODE_BYTES[i]);
	// }
	// printf("\n");

	// read file contents using mmap
	t_file_info guest_file;
	int ret = read_file(argv[1], &guest_file);
	if (ret)
		return ret;

	// parse elf_info
	t_elf_info elf_info;
	ret = init_elf_info(&elf_info, guest_file);
	if (ret)
	{
		free_file(&guest_file);
		return ret;
	}

	int key_size = (argc < 4) ? KEY_SIZE : atoi(argv[3]);
	if (key_size < 3)
	{
		error("key size must be more than 3");
		free_file(&guest_file);
		return 1;
	}
	t_key *key = generate_key(key_size);
	info("Key generated with size %d", key->size);
	if (!key)
	{
		free_file(&guest_file);
		free_key(key);
		return 1;
	}

	const char *out_file_path = argv[2];
	ret = create_stub(key, &guest_file, &elf_info, out_file_path);
	if (ret)
	{
		free_file(&guest_file);
		free_key(key);
		return ret;
	}
	
	info("woody file created!");
	free_file(&guest_file);
	free_key(key);
	return 0;
}
