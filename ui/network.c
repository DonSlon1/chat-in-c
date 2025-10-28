#include "client.h"
#include "../common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

SimpleClient* create_client(void) {
    SimpleClient *client = malloc(sizeof(SimpleClient));
    if (client == NULL) {
        return NULL;
    }
    client->socket_fd = -1;
    client->connected = false;
    client->username[0] = '\0';
    client->recv_buffer[0] = '\0';
    client->buffer_pos = 0;
    return client;
}

bool connect_to_server(SimpleClient *client, const char *ip, int port) {
    if (client == NULL) {
        return false;
    }

    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0) {
        printf("Failed to create socket\n");
        return false;
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (connect(client->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Failed to connect\n");
        close(client->socket_fd);
        client->socket_fd = -1;
        return false;
    }

    fcntl(client->socket_fd, F_SETFL, O_NONBLOCK);
    client->connected = true;
    printf("Connected to %s:%d\n", ip, port);
    return true;
}

bool send_chat_message(SimpleClient *client, const char *room, const char *message) {
    if (client == NULL || !client->connected) {
        return false;
    }

    uint8_t buffer[MAX_MESSAGE_SIZE];
    int len = protocol_create_chat_message(buffer, client->username, room, message);
    if (len < 0) {
        printf("Failed to create chat message\n");
        return false;
    }

    ssize_t sent = send(client->socket_fd, buffer, len, 0);
    if (sent < 0) {
        printf("Failed to send chat message\n");
        client->connected = false;
        return false;
    }

    return true;
}

bool send_command(SimpleClient *client, const char *command) {
    if (client == NULL || !client->connected) {
        return false;
    }

    uint8_t buffer[MAX_MESSAGE_SIZE];
    int len = protocol_create_command_message(buffer, command);
    if (len < 0) {
        printf("Failed to create command message\n");
        return false;
    }

    ssize_t sent = send(client->socket_fd, buffer, len, 0);
    if (sent < 0) {
        printf("Failed to send command\n");
        client->connected = false;
        return false;
    }
    return true;
}

bool check_for_messages(SimpleClient *client, ParsedMessage *msg_out) {
    if (client == NULL || !client->connected || msg_out == NULL) {
        return false;
    }

    uint8_t temp_buffer[MAX_MESSAGE_SIZE];
    ssize_t bytes = recv(client->socket_fd, temp_buffer, sizeof(temp_buffer), 0);

    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return false;
        }
        printf("Connection error\n");
        client->connected = false;
        return false;
    }

    if (bytes == 0) {
        printf("Server disconnected\n");
        client->connected = false;
        return false;
    }

    // Accumulate bytes in client buffer
    if (client->buffer_pos + bytes > MAX_MESSAGE_SIZE) {
        printf("ERROR: Buffer overflow! buffer_pos=%lu + bytes=%zd > MAX=%lu\n",
               client->buffer_pos, bytes, MAX_MESSAGE_SIZE);
        client->buffer_pos = 0;
        return false;
    }

    memcpy(client->recv_buffer + client->buffer_pos, temp_buffer, bytes);
    client->buffer_pos += bytes;

    // Try to parse a complete message
    if (client->buffer_pos < sizeof(MessageHeader)) {
        return false; // Not enough data for header yet
    }

    // Parse header
    MessageHeader header;
    if (!protocol_parse_header(client->recv_buffer, client->buffer_pos, &header)) {
        printf("ERROR: Invalid protocol header\n");
        client->buffer_pos = 0;
        return false;
    }

    // Check if we have complete message
    size_t total_msg_size = sizeof(MessageHeader) + header.content_len;

    if (client->buffer_pos < total_msg_size) {
        return false;
    }

    // Parse message based on type
    msg_out->type = header.type;
    bool success = false;

    success = protocol_get_parsed_message(client->recv_buffer, total_msg_size, msg_out, &header);

    if (success) {
        // Remove processed message from buffer
        size_t remaining = client->buffer_pos - total_msg_size;
        if (remaining > 0) {
            memmove(client->recv_buffer, client->recv_buffer + total_msg_size, remaining);
        }
        client->buffer_pos = remaining;
    } else {
        // Failed to parse, clear buffer
        client->buffer_pos = 0;
    }

    return success;
}

void disconnect_client(SimpleClient *client) {
    if (client != NULL) {
        if (client->socket_fd >= 0) {
            close(client->socket_fd);
        }
        client->connected = false;
    }
}
