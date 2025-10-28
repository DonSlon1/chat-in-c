#include "client.h"
#include "../common/protocol.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <string.h>

static Font load_system_font(void) {
    Font customFont = {0};

    for (int i = 0; i < 4; i++) {
        const char* fontPaths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/ubuntu/Ubuntu-R.ttf",
            "/usr/share/fonts/liberation/LiberationSans-Regular.ttf"
        };
        if (FileExists(fontPaths[i])) {
            customFont = LoadFontEx(fontPaths[i], 96, 0, 250);
            SetTextureFilter(customFont.texture, TEXTURE_FILTER_BILINEAR);
            break;
        }
    }
    return customFont;
}

static void draw_connect_dialog(int screen_width, int screen_height,
                                char *server_ip, char *username) {
    DrawRectangle(0, 0, screen_width, screen_height, (Color){0, 0, 0, 180});

    float dialog_w = 450;
    float dialog_h = 380;
    float dialog_x = ((float)screen_width - dialog_w) / 2;
    float dialog_y = ((float)screen_height - dialog_h) / 2;

    DrawRectangleRounded((Rectangle){dialog_x, dialog_y, dialog_w, dialog_h}, 0.05f, 8, WHITE);
    DrawText("Connect to Server", (int)dialog_x + 70, (int)dialog_y + 30, 32, DARKGRAY);

    DrawText("Server IP:", (int)dialog_x + 40, (int)dialog_y + 100, 24, DARKGRAY);
    DrawRectangleRounded((Rectangle){dialog_x + 40, dialog_y + 135, 370, 40}, 0.2f, 8, (Color){240, 240, 245, 255});
    GuiTextBox((Rectangle){dialog_x + 40, dialog_y + 135, 370, 40}, server_ip, 64, false);

    DrawText("Username:", (int)dialog_x + 40, (int)dialog_y + 195, 24, DARKGRAY);
    DrawRectangleRounded((Rectangle){dialog_x + 40, dialog_y + 230, 370, 40}, 0.2f, 8, (Color){240, 240, 245, 255});
    GuiTextBox((Rectangle){dialog_x + 40, dialog_y + 230, 370, 40}, username, 64, true);
}

static bool handle_connect_button(float dialog_x, float dialog_y,
                                  SimpleClient **client, const char *server_ip,
                                  const char *username) {
    if (GuiButton((Rectangle){dialog_x + 125, dialog_y + 295, 200, 50}, "Connect")) {
        if (*client != NULL) {
            disconnect_client(*client);
            free(*client);
            *client = NULL;
        }

        *client = create_client();
        if (*client != NULL && connect_to_server(*client, server_ip, 8080)) {
            strncpy((*client)->username, username, 63);
            send_chat_message(*client, "general", username);
            return true;
        }
        if (*client != NULL) {
            free(*client);
            *client = NULL;
        }
    }
    return false;
}

// Helper: Find or create room by name
static ChatRoom* find_or_create_room(ChatState *state, const char *room_name, ChatType type) {
    const char *prefix = (type == CHAT_TYPE_DM) ? "@ " : "# ";

    // Try to find existing room
    for (int i = 0; i < state->room_count; i++) {
        // Room names are stored with prefix (# for rooms, @ for DMs)
        if (strcmp(state->rooms[i].name + 2, room_name) == 0) {
            return &state->rooms[i];
        }
    }

    // Create new room if space available
    if (state->room_count < 20) {
        ChatRoom *new_room = &state->rooms[state->room_count];
        snprintf(new_room->name, 64, "%s%s", prefix, room_name);
        new_room->type = type;
        new_room->unread_count = 0;
        new_room->active = false;
        new_room->message_count = 0;
        state->room_count++;
        return new_room;
    }

    // Fallback to current room if no space
    return &state->rooms[state->active_room_index];
}

// Helper: Check if this is a DM (room field is a username)
static bool is_dm_message(const char *room_field, const char *my_username) {
    // If room field is a username (matches my username or another user), it's a DM
    // We'll detect this by checking if it's NOT a known room type
    // For simplicity: if it doesn't match typical room names, treat as DM
    return (room_field[0] != '\0' && strcmp(room_field, "general") != 0 &&
            strcmp(room_field, "random") != 0 && strcmp(room_field, "help") != 0);
}

static void handle_incoming_messages(SimpleClient *client, ChatState *state) {
    if (client == NULL || !client->connected) {
        return;
    }

    ParsedMessage msg;
    if (check_for_messages(client, &msg)) {
        switch (msg.type) {
            case MSG_TYPE_CHAT: {
                // Check if this is a DM (room field contains username)
                bool is_dm = is_dm_message(msg.chat.room, client->username);

                ChatRoom *target_room;
                if (is_dm) {
                    // For DM, create/find room with the OTHER person's username
                    const char *dm_partner = (strcmp(msg.chat.username, client->username) == 0)
                                           ? msg.chat.room  // I sent it, partner is in "room" field
                                           : msg.chat.username;  // They sent it, partner is username
                    target_room = find_or_create_room(state, dm_partner, CHAT_TYPE_DM);
                } else {
                    // Regular room message
                    target_room = find_or_create_room(state, msg.chat.room, CHAT_TYPE_ROOM);
                }

                if (target_room->message_count < 100) {
                    char formatted[256];
                    snprintf(formatted, 256, "%s: %s", msg.chat.username, msg.chat.message);
                    strncpy(target_room->messages[target_room->message_count], formatted, 255);
                    target_room->messages[target_room->message_count][255] = '\0';
                    get_current_time(target_room->timestamps[target_room->message_count], 8);
                    target_room->message_count++;
                }
                break;
            }

            case MSG_TYPE_SYSTEM: {
                // Check if this is a "Joined room:" message to update UI
                if (strncmp(msg.system.message, "Joined room: ", 13) == 0) {
                    const char *new_room_name = msg.system.message + 13;

                    // Find or create this room and switch to it
                    ChatRoom *new_room = find_or_create_room(state, new_room_name, CHAT_TYPE_ROOM);

                    // Find the index of this room
                    for (int i = 0; i < state->room_count; i++) {
                        if (strcmp(state->rooms[i].name + 2, new_room_name) == 0) {
                            state->active_room_index = i;
                            state->rooms[i].active = true;
                            break;
                        }
                    }
                }

                // System messages go to current room
                ChatRoom *current_room = &state->rooms[state->active_room_index];
                if (current_room->message_count < 100) {
                    char formatted[256];
                    snprintf(formatted, 256, "[System] %s", msg.system.message);
                    strncpy(current_room->messages[current_room->message_count], formatted, 255);
                    current_room->messages[current_room->message_count][255] = '\0';
                    get_current_time(current_room->timestamps[current_room->message_count], 8);
                    current_room->message_count++;
                }
                break;
            }

            case MSG_TYPE_ERROR: {
                // Error messages go to current room
                ChatRoom *current_room = &state->rooms[state->active_room_index];
                if (current_room->message_count < 100) {
                    char formatted[256];
                    snprintf(formatted, 256, "[Error] %s", msg.error.error);
                    strncpy(current_room->messages[current_room->message_count], formatted, 255);
                    current_room->messages[current_room->message_count][255] = '\0';
                    get_current_time(current_room->timestamps[current_room->message_count], 8);
                    current_room->message_count++;
                }
                break;
            }

            case MSG_TYPE_USERLIST: {
                state->online_user_count = 0;
                for (int i = 0; i < msg.userlist.count && i < 50; i++) {
                    strncpy(state->online_users[state->online_user_count],
                           msg.userlist.usernames[i], MAX_USERNAME_LEN - 1);
                    state->online_users[state->online_user_count][MAX_USERNAME_LEN - 1] = '\0';
                    state->online_user_count++;
                }
                break;
            }

            default:
                printf("WARNING: Unhandled message type 0x%02x\n", msg.type);
                break;
        }
    }
}

static void handle_send_message(SimpleClient *client, ChatState *state,
                               char *messageInput, bool enterPressed, bool sendClicked) {
    if ((enterPressed || sendClicked) && strlen(messageInput) > 0) {
        if (client != NULL && client->connected) {
            ChatRoom *current_room = &state->rooms[state->active_room_index];

            if (messageInput[0] == '/') {
                send_command(client, messageInput);
            } else {
                // Room name in UI has "# " prefix, skip it for protocol
                const char *room_name = current_room->name + 2;
                send_chat_message(client, room_name, messageInput);
            }
        }
        messageInput[0] = '\0';
    }
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 650, "Chat Client");
    SetTargetFPS(60);
    SetWindowMinSize(800, 500);

    Font customFont = load_system_font();
    if (customFont.texture.id != 0) {
        GuiSetFont(customFont);
    }
    GuiSetStyle(DEFAULT, TEXT_SIZE, 24);

    ChatState state = {0};
    init_chat_state(&state);

    char messageInput[256] = "\0";
    bool editMode = false;

    SimpleClient *client = NULL;
    bool show_connect_dialog = true;
    char server_ip[64] = "127.0.0.1";
    char username[64] = "User";

    while (!WindowShouldClose()) {
        const int screen_width = GetScreenWidth();
        const int screen_height = GetScreenHeight();

        handle_incoming_messages(client, &state);

        const float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            state.scroll_offset -= wheel * 40.0f;
        }

        const bool enterPressed = editMode && IsKeyPressed(KEY_ENTER);

        BeginDrawing();
        ClearBackground(CHAT_BG);

        if (show_connect_dialog) {
            draw_connect_dialog(screen_width, screen_height, server_ip, username);

            float dialog_w = 450;
            float dialog_h = 380;
            float dialog_x = ((float)screen_width - dialog_w) / 2;
            float dialog_y = ((float)screen_height - dialog_h) / 2;

            if (handle_connect_button(dialog_x, dialog_y, &client, server_ip, username)) {
                show_connect_dialog = false;
            }
        } else {
            draw_sidebar(&state, screen_height);
            draw_chat_area(&state, client, screen_width, screen_height);
            draw_input_area(messageInput, &editMode, screen_width, screen_height);

            const Vector2 mouse_pos = GetMousePosition();
            const int input_y = screen_height - INPUT_HEIGHT;
            const int send_btn_width = 80;
            const int send_btn_height = 44;
            const Rectangle send_btn = {
                (float)(screen_width - send_btn_width - 24),
                (float)(input_y + 13),
                (float)send_btn_width,
                (float)send_btn_height
            };

            bool sendClicked = CheckCollisionPointRec(mouse_pos, send_btn) &&
                              IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

            handle_send_message(client, &state, messageInput, enterPressed, sendClicked);
            if (enterPressed || sendClicked) {
                editMode = true;
            }
        }

        EndDrawing();
    }

    if (client != NULL) {
        disconnect_client(client);
        free(client);
        client = NULL;
    }

    if (customFont.texture.id != 0) {
        UnloadFont(customFont);
    }

    CloseWindow();
    return 0;
}
