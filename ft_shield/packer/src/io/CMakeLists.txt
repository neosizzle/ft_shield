add_library(io)

add_compile_options(-fsanitize=address -g3)
add_link_options(-fsanitize=address -g3)

target_sources(io
	PRIVATE
    	io.c

	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS
		.
		FILES
		io.h
)

target_link_libraries(io
  PRIVATE
    logging
)