add_library(shellcode INTERFACE)

set(SHELLCODE_ROOT "${CMAKE_BINARY_DIR}/ft_shield/packer/src/shellcode")
file(COPY "shellcode.s" DESTINATION ${SHELLCODE_ROOT})

# compile shellcode
execute_process(
    COMMAND sh -c "cd ${SHELLCODE_ROOT} && nasm shellcode.s -f elf64 -o shellcode.o"
)

# extract shellcode instruction bytes
if(APPLE)
    message(STATUS "Building on macOS")
    set(SHELLCODE_EXT_CMD "cd ${SHELLCODE_ROOT} && 	greadelf -x .text shellcode.o | grep -v 'NOTE: This section has relocations' | awk '{if(NR>2)print}' | sed -e '$d' | sed 's/  //' | cut -f 2- -d ' ' | cut -d ' ' -f 1,2,3,4 | sed 's/ //g' | sed 's/\\n//g' | tr -d '\n' | sed 's/../\\\\x&/g'")
elseif(UNIX AND NOT APPLE)
  message(STATUS "Building on Linux")
  set(SHELLCODE_EXT_CMD "cd ${SHELLCODE_ROOT} && 	readelf -x .text shellcode.o | awk '{if(NR>2)print}' | sed -e '$d' | sed 's/  //' | cut -f 2- -d ' ' | cut -d ' ' -f 1,2,3,4 | sed 's/ //g' | sed 's/\\n//g' | tr -d '\n' | sed 's/../\\\\x&/g'")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

execute_process(
      COMMAND sh -c "${SHELLCODE_EXT_CMD}"
      OUTPUT_VARIABLE SHELLCODE_BYTES_RAW
      OUTPUT_STRIP_TRAILING_WHITESPACE
)

# extract shellcode size
execute_process(
      COMMAND sh -c "echo $(( $(echo ${SHELLCODE_BYTES_RAW} | wc -c) / 3 ))"
      OUTPUT_VARIABLE SHELLCODE_SIZE
      OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "SHELLCODE SIZE ${SHELLCODE_SIZE}")
message(STATUS "SHELLCODE BYTES ${SHELLCODE_BYTES_RAW}")


# Generate the header from template
configure_file(shellcode.h.in ${CMAKE_CURRENT_BINARY_DIR}/shellcode.h)

# Export to parent or consumers via target interface
target_include_directories(shellcode INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<INSTALL_INTERFACE:include>
)