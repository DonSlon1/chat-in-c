#include "protocol.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>

// ============================================================================
// Helper Functions
// ============================================================================

uint64_t protocol_get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

const char* protocol_get_type_name(MessageType type) {
    switch (type) {
        case MSG_TYPE_CHAT:     return "CHAT";
        case MSG_TYPE_SYSTEM:   return "SYSTEM";
        case MSG_TYPE_ERROR:    return "ERROR";
        case MSG_TYPE_USERLIST: return "USERLIST";
        case MSG_TYPE_COMMAND:  return "COMMAND";
        case MSG_TYPE_PING:     return "PING";
        case MSG_TYPE_PONG:     return "PONG";
        default:                return "UNKNOWN";
    }
}

// ============================================================================
// Message Creation Functions
// ============================================================================

static void write_header(uint8_t *buffer, uint8_t type, uint32_t content_len) {
    MessageHeader *header = (MessageHeader *)buffer;
    header->version = PROTOCOL_VERSION;
    header->type = type;
    header->reserved = 0;
    header->content_len = htonl(content_len);  // Convert to network byte order
    header->timestamp = htobe64(protocol_get_timestamp());  // Convert to big-endian
}

int protocol_create_chat_message(uint8_t *buffer, const char *username,
                                  const char *room, const char *message) {
    if (!buffer || !username || !room || !message) return -1;

    // Validate lengths
    if (strlen(username) >= MAX_USERNAME_LEN ||
        strlen(room) >= MAX_ROOMNAME_LEN ||
        strlen(message) >= MAX_CONTENT_LEN) {
        return -1;
    }

    ChatMessage msg = {0};
    strncpy(msg.username, username, MAX_USERNAME_LEN - 1);
    strncpy(msg.room, room, MAX_ROOMNAME_LEN - 1);
    strncpy(msg.message, message, MAX_CONTENT_LEN - 1);

    uint32_t content_len = sizeof(ChatMessage);
    write_header(buffer, MSG_TYPE_CHAT, content_len);
    memcpy(buffer + sizeof(MessageHeader), &msg, sizeof(ChatMessage));

    return sizeof(MessageHeader) + content_len;
}

int protocol_create_system_message(uint8_t *buffer, const char *message) {

    if (strlen(message) >= MAX_CONTENT_LEN) {
        return -1;
        }

    SystemMessage msg = {0};
    strncpy(msg.message, message, MAX_CONTENT_LEN - 1);

    uint32_t content_len = sizeof(SystemMessage);
    write_header(buffer, MSG_TYPE_SYSTEM, content_len);
    memcpy(buffer + sizeof(MessageHeader), &msg, sizeof(SystemMessage));

    return sizeof(MessageHeader) + content_len;
}

int protocol_create_error_message(uint8_t *buffer, const char *error) {

    if (strlen(error) >= MAX_CONTENT_LEN) {
        return -1;
    }

    ErrorMessage msg = {0};
    strncpy(msg.error, error, MAX_CONTENT_LEN - 1);

    uint32_t content_len = sizeof(ErrorMessage);
    write_header(buffer, MSG_TYPE_ERROR, content_len);
    memcpy(buffer + sizeof(MessageHeader), &msg, sizeof(ErrorMessage));

    return sizeof(MessageHeader) + content_len;
}

int protocol_create_userlist_message(uint8_t *buffer, const char **usernames,uint16_t count) {
    if (count >= MAX_USER_COUNT) {
        return -1;
    }

    for (int i = 0; i < count; i++) {
        if (strlen(usernames[i]) >= MAX_USERNAME_LEN) {
            return -1;
        }
    }

    UserListMessage msg = {0};
    for (int i = 0; i < count; i++) {
        strcpy(msg.usernames[i],usernames[i]);
    }
    msg.count = htons(count);

    uint32_t content_len = sizeof(UserListMessage);
    write_header(buffer, MSG_TYPE_USERLIST, content_len);
    memcpy(buffer + sizeof(MessageHeader), &msg, sizeof(UserListMessage));

    return sizeof(MessageHeader) + content_len;
}

int protocol_create_command_message(uint8_t *buffer, const char *command) {
    if (strlen(command) >= MAX_CONTENT_LEN) {
        return -1;
    }

    CommandMessage msg = {0};
    strncpy(msg.command, command, MAX_CONTENT_LEN - 1);

    uint32_t content_len = sizeof(CommandMessage);
    write_header(buffer, MSG_TYPE_COMMAND, content_len);
    memcpy(buffer + sizeof(MessageHeader), &msg, sizeof(CommandMessage));

    return sizeof(MessageHeader) + content_len;
}

// ============================================================================
// Message Parsing Functions
// ============================================================================

bool protocol_parse_header(const uint8_t *data, size_t len, MessageHeader *header) {
    if (!data || !header) return false;

    if (len < sizeof(MessageHeader)) {
        return false;
    }

    memcpy(header, data, sizeof(MessageHeader));

    header->content_len = ntohl(header->content_len);
    header->timestamp = be64toh(header->timestamp);

    if (header->version != PROTOCOL_VERSION) {
        return false;
    }

    return true;
}

bool protocol_get_parsed_message(const uint8_t *data, size_t len, ParsedMessage *msg_out, MessageHeader *header) {
    bool success = false;
    switch (header->type) {
        case MSG_TYPE_CHAT:
            printf("DEBUG RECV: Parsing CHAT message\n");
            success = protocol_parse_chat_message(data, len, &msg_out->chat);
            if (success) {
                printf("DEBUG RECV: CHAT parsed - user='%s', room='%s', msg='%s'\n",
                       msg_out->chat.username, msg_out->chat.room, msg_out->chat.message);
            }
            break;

        case MSG_TYPE_SYSTEM:
            printf("DEBUG RECV: Parsing SYSTEM message\n");
            success = protocol_parse_system_message(data, len, &msg_out->system);
            if (success) {
                printf("DEBUG RECV: SYSTEM parsed - msg='%s'\n", msg_out->system.message);
            }
            break;

        case MSG_TYPE_ERROR:
            printf("DEBUG RECV: Parsing ERROR message\n");
            success = protocol_parse_error_message(data, len, &msg_out->error);
            if (success) {
                printf("DEBUG RECV: ERROR parsed - msg='%s'\n", msg_out->error.error);
            }
            break;

        case MSG_TYPE_USERLIST:
            printf("DEBUG RECV: Parsing USERLIST message\n");
            success = protocol_parse_userlist_message(data, len, &msg_out->userlist);
            if (success) {
                printf("DEBUG RECV: USERLIST parsed - count=%d\n", msg_out->userlist.count);
            }
            break;

        default:
            printf("WARNING: Unknown message type 0x%02x\n", header->type);
            success = false;
            break;
    }
    return success;
}

bool protocol_parse_chat_message(const uint8_t *data, size_t len, ChatMessage *msg) {
    if (!data || !msg) return false;

    if (len < sizeof(MessageHeader) + sizeof(ChatMessage)) {
        return false;
    }

    memcpy(msg, data + sizeof(MessageHeader), sizeof(ChatMessage));

    return true;
}

bool protocol_parse_system_message(const uint8_t *data, size_t len, SystemMessage *msg) {
    if (!data || !msg) return false;

    if (len < sizeof(MessageHeader) + sizeof(SystemMessage)) {
        return false;
    }

    memcpy(msg, data + sizeof(MessageHeader), sizeof(SystemMessage));

    return true;
}

bool protocol_parse_error_message(const uint8_t *data, size_t len, ErrorMessage *msg) {
    if (!data || !msg) return false;

    if (len < sizeof(MessageHeader) + sizeof(ErrorMessage)) {
        return false;
    }

    memcpy(msg, data + sizeof(MessageHeader), sizeof(ErrorMessage));

    return true;
}

bool protocol_parse_userlist_message(const uint8_t *data, size_t len, UserListMessage *msg) {
    if (!data || !msg) return false;

    if (len < sizeof(MessageHeader) + sizeof(UserListMessage)) {
        return false;
    }

    memcpy(msg, data + sizeof(MessageHeader), sizeof(UserListMessage));
    msg->count = ntohs(msg->count);

    return true;
}

bool protocol_parse_command_message(const uint8_t *data, size_t len, CommandMessage *msg) {
    if (!data || !msg) return false;

    if (len < sizeof(MessageHeader) + sizeof(CommandMessage)) {
        return false;
    }

    memcpy(msg, data + sizeof(MessageHeader), sizeof(CommandMessage));

    return true;
}
