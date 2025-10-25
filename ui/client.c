#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

typedef struct {
    int socket_fd;
    bool connected;
    char username[64];
    char recv_buffer[512];
    int buffer_pos;
} SimpleClient;

SimpleClient* create_client() {
    SimpleClient *client = malloc(sizeof(SimpleClient));
    client->socket_fd = -1;
    client->connected = false;
    client->username[0] = '\0';
    client->recv_buffer[0] = '\0';
    client->buffer_pos = 0;
    return client;
}

bool connect_to_server(SimpleClient *client, const char *ip, int port) {
    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0) {
        printf("Failed to create socket\n");
        return false;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (connect(client->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Failed to connect\n");
        close(client->socket_fd);
        return false;
    }

    fcntl(client->socket_fd, F_SETFL, O_NONBLOCK);

    client->connected = true;
    printf("Connected to %s:%d\n", ip, port);
    return true;
}

bool send_message(SimpleClient *client, const char *message) {
    if (!client->connected) {
        return false;
    }

    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s\n", message);

    ssize_t sent = send(client->socket_fd, buffer, strlen(buffer), 0);

    if (sent < 0) {
        printf("Failed to send\n");
        client->connected = false;
        return false;
    }

    return true;
}

char* check_for_messages(SimpleClient *client) {
    if (!client->connected) {
        return NULL;
    }

    static char complete_message[512];
    char temp_buffer[512];

    ssize_t bytes = recv(client->socket_fd, temp_buffer, sizeof(temp_buffer) - 1, 0);

    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return NULL;
        }
        printf("Connection error\n");
        client->connected = false;
        return NULL;
    }

    if (bytes == 0) {
        printf("Server disconnected\n");
        client->connected = false;
        return NULL;
    }

    for (int i = 0; i < bytes; i++) {
        char c = temp_buffer[i];

        if (client->buffer_pos < sizeof(client->recv_buffer) - 1) {
            client->recv_buffer[client->buffer_pos++] = c;
        }

        if (c == '\n') {
            client->recv_buffer[client->buffer_pos - 1] = '\0';
            strncpy(complete_message, client->recv_buffer, sizeof(complete_message) - 1);
            client->buffer_pos = 0;
            return complete_message;
        }
    }

    return NULL;
}

void disconnect_client(SimpleClient *client) {
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
    }
    client->connected = false;
}

int main() {
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    InitWindow(800, 600, "Simple Chat Client");
    SetTargetFPS(60);

    char messageInput[256] = "\0";
    bool editMode = false;

    char chatHistory[50][256];
    int messageCount = 0;

    SimpleClient *client = NULL;
    bool show_connect_dialog = true;
    char server_ip[64] = "127.0.0.1\0";
    char username[64] = "Alice\0";

    while (!WindowShouldClose()) {

        if (client && client->connected) {
            char *new_message = check_for_messages(client);

            if (new_message != NULL) {
                if (messageCount < 50) {
                    strncpy(chatHistory[messageCount], new_message, 255);
                    messageCount++;
                }
            }
        }

        bool enterPressed = false;
        if (editMode && IsKeyPressed(KEY_ENTER)) {
            enterPressed = true;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (show_connect_dialog) {
            DrawText("Connect to Chat Server", 250, 150, 24, DARKGRAY);

            DrawText("Server IP:", 250, 200, 20, DARKGRAY);
            GuiTextBox((Rectangle){250, 230, 300, 30}, server_ip, 64, false);

            DrawText("Username:", 250, 280, 20, DARKGRAY);
            GuiTextBox((Rectangle){250, 310, 300, 30}, username, 64, true);

            if (GuiButton((Rectangle){325, 360, 150, 40}, "Connect")) {
                client = create_client();

                if (connect_to_server(client, server_ip, 8080)) {
                    strncpy(client->username, username, 63);
                    send_message(client, username);
                    show_connect_dialog = false;
                } else {
                    free(client);
                    client = NULL;
                }
            }

            if (client == NULL) {
                DrawText("Connection failed!", 300, 420, 18, RED);
            }
        } else {
            if (client && client->connected) {
                DrawText(TextFormat("Connected as: %s", client->username), 10, 5, 18, DARKGREEN);
            } else {
                DrawText("Disconnected - Close and restart", 10, 5, 18, RED);
            }

            DrawRectangle(10, 35, 780, 500, LIGHTGRAY);

            int start_index = (messageCount > 20) ? messageCount - 20 : 0;

            for (int i = start_index; i < messageCount; i++) {
                int y_pos = 40 + (i - start_index) * 24;
                DrawText(chatHistory[i], 15, y_pos, 18, DARKGRAY);
            }

            if (GuiTextBox(
                (Rectangle){ 10, 550, 680, 30 },
                messageInput,
                256,
                editMode
            )) {
                if (!enterPressed) {
                    editMode = !editMode;
                }
            }

            bool sendClicked = GuiButton((Rectangle){ 700, 550, 90, 30 }, "Send");

            if ((enterPressed || sendClicked) && strlen(messageInput) > 0) {
                if (client && client->connected) {
                    send_message(client, messageInput);
                }

                messageInput[0] = '\0';
                editMode = true;
            }
        }

        EndDrawing();
    }

    if (client) {
        disconnect_client(client);
        free(client);
    }

    CloseWindow();
    return 0;
}
