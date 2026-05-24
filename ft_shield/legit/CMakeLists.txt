add_library(legit)

target_sources(legit
	PRIVATE
    	legit.c

	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS
		.
		FILES
		legit.h
)