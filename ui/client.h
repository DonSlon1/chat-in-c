#ifndef CLIENT_H
#define CLIENT_H

#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>
#include "../common/protocol.h"

// UI Constants
#define SIDEBAR_WIDTH 240
#define HEADER_HEIGHT 48
#define INPUT_HEIGHT 70
#define BUTTON_HEIGHT 32

// Discord-like Colors
#define SIDEBAR_BG (Color){47, 49, 54, 255}
#define HEADER_BG (Color){54, 57, 63, 255}
#define CHAT_BG (Color){54, 57, 63, 255}
#define INPUT_BG (Color){64, 68, 75, 255}
#define INPUT_FIELD_BG (Color){64, 68, 75, 255}
#define MESSAGE_BG (Color){47, 49, 54, 255}
#define ACCENT_COLOR (Color){88, 101, 242, 255}
#define ACTIVE_ROOM_BG (Color){66, 70, 77, 255}
#define HOVER_BG (Color){60, 63, 69, 255}
#define TEXT_PRIMARY (Color){220, 221, 222, 255}
#define TEXT_SECONDARY (Color){142, 146, 151, 255}
#define TEXT_MUTED (Color){114, 118, 125, 255}

typedef enum {
    CHAT_TYPE_ROOM,
    CHAT_TYPE_DM
} ChatType;

typedef struct {
    char name[64];
    ChatType type;
    int unread_count;
    bool active;
    char messages[100][256];
    char timestamps[100][8];
    int message_count;
} ChatRoom;

typedef struct {
    int socket_fd;
    bool connected;
    char username[64];
    uint8_t recv_buffer[MAX_MESSAGE_SIZE];
    size_t buffer_pos;
} SimpleClient;

typedef struct {
    ChatRoom rooms[20];
    int room_count;
    int active_room_index;
    float scroll_offset;
    char online_users[50][MAX_USERNAME_LEN];
    int online_user_count;
} ChatState;


// Network functions
SimpleClient* create_client(void);
bool connect_to_server(SimpleClient *client, const char *ip, int port);
bool send_chat_message(SimpleClient *client, const char *room, const char *message);
bool send_command(SimpleClient *client, const char *command);
bool check_for_messages(SimpleClient *client, ParsedMessage *msg_out);
void disconnect_client(SimpleClient *client);

// Utility functions
void get_current_time(char *buffer, size_t size);

// UI functions
void init_chat_state(ChatState *state);
void draw_sidebar(ChatState *state, int screen_height);
void draw_chat_area(ChatState *state, const SimpleClient *client, int screen_width, int screen_height);
void draw_input_area(char *input, bool *edit_mode, int screen_width, int screen_height);

#endif // CLIENT_H
