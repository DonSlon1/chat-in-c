#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <asm-generic/errno-base.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define DEFAULT_CLIENT_COUNT 16

typedef struct {
    int fd;
    char username[64];
    char recv_buffer[BUFFER_SIZE];
    int buffer_pos;
} Client;

volatile sig_atomic_t running = 1;
static Client *global_clients = NULL;
static int global_client_count = 0;
static int global_server_fd = -1;

void cleanup_resources(void);
void set_nonblocking(int fd);
void remove_client(int index);
void broadcast_message(const char *message, int sender_index);

int main(void) {
    int server_fd;
    int max_client_count = DEFAULT_CLIENT_COUNT;

    global_clients = malloc(sizeof(Client) * max_client_count);
    if (global_clients == NULL) {
        perror("Failed to allocate initial client array");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("Error: Socket is not created\n");
        exit(EXIT_FAILURE);
    }
    global_server_fd = server_fd;

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error: bind was not successful");
        exit(EXIT_FAILURE);
    }
    printf("Bind successful, start listening on port %d\n", PORT);

    listen(server_fd, 10);
    set_nonblocking(server_fd);

    while (running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;

        for (int i = 0; i < global_client_count; i++) {
            FD_SET(global_clients[i].fd, &read_fds);
            if (global_clients[i].fd > max_fd) {
                max_fd = global_clients[i].fd;
            }
        }

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if (activity < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("select error");
            break;
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            while (1) {
                int client_fd = accept(server_fd, NULL, NULL);

                if (client_fd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    }
                    perror("accept error");
                    break;
                }

                set_nonblocking(client_fd);

                if (global_client_count >= max_client_count) {
                    max_client_count *= 2;
                    Client *new_clients = realloc(global_clients, sizeof(Client) * max_client_count);

                    if (new_clients == NULL) {
                        perror("error while reallocating memory");
                        close(client_fd);
                        continue;
                    }
                    global_clients = new_clients;
                }

                global_clients[global_client_count].fd = client_fd;
                global_clients[global_client_count].username[0] = '\0';
                global_clients[global_client_count].recv_buffer[0] = '\0';
                global_clients[global_client_count].buffer_pos = 0;

                global_client_count++;
                printf("New client connected: fd=%d (total clients: %d)\n",
                       client_fd, global_client_count);
            }
        }

        for (int i = 0; i < global_client_count; i++) {
            if (FD_ISSET(global_clients[i].fd, &read_fds)) {
                char temp_buffer[BUFFER_SIZE];
                ssize_t bytes = recv(global_clients[i].fd, temp_buffer, BUFFER_SIZE - 1, 0);

                if (bytes > 0) {
                    for (int j = 0; j < bytes; j++) {
                        char c = temp_buffer[j];

                        if (global_clients[i].buffer_pos < BUFFER_SIZE - 1) {
                            global_clients[i].recv_buffer[global_clients[i].buffer_pos++] = c;
                        }

                        if (c == '\n') {
                            global_clients[i].recv_buffer[global_clients[i].buffer_pos - 1] = '\0';

                            printf("Client %d sent: %s\n",
                                   global_clients[i].fd,
                                   global_clients[i].recv_buffer);

                            if (global_clients[i].username[0] == '\0') {
                                strncpy(global_clients[i].username,
                                        global_clients[i].recv_buffer,
                                        sizeof(global_clients[i].username) - 1);
                                printf("Client %d set username: %s\n",
                                       global_clients[i].fd,
                                       global_clients[i].username);

                                char announce[BUFFER_SIZE];
                                snprintf(announce, BUFFER_SIZE, "%s joined the chat\n",
                                         global_clients[i].username);
                                broadcast_message(announce, i);
                            } else {
                                char formatted[BUFFER_SIZE];
                                snprintf(formatted, BUFFER_SIZE, "%s: %s\n",
                                         global_clients[i].username,
                                         global_clients[i].recv_buffer);
                                broadcast_message(formatted, i);
                            }

                            global_clients[i].buffer_pos = 0;
                        }
                    }
                } else if (bytes == 0) {
                    printf("Client %d (%s) disconnected\n",
                           global_clients[i].fd,
                           global_clients[i].username[0] ? global_clients[i].username : "unknown");

                    if (global_clients[i].username[0] != '\0') {
                        char announce[BUFFER_SIZE];
                        snprintf(announce, BUFFER_SIZE, "%s left the chat\n",
                                 global_clients[i].username);
                        broadcast_message(announce, i);
                    }

                    close(global_clients[i].fd);
                    remove_client(i);
                    i--;

                } else {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("recv error");
                        close(global_clients[i].fd);
                        remove_client(i);
                        i--;
                    }
                }
            }
        }
    }

    printf("Server shutting down...\n");
    cleanup_resources();
    return 0;
}

void broadcast_message(const char *message, int sender_index) {
    for (int j = 0; j < global_client_count; j++) {
        if (j != sender_index) {
            ssize_t sent = send(global_clients[j].fd, message, strlen(message), MSG_NOSIGNAL);
            if (sent < 0) {
                printf("Failed to send to client %d\n", global_clients[j].fd);
            }
        }
    }
}

void remove_client(int index) {
    if (index < 0 || index >= global_client_count) {
        return;
    }

    memmove(&global_clients[index],
            &global_clients[index + 1],
            (global_client_count - index - 1) * sizeof(Client));

    global_client_count--;
}

void set_nonblocking(const int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
    }
}

void cleanup_resources(void) {
    if (global_clients != NULL) {
        for (int i = 0; i < global_client_count; i++) {
            close(global_clients[i].fd);
        }
        free(global_clients);
        global_clients = NULL;
    }
    if (global_server_fd >= 0) {
        close(global_server_fd);
        global_server_fd = -1;
    }
}
