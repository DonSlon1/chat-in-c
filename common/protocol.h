#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Chat Protocol Specification v1.0
 *
 * HTTP-inspired protocol with length-prefixed messages to avoid delimiter conflicts.
 * All multi-byte integers are in network byte order (big-endian).
 *
 * Protocol Design:
 * - Version field for future compatibility
 * - Length-prefixed to avoid delimiter conflicts in content
 * - Structured headers separate from body
 * - Binary-safe (can send any content including newlines, colons, etc.)
 */

// Protocol version
#define PROTOCOL_VERSION 1

// Message types (1 byte)
typedef enum {
    MSG_TYPE_CHAT     = 0x01,  // Regular chat message
    MSG_TYPE_SYSTEM   = 0x02,  // System announcement
    MSG_TYPE_ERROR    = 0x03,  // Error message
    MSG_TYPE_USERLIST = 0x04,  // Online users list
    MSG_TYPE_COMMAND  = 0x05,  // Client command
    MSG_TYPE_PING     = 0x06,  // Keep-alive ping
    MSG_TYPE_PONG     = 0x07   // Keep-alive response
} MessageType;

// Maximum field lengths
#define MAX_USERNAME_LEN  32
#define MAX_ROOMNAME_LEN  64
#define MAX_USER_COUNT    50
#define MAX_CONTENT_LEN   2048
#define MAX_MESSAGE_SIZE  (sizeof(MessageHeader) + sizeof(ChatMessage))

/**
 * Message Header Structure (fixed size: 16 bytes)
 *
 * Wire format:
 * +--------+--------+----------+------------+-----------+
 * | Version| Type   | Reserved | Content Len| Timestamp |
 * | 1 byte | 1 byte | 2 bytes  | 4 bytes    | 8 bytes   |
 * +--------+--------+----------+------------+-----------+
 *
 * Followed by variable-length content
 */
typedef struct __attribute__((packed)) {
    uint8_t  version;       // Protocol version (always 1 for now)
    uint8_t  type;          // Message type (MessageType enum)
    uint16_t reserved;      // Reserved for future use (set to 0)
    uint32_t content_len;   // Length of content following header
    uint64_t timestamp;     // Unix timestamp in milliseconds
} MessageHeader;

/**
 * Chat Message Structure
 *
 * Header fields:
 *   - type: MSG_TYPE_CHAT
 *   - content_len: length of entire ChatMessageBody
 *
 * Body format (variable length):
 *   - username: null-terminated string (max MAX_USERNAME_LEN)
 *   - room: null-terminated string (max MAX_ROOMNAME_LEN)
 *   - message: null-terminated string (max MAX_CONTENT_LEN)
 */
typedef struct {
    char username[MAX_USERNAME_LEN];
    char room[MAX_ROOMNAME_LEN];
    char message[MAX_CONTENT_LEN];
} ChatMessage;

/**
 * System Message Structure
 *
 * Header fields:
 *   - type: MSG_TYPE_SYSTEM
 *   - content_len: length of message string + 1 (null terminator)
 */
typedef struct {
    char message[MAX_CONTENT_LEN];
} SystemMessage;

/**
 * Error Message Structure
 *
 * Header fields:
 *   - type: MSG_TYPE_ERROR
 *   - content_len: length of error string + 1 (null terminator)
 */
typedef struct {
    char error[MAX_CONTENT_LEN];
} ErrorMessage;

/**
 * User List Message Structure
 *
 * Header fields:
 *   - type: MSG_TYPE_USERLIST
 *   - content_len: total size of user list data
 *
 * Body format:
 *   - count: 2 bytes (number of users)
 *   - usernames: array of null-terminated strings
 */
typedef struct {
    uint16_t count;
    char usernames[MAX_USER_COUNT][MAX_USERNAME_LEN];  // Max 50 users
} UserListMessage;

/**
 * Command Message Structure
 *
 * Header fields:
 *   - type: MSG_TYPE_COMMAND
 *   - content_len: length of command string + 1
 *
 * Commands start with '/' and include arguments
 */
typedef struct {
    char command[MAX_CONTENT_LEN];
} CommandMessage;


/**
 * General Parsed Message Structure
 */
typedef struct {
    MessageType type;
    union {
        ChatMessage chat;
        SystemMessage system;
        ErrorMessage error;
        UserListMessage userlist;
    };
} ParsedMessage;

// ============================================================================
// Protocol Helper Functions
// ============================================================================

/**
 * Get current timestamp in milliseconds
 * @return Unix timestamp in milliseconds
 */
uint64_t protocol_get_timestamp(void);

/**
 * Create and serialize a chat message
 * @param buffer Output buffer (must be at least MAX_MESSAGE_SIZE bytes)
 * @param username Sender username
 * @param room Room name
 * @param message Message content
 * @return Total bytes written, or -1 on error
 */
int protocol_create_chat_message(uint8_t *buffer, const char *username,
                                  const char *room, const char *message);

/**
 * Create and serialize a system message
 * @param buffer Output buffer (must be at least MAX_MESSAGE_SIZE bytes)
 * @param message System message content
 * @return Total bytes written, or -1 on error
 */
int protocol_create_system_message(uint8_t *buffer, const char *message);

/**
 * Create and serialize an error message
 * @param buffer Output buffer (must be at least MAX_MESSAGE_SIZE bytes)
 * @param error Error description
 * @return Total bytes written, or -1 on error
 */
int protocol_create_error_message(uint8_t *buffer, const char *error);

/**
 * Create and serialize a user list message
 * @param buffer Output buffer (must be at least MAX_MESSAGE_SIZE bytes)
 * @param usernames Array of username strings
 * @param count Number of usernames
 * @return Total bytes written, or -1 on error
 */
int protocol_create_userlist_message(uint8_t *buffer, const char **usernames,
                                      uint16_t count);

/**
 * Create and serialize a command message
 * @param buffer Output buffer (must be at least MAX_MESSAGE_SIZE bytes)
 * @param command Command string (should start with '/')
 * @return Total bytes written, or -1 on error
 */
int protocol_create_command_message(uint8_t *buffer, const char *command);

/**
 * Parse message header from received data
 * @param data Raw data buffer
 * @param len Length of data
 * @param header Output header structure
 * @return true if valid header parsed, false otherwise
 */
bool protocol_parse_header(const uint8_t *data, size_t len, MessageHeader *header);

/**
 * Parse a complete message into a ParsedMessage structure
 * @param data Raw data buffer (including header)
 * @param len Length of data
 * @param msg_out Output parsed message structure
 * @param header Output message header structure
 * @return true if successfully parsed, false otherwise
 */
bool protocol_get_parsed_message(const uint8_t *data, size_t len, ParsedMessage *msg_out, MessageHeader *header);

/**
 * Parse a complete chat message
 * @param data Raw data buffer (including header)
 * @param len Length of data
 * @param msg Output chat message structure
 * @return true if successfully parsed, false otherwise
 */
bool protocol_parse_chat_message(const uint8_t *data, size_t len, ChatMessage *msg);

/**
 * Parse a system message
 * @param data Raw data buffer (including header)
 * @param len Length of data
 * @param msg Output system message structure
 * @return true if successfully parsed, false otherwise
 */
bool protocol_parse_system_message(const uint8_t *data, size_t len, SystemMessage *msg);

/**
 * Parse an error message
 * @param data Raw data buffer (including header)
 * @param len Length of data
 * @param msg Output error message structure
 * @return true if successfully parsed, false otherwise
 */
bool protocol_parse_error_message(const uint8_t *data, size_t len, ErrorMessage *msg);

/**
 * Parse a user list message
 * @param data Raw data buffer (including header)
 * @param len Length of data
 * @param msg Output user list structure
 * @return true if successfully parsed, false otherwise
 */
bool protocol_parse_userlist_message(const uint8_t *data, size_t len, UserListMessage *msg);

/**
 * Parse a command message
 * @param data Raw data buffer (including header)
 * @param len Length of data
 * @param msg Output command structure
 * @return true if successfully parsed, false otherwise
 */
bool protocol_parse_command_message(const uint8_t *data, size_t len, CommandMessage *msg);

/**
 * Check if a string is a command (starts with '/')
 * @param message The message to check
 * @return true if message is a command, false otherwise
 */
static inline bool protocol_is_command(const char *message) {
    return message != NULL && message[0] == '/';
}

/**
 * Validate protocol version compatibility
 * @param version Protocol version from received message
 * @return true if compatible, false if version mismatch
 */
static inline bool protocol_version_compatible(uint8_t version) {
    return version == PROTOCOL_VERSION;
}

/**
 * Get human-readable message type name
 * @param type Message type
 * @return String representation of message type
 */
const char* protocol_get_type_name(MessageType type);

#endif // PROTOCOL_H
