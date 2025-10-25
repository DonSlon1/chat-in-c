cmake_minimum_required(VERSION 3.5)
project(chat C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set default install prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install prefix" FORCE)
endif()

# Add subdirectories for server and UI
add_subdirectory(server)
add_subdirectory(ui)

# Installation configuration
include(GNUInstallDirs)

# Print installation directory
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Executables will be installed to: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
