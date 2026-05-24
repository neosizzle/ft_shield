add_library(logging)

target_sources(logging
	PRIVATE
    	logging.c

	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS
		.
		FILES
		logging.h
)