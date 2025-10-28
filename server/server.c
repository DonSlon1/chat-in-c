#include "server.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

// --- Public Function Definitions ---

bool server_init(Server *server, int port) {
    server->client_count = 0;
    server->client_capacity = DEFAULT_CLIENT_COUNT;
    server->clients = malloc(sizeof(Client) * server->client_capacity);
    if (server->clients == NULL) {
        perror("Failed to allocate initial client array");
        return false;
    }

    // Initialize default room
    server->room_count = 1;
    strncpy(server->rooms[0].name, "general", MAX_ROOM_NAME - 1);
    server->rooms[0].client_count = 0;

    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd < 0) {
        perror("Error: Socket is not created");
        return false;
    }

    int opt = 1;
    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server->server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error: bind was not successful");
        close(server->server_fd);
        return false;
    }

    if (listen(server->server_fd, 10) < 0) {
        perror("Error: listen was not successful");
        close(server->server_fd);
        return false;
    }

    set_nonblocking(server->server_fd);
    printf("Bind successful, start listening on port %d\n", port);
    return true;
}

void server_shutdown(Server *server) {
    printf("Server shutting down...\n");
    if (server->clients != NULL) {
        for (int i = 0; i < server->client_count; i++) {
            close(server->clients[i].fd);
        }
        free(server->clients);
        server->clients = NULL;
    }
    if (server->server_fd >= 0) {
        close(server->server_fd);
        server->server_fd = -1;
    }
    // Rooms are statically allocated, no cleanup needed
}

void server_poll_events(Server *server) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(server->server_fd, &read_fds);
    int max_fd = server->server_fd;

    for (int i = 0; i < server->client_count; i++) {
        FD_SET(server->clients[i].fd, &read_fds);
        if (server->clients[i].fd > max_fd) {
            max_fd = server->clients[i].fd;
        }
    }

    int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

    if (activity < 0) {
        if (errno == EINTR) {
            return; // Interrupted by signal, just loop again
        }
        perror("select error");
        return;
    }

    // Check for new connections
    if (FD_ISSET(server->server_fd, &read_fds)) {
        server_accept_new_clients(server);
    }

    // Check for client data
    for (int i = 0; i < server->client_count; i++) {
        if (FD_ISSET(server->clients[i].fd, &read_fds)) {
            // Handle data, and if the client was removed, decrement i
            // to correctly check the client that got shifted into its place.
            if (server_handle_client_data(server, i)) {
                i--;
            }
        }
    }
}

void server_broadcast_message(Server *server, const uint8_t *data, const int len, int sender_index) {
    // sender_index == -1 means broadcast to ALL (system messages)
    if (sender_index < 0 || sender_index >= server->client_count) {
        for (int j = 0; j < server->client_count; j++) {
            ssize_t sent = send(server->clients[j].fd, data, len, MSG_NOSIGNAL);
            if (sent < 0) {
                printf("Failed to send to client %d\n", server->clients[j].fd);
            }
        }
        return;
    }

    // Broadcast only to clients in same room as sender
    const char *sender_room = server->clients[sender_index].current_room;
    printf("Broadcasting to room '%s' (sender index %d)\n", sender_room, sender_index);

    for (int j = 0; j < server->client_count; j++) {
        if (j != sender_index && strcmp(server->clients[j].current_room, sender_room) == 0) {
            ssize_t sent = send(server->clients[j].fd, data, len, MSG_NOSIGNAL);
            if (sent < 0) {
                printf("Failed to send to client %d\n", server->clients[j].fd);
            }
        }
    }
}

// --- Static Helper Function Definitions ---

// Room Management Functions

static Room* find_room(Server *server, const char *room_name) {
    for (int i = 0; i < server->room_count; i++) {
        if (strcmp(server->rooms[i].name, room_name) == 0) {
            return &server->rooms[i];
        }
    }
    return NULL;
}

static void room_add_client(Room *room, int client_fd) {
    for (int i = 0; i < room->client_count; i++) {
        if (room->client_fds[i] == client_fd) {
            return;  // Already in room
        }
    }

    if (room->client_count < 100) {
        room->client_fds[room->client_count++] = client_fd;
        printf("Added client fd=%d to room '%s' (now %d clients)\n",
               client_fd, room->name, room->client_count);
    }
}

static void room_remove_client(Room *room, int client_fd) {
    for (int i = 0; i < room->client_count; i++) {
        if (room->client_fds[i] == client_fd) {
            memmove(&room->client_fds[i],
                    &room->client_fds[i + 1],
                    (room->client_count - i - 1) * sizeof(int));
            room->client_count--;
            printf("Removed client fd=%d from room '%s' (now %d clients)\n",
                   client_fd, room->name, room->client_count);
            break;
        }
    }
}

static Room* create_room(Server *server, const char *room_name) {
    if (server->room_count >= MAX_ROOMS) {
        printf("ERROR: Cannot create room '%s', max rooms reached\n", room_name);
        return NULL;
    }

    Room *room = &server->rooms[server->room_count++];
    strncpy(room->name, room_name, MAX_ROOM_NAME - 1);
    room->client_count = 0;
    printf("Created new room: '%s'\n", room_name);
    return room;
}

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
    }
}

static void server_accept_new_clients(Server *server) {
    while (1) {
        int client_fd = accept(server->server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // No more pending connections
            }
            perror("accept error");
            break;
        }

        set_nonblocking(client_fd);

        // Resize client array if full
        if (server->client_count >= server->client_capacity) {
            server->client_capacity *= 2;
            Client *new_clients = realloc(server->clients, sizeof(Client) * server->client_capacity);

            if (new_clients == NULL) {
                perror("error while reallocating memory");
                close(client_fd);
                continue;
            }
            server->clients = new_clients;
        }

        // Add new client to the list
        Client *new_client = &server->clients[server->client_count];
        new_client->fd = client_fd;
        new_client->username[0] = '\0';
        memset(new_client->recv_buffer, 0, MAX_MESSAGE_SIZE);
        new_client->buffer_pos = 0;
        strncpy(new_client->current_room, "general", MAX_ROOM_NAME - 1);

        server->client_count++;
        printf("New client connected: fd=%d (total clients: %d)\n",
               client_fd, server->client_count);

        // Send welcome message
        uint8_t welcome_buf[MAX_MESSAGE_SIZE];
        const char *welcome_text = "Welcome to the chat server! Please send your username.";
        int welcome_len = protocol_create_system_message(welcome_buf, welcome_text);
        if (welcome_len > 0) {
            send(client_fd, welcome_buf, welcome_len, MSG_NOSIGNAL);
        }
    }
}

static bool server_handle_client_data(Server *server, int client_index) {
    Client *client = &server->clients[client_index];
    uint8_t temp_buffer[MAX_MESSAGE_SIZE];

    ssize_t bytes = recv(client->fd, temp_buffer, MAX_MESSAGE_SIZE - 1, 0);

    if (bytes > 0) {
        if (client->buffer_pos + bytes > MAX_MESSAGE_SIZE) {
            printf("ERROR: Buffer overflow for client %d\n", client->fd);
            server_remove_client(server, client_index);
            return true;
        }
        //server_remove_client(server, client_index);
        memcpy(client->recv_buffer + client->buffer_pos,temp_buffer,bytes);
        client->buffer_pos += bytes;
        while (client->buffer_pos >= sizeof(MessageHeader)) {
            MessageHeader header;
            if (!protocol_parse_header(client->recv_buffer,client->buffer_pos,&header)) {
                printf("ERROR: Invalid protocol header from client %d\n", client->fd);
                server_remove_client(server, client_index);
                return true;
            }

            const size_t total_msg_size = sizeof(MessageHeader) + header.content_len;

            if (client->buffer_pos < total_msg_size) {
                printf("Waiting for the full message\n");
                break;
            }

            client_process_message(server,client_index,client->recv_buffer,total_msg_size);

            const size_t remaining = client->buffer_pos - total_msg_size;
            if (remaining > 0) {
                memmove(client->recv_buffer,
                       client->recv_buffer + total_msg_size,
                       remaining);
            }
            client->buffer_pos = remaining;
        }

    } else if (bytes == 0) {
        // Client disconnected gracefully
        printf("Client %d (%s) disconnected\n",
               client->fd, client->username[0] ? client->username : "unknown");

        if (client->username[0] != '\0') {
            char message[MAX_CONTENT_LEN];
            uint8_t buffer[MAX_MESSAGE_SIZE];
            snprintf(message, MAX_CONTENT_LEN, "%s left the chat\n", client->username);
            const int buf_len = protocol_create_system_message(buffer,message);
            server_broadcast_message(server, buffer,buf_len, client_index);
            server_broadcast_user_list(server);
        }

        server_remove_client(server, client_index);
        return true; // Client was removed
    } else {
        // Error
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recv error");
            server_remove_client(server, client_index);
            return true; // Client was removed
        }
    }

    return false; // Client was not removed
}

static void server_remove_client(Server *server, int index) {
    if (index < 0 || index >= server->client_count) {
        return;
    }

    // Remove from current room
    Client *client = &server->clients[index];
    Room *room = find_room(server, client->current_room);
    if (room) {
        room_remove_client(room, client->fd);
    }

    // Close the socket
    close(server->clients[index].fd);

    // Shift the rest of the array down
    memmove(&server->clients[index],
            &server->clients[index + 1],
            (server->client_count - index - 1) * sizeof(Client));

    server->client_count--;
}

static void client_process_message(Server *server, int client_index, uint8_t *message, size_t total_message_size) {
    Client *client = &server->clients[client_index];

    // Parse header to determine message type
    MessageHeader header;
    if (!protocol_parse_header(message, total_message_size, &header)) {
        printf("ERROR: Failed to parse header from client %d\n", client->fd);
        send_error_message(server, client_index, "Invalid message header");
        return;
    }

    printf("DEBUG: Received message type 0x%02x from client %d\n", header.type, client->fd);

    // Handle different message types
    switch (header.type) {
        case MSG_TYPE_CHAT: {
            ChatMessage chat_msg;
            if (!protocol_parse_chat_message(message, total_message_size, &chat_msg)) {
                printf("ERROR: Failed to parse chat message from client %d\n", client->fd);
                send_error_message(server, client_index, "Invalid chat message format");
                return;
            }

            printf("DEBUG: Chat message - user='%s', room='%s', msg='%s'\n",
                   chat_msg.username, chat_msg.room, chat_msg.message);

            // First message = username registration
            if (client->username[0] == '\0') {
                strncpy(client->username, chat_msg.username, sizeof(client->username) - 1);
                printf("Client %d registered username: %s\n", client->fd, client->username);

                // Add client to general room
                Room *general = find_room(server, "general");
                if (general) {
                    room_add_client(general, client->fd);
                }

                // Send system message announcing new user
                uint8_t announce_buf[MAX_MESSAGE_SIZE];
                char announce_text[MAX_CONTENT_LEN];
                snprintf(announce_text, MAX_CONTENT_LEN, "%s joined the chat", client->username);
                int announce_len = protocol_create_system_message(announce_buf, announce_text);
                if (announce_len > 0) {
                    server_broadcast_message(server, announce_buf, announce_len, client_index);
                }
                server_broadcast_user_list(server);
            } else {
                // Regular chat message - broadcast to all IN SAME ROOM (including sender)
                printf("Broadcasting message from %s: %s\n", client->username, chat_msg.message);

                // Send to everyone in the room (including sender)
                const char *sender_room = client->current_room;
                for (int j = 0; j < server->client_count; j++) {
                    if (strcmp(server->clients[j].current_room, sender_room) == 0) {
                        ssize_t sent = send(server->clients[j].fd, message, (int)total_message_size, MSG_NOSIGNAL);
                        if (sent < 0) {
                            printf("Failed to send to client %d\n", server->clients[j].fd);
                        }
                    }
                }
            }
            break;
        }

        case MSG_TYPE_COMMAND: {
            CommandMessage cmd_msg;
            if (!protocol_parse_command_message(message, total_message_size, &cmd_msg)) {
                printf("ERROR: Failed to parse command from client %d\n", client->fd);
                send_error_message(server, client_index, "Invalid command format");
                return;
            }

            printf("DEBUG: Command from client %d: %s\n", client->fd, cmd_msg.command);

            // Handle /dm <username> <message>
            if (strncmp(cmd_msg.command, "/dm ", 4) == 0) {
                const char *rest = cmd_msg.command + 4;
                char target_username[MAX_USERNAME_LEN];
                const char *dm_message = NULL;

                // Parse: /dm username message text
                int i = 0;
                while (rest[i] != ' ' && rest[i] != '\0' && i < MAX_USERNAME_LEN - 1) {
                    target_username[i] = rest[i];
                    i++;
                }
                target_username[i] = '\0';

                if (rest[i] == ' ') {
                    dm_message = rest + i + 1;
                }

                if (dm_message == NULL || strlen(dm_message) == 0) {
                    send_error_message(server, client_index, "Usage: /dm <username> <message>");
                } else {
                    // Find target user
                    int target_index = -1;
                    for (int j = 0; j < server->client_count; j++) {
                        if (strcmp(server->clients[j].username, target_username) == 0) {
                            target_index = j;
                            break;
                        }
                    }

                    if (target_index == -1) {
                        char error[MAX_CONTENT_LEN];
                        snprintf(error, MAX_CONTENT_LEN, "User '%s' not found", target_username);
                        send_error_message(server, client_index, error);
                    } else {
                        // Create DM as chat message with recipient username as "room"
                        uint8_t dm_buf[MAX_MESSAGE_SIZE];
                        int dm_len = protocol_create_chat_message(dm_buf,
                                                                  client->username,
                                                                  target_username,  // Use recipient as "room"
                                                                  dm_message);
                        if (dm_len > 0) {
                            // Send to recipient
                            send(server->clients[target_index].fd, dm_buf, dm_len, MSG_NOSIGNAL);
                            // Also send back to sender (so they see it)
                            send(client->fd, dm_buf, dm_len, MSG_NOSIGNAL);
                        }
                    }
                }
            }
            // Handle /join <room>
            else if (strncmp(cmd_msg.command, "/join ", 6) == 0) {
                const char *room_name = cmd_msg.command + 6;

                // Remove from old room
                Room *old_room = find_room(server, client->current_room);
                if (old_room) {
                    room_remove_client(old_room, client->fd);
                }

                // Find or create new room
                Room *new_room = find_room(server, room_name);
                if (!new_room) {
                    new_room = create_room(server, room_name);
                }

                if (new_room) {
                    strncpy(client->current_room, room_name, MAX_ROOM_NAME - 1);
                    room_add_client(new_room, client->fd);

                    char msg[MAX_CONTENT_LEN];
                    snprintf(msg, MAX_CONTENT_LEN, "Joined room: %s", room_name);
                    uint8_t buf[MAX_MESSAGE_SIZE];
                    int len = protocol_create_system_message(buf, msg);
                    send(client->fd, buf, len, MSG_NOSIGNAL);
                } else {
                    send_error_message(server, client_index, "Failed to join room (max rooms reached)");
                }
            }
            // Handle /rooms
            else if (strcmp(cmd_msg.command, "/rooms") == 0) {
                char msg[MAX_CONTENT_LEN] = "Available rooms:\n";
                for (int i = 0; i < server->room_count; i++) {
                    char line[128];
                    snprintf(line, 128, "  - %s (%d users)\n",
                             server->rooms[i].name,
                             server->rooms[i].client_count);
                    strncat(msg, line, MAX_CONTENT_LEN - strlen(msg) - 1);
                }

                uint8_t buf[MAX_MESSAGE_SIZE];
                int len = protocol_create_system_message(buf, msg);
                send(client->fd, buf, len, MSG_NOSIGNAL);
            }
            // Handle /leave
            else if (strcmp(cmd_msg.command, "/leave") == 0) {
                if (strcmp(client->current_room, "general") == 0) {
                    send_error_message(server, client_index, "Already in general room");
                } else {
                    Room *old_room = find_room(server, client->current_room);
                    if (old_room) {
                        room_remove_client(old_room, client->fd);
                    }

                    strncpy(client->current_room, "general", MAX_ROOM_NAME - 1);
                    Room *general = find_room(server, "general");
                    if (general) {
                        room_add_client(general, client->fd);
                    }

                    uint8_t buf[MAX_MESSAGE_SIZE];
                    int len = protocol_create_system_message(buf, "Returned to general room");
                    send(client->fd, buf, len, MSG_NOSIGNAL);
                }
            }
            // Handle /help
            else if (strcmp(cmd_msg.command, "/help") == 0) {
                const char *help_text =
                    "Available commands:\n"
                    "  /help - Show this help message\n"
                    "  /rooms - List all rooms\n"
                    "  /join <room> - Join or create a room\n"
                    "  /leave - Return to general room\n"
                    "  /dm <username> <message> - Send direct message";

                uint8_t buf[MAX_MESSAGE_SIZE];
                int len = protocol_create_system_message(buf, help_text);
                send(client->fd, buf, len, MSG_NOSIGNAL);
            }
            else {
                send_error_message(server, client_index, "Unknown command. Type /help for available commands");
            }
            break;
        }

        case MSG_TYPE_PING: {
            // TODO: Send PONG response
            printf("DEBUG: Received PING from client %d\n", client->fd);
            break;
        }

        default:
            printf("WARNING: Unknown message type 0x%02x from client %d\n", header.type, client->fd);
            send_error_message(server, client_index, "Unsupported message type");
            break;
    }
}

static void send_error_message(Server *server, int client_index, const char *message) {
    uint8_t response[MAX_MESSAGE_SIZE];
    int len = protocol_create_error_message(response, message);
    if (len < 0) {
        printf("ERROR: Failed to create error message\n");
        return;
    }

    ssize_t sent = send(server->clients[client_index].fd, response, len, MSG_NOSIGNAL);
    if (sent < 0) {
        printf("ERROR: Failed to send error message to client %d\n", server->clients[client_index].fd);
    }
}

void server_broadcast_user_list(Server *server) {
    uint8_t announce_buf[MAX_MESSAGE_SIZE];

    const char **all_users = malloc(sizeof(char*) * server->client_count);
    for (int i = 0; i < server->client_count; i++) {
        all_users[i] = server->clients[i].username;
    }

    int len = protocol_create_userlist_message(announce_buf, all_users, server->client_count);
    server_broadcast_message(server, announce_buf, len, -1);

    free(all_users);
}
