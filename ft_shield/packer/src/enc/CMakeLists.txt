add_library(enc)

target_sources(enc
	PRIVATE
    	enc.c

	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS
		.
		FILES
		enc.h
)

target_link_libraries(enc
  PRIVATE
	logging
)