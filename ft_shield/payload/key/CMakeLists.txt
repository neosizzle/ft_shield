add_library(key)

target_sources(key
	PRIVATE
    	key.c

	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS
		.
		FILES
		key.h
)