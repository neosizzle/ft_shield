# add_compile_options(-fsanitize=address -Wall -Wextra -g3)
# add_link_options(-fsanitize=address -Wall -Wextra -g3)


# include libelf installation on apple
if(APPLE)
	include_directories(/opt/homebrew/include /opt/homebrew/include/libelf)
endif()

add_subdirectory(src)
