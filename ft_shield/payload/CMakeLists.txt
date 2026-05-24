# add_compile_options(-fsanitize=address -Wall -Wextra -g3)
# add_link_options(-fsanitize=address -Wall -Wextra -g3)

add_executable(payload)

target_sources(payload
	PRIVATE main.c
)

# target_link_libraries(woody_woodpacker
#   PRIVATE
#     logging
# 	io
# 	types
# 	ft_elf
# 	shellcode
# 	enc
# 	libft
# )
