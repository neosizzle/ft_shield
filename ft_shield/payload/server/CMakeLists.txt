add_library(server)

target_sources(server
	PRIVATE
    	server.c
		commands.c

	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS
		.
		FILES
		server.h
		commands.h
)