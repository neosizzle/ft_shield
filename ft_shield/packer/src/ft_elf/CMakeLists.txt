add_library(ft_elf)

target_sources(ft_elf
	PRIVATE
    	parsing.c
		binary.c

	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS
		.
		FILES
		ft_elf.h
)

target_link_libraries(ft_elf
  PRIVATE
    types
	logging
	io
	enc
	shellcode
	libft
)