#include "client.h"
#include "raygui.h"
#include <stdio.h>
#include <string.h>

void init_chat_state(ChatState *state) {
    state->room_count = 0;
    state->active_room_index = 0;
    state->scroll_offset = 0.0f;

    // Add some default rooms
    strcpy(state->rooms[state->room_count].name, "# general");
    state->rooms[state->room_count].type = CHAT_TYPE_ROOM;
    state->rooms[state->room_count].unread_count = 0;
    state->rooms[state->room_count].active = true;
    state->rooms[state->room_count].message_count = 0;
    state->room_count++;

    strcpy(state->rooms[state->room_count].name, "# random");
    state->rooms[state->room_count].type = CHAT_TYPE_ROOM;
    state->rooms[state->room_count].unread_count = 0;
    state->rooms[state->room_count].active = false;
    state->rooms[state->room_count].message_count = 0;
    state->room_count++;

    strcpy(state->rooms[state->room_count].name, "# help");
    state->rooms[state->room_count].type = CHAT_TYPE_ROOM;
    state->rooms[state->room_count].unread_count = 0;
    state->rooms[state->room_count].active = false;
    state->rooms[state->room_count].message_count = 0;
    state->room_count++;
}

void draw_sidebar(ChatState *state, int screen_height) {
    // Sidebar background
    DrawRectangle(0, 0, SIDEBAR_WIDTH, screen_height, SIDEBAR_BG);

    // Server name header with shadow
    DrawRectangle(0, 0, SIDEBAR_WIDTH, HEADER_HEIGHT, HEADER_BG);
    DrawRectangle(0, HEADER_HEIGHT - 1, SIDEBAR_WIDTH, 1, (Color){0, 0, 0, 80});
    DrawText("My Server", 16, 12, 26, TEXT_PRIMARY);

    // Section labels
    int y_pos = HEADER_HEIGHT + 16;
    DrawText("TEXT CHANNELS", 16, y_pos, 16, TEXT_MUTED);
    y_pos += 28;

    // Draw room buttons
    Vector2 mouse_pos = GetMousePosition();
    for (int i = 0; i < state->room_count; i++) {
        if (state->rooms[i].type != CHAT_TYPE_ROOM) continue;

        Rectangle room_rect = {8, (float)y_pos, SIDEBAR_WIDTH - 16, BUTTON_HEIGHT};
        bool is_hovered = CheckCollisionPointRec(mouse_pos, room_rect);
        bool is_active = (i == state->active_room_index);

        // Draw button background (Discord style)
        if (is_active) {
            DrawRectangleRounded(room_rect, 0.12f, 8, ACTIVE_ROOM_BG);
        } else if (is_hovered) {
            DrawRectangleRounded(room_rect, 0.12f, 8, HOVER_BG);
        }

        // Draw room name
        DrawText(state->rooms[i].name, 20, y_pos + 2, 24,
                is_active ? TEXT_PRIMARY : (is_hovered ? TEXT_SECONDARY : TEXT_MUTED));

        // Draw unread badge if needed
        if (state->rooms[i].unread_count > 0) {
            char badge[10];
            snprintf(badge, sizeof(badge), "%d", state->rooms[i].unread_count);
            int badge_width = MeasureText(badge, 12) + 10;
            DrawRectangleRounded(
                (Rectangle){SIDEBAR_WIDTH - badge_width - 10, (float)y_pos + 10, (float)badge_width, 18},
                0.5f, 8, RED
            );
            DrawText(badge, SIDEBAR_WIDTH - badge_width - 5, y_pos + 12, 12, WHITE);
        }

        // Handle click
        if (is_hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state->rooms[state->active_room_index].active = false;
            state->active_room_index = i;
            state->rooms[i].active = true;
            state->rooms[i].unread_count = 0;
            state->scroll_offset = 0.0f;
        }

        y_pos += BUTTON_HEIGHT + 5;
    }

    // Direct Messages section
    y_pos += 16;
    DrawText("DIRECT MESSAGES", 16, y_pos, 16, TEXT_MUTED);
    y_pos += 28;

    // Draw existing DM conversations
    for (int i = 0; i < state->room_count; i++) {
        if (state->rooms[i].type != CHAT_TYPE_DM) continue;

        Rectangle dm_rect = {8, (float)y_pos, SIDEBAR_WIDTH - 16, BUTTON_HEIGHT};
        bool is_hovered = CheckCollisionPointRec(mouse_pos, dm_rect);
        bool is_active = (i == state->active_room_index);

        // Draw button background
        if (is_active) {
            DrawRectangleRounded(dm_rect, 0.12f, 8, ACTIVE_ROOM_BG);
        } else if (is_hovered) {
            DrawRectangleRounded(dm_rect, 0.12f, 8, HOVER_BG);
        }

        // Draw DM name (with @ prefix)
        DrawText(state->rooms[i].name, 20, y_pos + 2, 24,
                is_active ? TEXT_PRIMARY : (is_hovered ? TEXT_SECONDARY : TEXT_MUTED));

        // Draw unread badge
        if (state->rooms[i].unread_count > 0) {
            char badge[10];
            snprintf(badge, sizeof(badge), "%d", state->rooms[i].unread_count);
            int badge_width = MeasureText(badge, 12) + 10;
            DrawRectangleRounded(
                (Rectangle){SIDEBAR_WIDTH - badge_width - 10, (float)y_pos + 10, (float)badge_width, 18},
                0.5f, 8, RED
            );
            DrawText(badge, SIDEBAR_WIDTH - badge_width - 5, y_pos + 12, 12, WHITE);
        }

        // Handle click
        if (is_hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state->rooms[state->active_room_index].active = false;
            state->active_room_index = i;
            state->rooms[i].active = true;
            state->rooms[i].unread_count = 0;
            state->scroll_offset = 0.0f;
        }

        y_pos += BUTTON_HEIGHT + 5;
    }
}

void draw_chat_area(ChatState *state, const SimpleClient *client, int screen_width, int screen_height) {
    // TODO PHASE 3.2: Reserve space for user list on right side
    // const int userlist_width = 200;
    // const int chat_width = screen_width - SIDEBAR_WIDTH - userlist_width;
    const int chat_x = SIDEBAR_WIDTH;
    const int chat_width = screen_width - SIDEBAR_WIDTH;

    // Header with shadow
    DrawRectangle(chat_x, 0, chat_width, HEADER_HEIGHT, HEADER_BG);
    DrawRectangle(chat_x, HEADER_HEIGHT - 1, chat_width, 1, (Color){0, 0, 0, 80});

    if (state->active_room_index >= 0 && state->active_room_index < state->room_count) {
        DrawText("#", chat_x + 16, 11, 28, TEXT_MUTED);
        DrawText(state->rooms[state->active_room_index].name + 2, chat_x + 40, 13, 24, TEXT_PRIMARY);
    }

    // Connection status
    const char *status = client && client->connected ? "●" : "●";
    const Color status_color = client && client->connected ?
        (Color){59, 165, 93, 255} : (Color){237, 66, 69, 255};
    DrawText(status, chat_x + chat_width - 80, 12, 32, status_color);
    DrawText(client && client->connected ? "Online" : "Offline",
             chat_x + chat_width - 60, 16, 18, TEXT_SECONDARY);

    // Messages area
    const int msg_area_y = HEADER_HEIGHT;
    const int msg_area_height = screen_height - HEADER_HEIGHT - INPUT_HEIGHT;
    DrawRectangle(chat_x, msg_area_y, chat_width, msg_area_height, CHAT_BG);

    const ChatRoom *current_room = &state->rooms[state->active_room_index];
    const int message_height = 95;

    // Calculate scrolling bounds
    const int total_content_height = current_room->message_count * message_height;
    int max_scroll = total_content_height - msg_area_height + 40;
    if (max_scroll < 0) max_scroll = 0;

    if (state->scroll_offset < 0) state->scroll_offset = 0;
    if (state->scroll_offset > (float)max_scroll) state->scroll_offset = (float)max_scroll;

    int y_offset = msg_area_y + 20 - (int)state->scroll_offset;

    // Draw messages
    for (int i = 0; i < current_room->message_count; i++) {
        const int avatar_size = 50;

        // Skip messages above visible area
        if (y_offset + message_height < msg_area_y) {
            y_offset += message_height;
            continue;
        }

        // Stop drawing messages below visible area
        if (y_offset > msg_area_y + msg_area_height) {
            break;
        }

        // Message hover background
        const Vector2 mouse_pos = GetMousePosition();
        const Rectangle msg_hover_rect = {(float)chat_x, (float)y_offset - 2, (float)chat_width, 68};
        const bool msg_hovered = CheckCollisionPointRec(mouse_pos, msg_hover_rect);
        if (msg_hovered) {
            DrawRectangle(chat_x, y_offset - 2, chat_width, 68, (Color){46, 48, 54, 255});
        }

        // Avatar
        DrawCircle(chat_x + 32, y_offset + 20, (float)avatar_size / 2, ACCENT_COLOR);
        DrawCircle(chat_x + 32, y_offset + 20, (float)avatar_size / 2 - 2, (Color){120, 130, 250, 255});
        DrawText("U", chat_x + 24, y_offset + 7, 26, WHITE);

        // Message content
        int msg_x = chat_x + 72;
        DrawText("User", msg_x, y_offset, 22, TEXT_PRIMARY);
        DrawText(current_room->timestamps[i], msg_x + 70, y_offset + 2, 16, TEXT_MUTED);
        DrawText(current_room->messages[i], msg_x, y_offset + 28, 20, TEXT_SECONDARY);

        y_offset += message_height;
    }

    // TODO PHASE 3.2: Draw user list on right side
    // const int userlist_x = screen_width - userlist_width;
    // DrawRectangle(userlist_x, 0, userlist_width, screen_height - INPUT_HEIGHT, SIDEBAR_BG);
    // DrawText("Members", userlist_x + 15, 15, 18, TEXT_PRIMARY);
    //
    // int user_y = 50;
    // for (int i = 0; i < state->online_user_count; i++) {
    //     // Draw online indicator (green dot)
    //     DrawCircle(userlist_x + 20, user_y + 10, 5, GREEN);
    //     // Draw username
    //     DrawText(state->online_users[i], userlist_x + 35, user_y, 18, TEXT_SECONDARY);
    //
    //     // TODO: Make usernames clickable to start DM
    //     // Vector2 mouse = GetMousePosition();
    //     // Rectangle user_rect = {userlist_x, user_y - 5, userlist_width, 30};
    //     // if (CheckCollisionPointRec(mouse, user_rect)) {
    //     //     DrawRectangle(userlist_x, user_y - 5, userlist_width, 30, HOVER_BG);
    //     //     if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    //     //         // Start DM with this user
    //     //     }
    //     // }
    //
    //     user_y += 35;
    // }
}

void draw_input_area(char *input, bool *edit_mode, int screen_width, int screen_height) {
    const int input_y = screen_height - INPUT_HEIGHT;
    const int chat_x = SIDEBAR_WIDTH;

    DrawRectangle(chat_x, input_y, screen_width - chat_x, INPUT_HEIGHT, CHAT_BG);

    // Send button
    const int send_btn_width = 80;
    const int send_btn_height = 44;
    const Rectangle send_btn = {
        (float)(screen_width - send_btn_width - 24),
        (float)(input_y + 13),
        (float)send_btn_width,
        (float)send_btn_height
    };

    const Vector2 mouse_pos = GetMousePosition();
    const bool btn_hovered = CheckCollisionPointRec(mouse_pos, send_btn);
    const Color btn_color = btn_hovered ? (Color){78, 84, 200, 255} : ACCENT_COLOR;
    DrawRectangleRounded(send_btn, 0.08f, 8, btn_color);
    DrawText("Send", (int)send_btn.x + 18, (int)send_btn.y + 10, 24, WHITE);

    // Input box
    const int input_box_width = screen_width - chat_x - send_btn_width - 56;
    const Rectangle input_rect = {
        (float)(chat_x + 16),
        (float)(input_y + 13),
        (float)input_box_width,
        44.0f
    };
    DrawRectangleRounded(input_rect, 0.10f, 8, INPUT_FIELD_BG);

    if (GuiTextBox(input_rect, input, 256, *edit_mode)) {
        *edit_mode = !*edit_mode;
    }
}
