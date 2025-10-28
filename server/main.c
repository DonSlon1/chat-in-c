#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "server.h"

#define PORT 8080

// The *only* global variable, for signal handling.
volatile sig_atomic_t running = 1;

/**
 * @brief Handles signals like CTRL+C (SIGINT) to allow graceful shutdown.
 */
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\nSIGINT received, shutting down...\n");
        running = 0;
    }
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);

    signal(SIGINT, signal_handler);

    Server server = {0};

    if (!server_init(&server, PORT)) {
        fprintf(stderr, "Failed to initialize server.\n");
        exit(EXIT_FAILURE);
    }

    while (running) {
        server_poll_events(&server);
    }

    server_shutdown(&server);

    return 0;
}
