add_library(types INTERFACE)
configure_file(types.h ${CMAKE_CURRENT_BINARY_DIR}/types.h)

# Export to parent or consumers via target interface
target_include_directories(types INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<INSTALL_INTERFACE:include>
)