# add_compile_options(-fsanitize=address -Wall -Wextra -g3)
# add_link_options(-fsanitize=address -Wall -Wextra -g3)

add_executable(payload)

target_sources(payload
	PRIVATE main.c
)

add_subdirectory(key)
add_subdirectory(server)

target_link_libraries(payload
  PRIVATE
	key
	server
)
