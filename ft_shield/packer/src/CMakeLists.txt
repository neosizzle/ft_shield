add_compile_options(-fsanitize=address -Wall -Wextra -g3)
add_link_options(-fsanitize=address -Wall -Wextra -g3)

add_executable(packer)

add_subdirectory(types)
add_subdirectory(io)
add_subdirectory(ft_elf)
add_subdirectory(enc)
add_subdirectory(logging)
add_subdirectory(shellcode)
add_subdirectory(libft)

target_sources(packer
	PRIVATE main.c
)

target_link_libraries(packer
  PRIVATE
    logging
	io
	types
	ft_elf
	shellcode
	enc
	libft
)
