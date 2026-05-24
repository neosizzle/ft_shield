cmake_minimum_required(VERSION 3.23)
# set(CMAKE_VERBOSE_MAKEFILE on)

# include libelf installation on apple
if(APPLE)
	include_directories(/opt/homebrew/include /opt/homebrew/include/libelf)
endif()


project(ft_shield)

add_subdirectory(ft_shield)


