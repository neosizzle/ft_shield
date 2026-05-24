add_library(libft)

add_compile_options(-fsanitize=address -g3)
add_link_options(-fsanitize=address -g3)

target_sources(libft
	PRIVATE
    	libft.c

	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS
		.
		FILES
		libft.h
)

target_link_libraries(libft
  PRIVATE
    types
)