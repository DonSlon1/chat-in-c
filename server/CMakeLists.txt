cmake_minimum_required(VERSION 3.5)
project(chat-server C)
# Server executable
add_executable(chat-server server.c)

# Link necessary libraries (if needed)
# target_link_libraries(chat-server pthread)

# Install server executable
install(TARGETS chat-server
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
