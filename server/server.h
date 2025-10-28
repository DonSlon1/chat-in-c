#pragma once

#include <stdbool.h>
#include <netinet/in.h>
#include "../common/protocol.h"

#define DEFAULT_CLIENT_COUNT 16
#define MAX_ROOMS 10
#define MAX_ROOM_NAME 64

// Represents a chat room
typedef struct {
    char name[MAX_ROOM_NAME];
    int client_count;
    int client_fds[100];  // List of client FDs in this room
} Room;

// Represents a single connected client
typedef struct {
    int fd;
    char username[64];
    uint8_t recv_buffer[MAX_MESSAGE_SIZE];
    size_t buffer_pos;
    char current_room[MAX_ROOM_NAME];
} Client;

// Represents the entire server state
typedef struct {
    int server_fd;
    Client *clients;
    int client_count;
    int client_capacity;
    Room rooms[MAX_ROOMS];
    int room_count;
} Server;

/**
 * @brief Initializes the server, allocates memory, and starts listening.
 * @param server A pointer to the Server struct to initialize.
 * @param port The port to listen on.
 * @return true on success, false on failure.
 */
bool server_init(Server *server, int port);

/**
 * @brief Shuts down the server, closes all sockets, and frees memory.
 * @param server A pointer to the Server struct to clean up.
 */
void server_shutdown(Server *server);

/**
 * @brief Waits for and processes all network events (new connections, messages).
 * @param server A pointer to the Server struct.
 */
void server_poll_events(Server *server);

/**
 * @brief Broadcasts a message to all connected clients (except the sender).
 * @param server A pointer to the Server struct.
 * @param data The message data to send.
 * @param len The length of the message data.
 * @param sender_index The index of the client who sent the message.
 */
void server_broadcast_message(Server *server, const uint8_t *data,const int len, int sender_index);

// --- Static Helper Function Declarations ---
static void set_nonblocking(int fd);
static void server_accept_new_clients(Server *server);
static bool server_handle_client_data(Server *server, int client_index);
static void server_remove_client(Server *server, int client_index);
static void client_process_message(Server *server, int client_index, uint8_t *message, size_t total_message_size);
static void send_error_message(Server *server, int client_index, const char *message);
/**
 * @brief send alle the active users to all clients
 * @param server A pointer to the Server struct.
 */
static void server_broadcast_user_list(Server *server);
