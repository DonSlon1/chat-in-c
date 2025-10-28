cmake_minimum_required(VERSION 3.5)
# Try to find Raylib, if not found, download and build it
find_package(raylib 5.5 QUIET)
if (NOT raylib_FOUND)
    message(STATUS "Raylib not found, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()

# Fetch raygui (header-only library)
include(FetchContent)
FetchContent_Declare(
    raygui
    GIT_REPOSITORY https://github.com/raysan5/raygui.git
    GIT_TAG 4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raygui)

# UI/Client executable - now modular!
add_executable(chat-client
    client.c
    network.c
    ui_drawing.c
    utils.c
    ../common/protocol.h
    ../common/protocol.c
)

# Include raygui headers (it's header-only, no linking needed)
target_include_directories(chat-client PRIVATE
    ${raygui_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link only Raylib library (raygui is header-only)
target_link_libraries(chat-client
    raylib
    pthread
    m
)

# Install client executable
install(TARGETS chat-client
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
