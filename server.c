#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
// #include <semaphore.h>
#include <sys/wait.h>

static const unsigned short PORT_NUMBER = 8080;
static int server_socket;
static pthread_mutex_t mutex;
// static sem_t binary_semaphore; // Supported on Linux only, for macOS use Grand Central Dispatch
static unsigned long long counter = 0;

typedef struct sock_info {
    int             client_socket;
    struct in_addr  client_address;
} sock_info;

/**
 * @brief Thread that handles each client connection.
 *
 * @param arg pointer to sock_info struct
 * @return NULL
 */
static void *client_handler(void *arg)
{
    char server_message[4096], content[64], content_length[64], date[64];
    sock_info *socket_info;
    int client_socket;
    struct in_addr client_address;
    time_t curr_time;
    struct tm *time_info;

    // critical section
    pthread_mutex_lock(&mutex); // using a mutex
    // sem_wait(&binary_semaphore); // using a binary semaphore
    ++counter;
    printf("Handling request #%llu\n", counter);
    // sem_post(&binary_semaphore);
    pthread_mutex_unlock(&mutex);

    socket_info = (sock_info *) arg;
    client_socket = socket_info->client_socket;
    client_address = socket_info->client_address;

    curr_time = time(NULL);
    time_info = gmtime(&curr_time);

    strcpy(server_message, "HTTP/1.1 200 OK\n");
    strftime(date, 64, "Date: %a, %d %b %Y %X GMT\n", time_info);
    strcat(server_message, date);
    strcat(server_message, "Server: Web Server\n");
    strcat(server_message, "Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\n");
    strcat(server_message, "Accept-Ranges: bytes\n");

    strcpy(content, "What's up? Your IP address is ");
    strncat(content, inet_ntoa(client_address), 15);
    strcat(content, "\n");

    sprintf(content_length, "Content-Length: %zu\n", strlen(content));
    strcat(server_message, content_length);
    strcat(server_message, "Content-Type: text/html\n\n");
    strcat(server_message, content);

    if (send(client_socket, server_message, strlen(server_message), 0) < 0) {
        fprintf(stderr, "send: %s\n", strerror(errno));
        return NULL;
    }
    if (shutdown(client_socket, SHUT_WR) < 0) {
        fprintf(stderr, "shutdown: %s\n", strerror(errno));
        return NULL;
    }
    if (close(client_socket) < 0) {
        fprintf(stderr, "close: %s\n", strerror(errno));
        return NULL;
    }
    return NULL;
}

/**
 * @brief Signal handler for Ctrl + C (SIGINT).
 *
 * @param signum unused
 */
static void signal_handler(__attribute__((unused)) int signum)
{
    if (close(server_socket) < 0) {
        fprintf(stderr, "close: %s\n", strerror(errno));
    }
    exit(0);
}

/**
 * @brief Main server loop for the web server.
 *
 * @return 0
 */
int main(int argc, char *argv[])
{
    unsigned short port_number = 0;
    int client_socket, thread_index, reuseaddr = 1, fildes[2];
    struct sockaddr_in server_connection, client_connection;
    socklen_t client_connection_size = (socklen_t) sizeof(client_connection);
    pthread_t thread_id[60];
    sock_info socket_info;
    char date[30], client_message[4096], *headers_begin, *request_line;
    long request_line_length;

    if (pipe(fildes) < 0) {
        fprintf(stderr, "pipe: %s\n", strerror(errno));
        exit(0);
    }

    switch (fork()) {
    case -1:
        fprintf(stderr, "fork: %s\n", strerror(errno));
        exit(0);
    case 0:
        close(fildes[0]);
        dup2(fildes[1], STDOUT_FILENO);
        execlp("date", "", NULL);
        exit(0);
    default:
        close(fildes[1]);
        read(fildes[0], date, sizeof(date) - 1);
        printf("Current time: %s", date);
        close(fildes[0]);
        wait(NULL);
    }

    if (argc == 2) {
        port_number = (unsigned short) atoi(argv[1]);
    }

    if (pthread_mutex_init(&mutex, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init: %s\n", strerror(errno));
        exit(0);
    }

    // if (sem_init(&binary_semaphore, 0, 1) < 0) {
    //     fprintf(stderr, "sem_init: %s\n", strerror(errno));
    //     exit(0);
    // }

    // To support IPv6 as well, one could create an AF_INET6 socket and
    // initially use FD_ZERO() and FD_SET(), and then select() and FD_ISSET()
    // to poll desired file descriptors (our sockets) to see which sockets are
    // currently readable. #include <sys/select.h>
    //
    // This happens synchronously, so maybe 2 separate threads are desired.
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "socket: %s\n", strerror(errno));
        exit(0);
    }
    signal(SIGINT, signal_handler);
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) < 0) {
        fprintf(stderr, "setsockopt: %s\n", strerror(errno));
        goto cleanup;
    }

    server_connection.sin_family = AF_INET;
    if (port_number >= 10000) {
        server_connection.sin_port = htons(port_number);
    } else {
        server_connection.sin_port = htons(PORT_NUMBER);
    }
    // inet_pton(AF_INET6, "::1", &server_connection.sin6_addr.s6_addr) for IPv6
    server_connection.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(server_socket, (struct sockaddr *) &server_connection,
            (socklen_t) sizeof(server_connection)) < 0) {
        fprintf(stderr, "bind: %s\n", strerror(errno));
        goto cleanup;
    }

    if (listen(server_socket, INT_MAX) < 0) {
        fprintf(stderr, "listen: %s\n", strerror(errno));
        goto cleanup;
    }

    thread_index = 0;
    while (1) {
        memset(&client_connection, 0, sizeof(client_connection));
        // To connect to a server:
        // struct sockaddr_in connection;
        // connection.sin_family = AF_INET;
        // connection.sin_port = htons(5000);
        // connection.sin_addr.s_addr = inet_addr("127.0.0.1");
        // int sock = socket(AF_INET, SOCK_STREAM, 0);
        // connect(sock, (struct sockaddr *) &connection, sizeof(connection));
        if ((client_socket = accept(server_socket,
                (struct sockaddr *) &client_connection,
                &client_connection_size)) < 0) {
            fprintf(stderr, "accept: %s\n", strerror(errno));
            goto cleanup;
        }

        if (recv(client_socket, client_message, sizeof(client_message), 0) < 0) {
            fprintf(stderr, "recv: %s\n", strerror(errno));
            goto cleanup;
        }
        headers_begin = strchr(client_message, '\n');
        // headers_begin = strstr(client_message, "\n");
        request_line_length = headers_begin - client_message;
        request_line = calloc(request_line_length + 1, sizeof(char));
        if (request_line == NULL) {
            fprintf(stderr, "malloc: %s\n", strerror(errno));
            goto cleanup;
        }
        strncpy(request_line, client_message, request_line_length);
        printf("%s\n", request_line);
        free(request_line);

        // would need to deep copy sin6_addr to client_address for IPv6
        socket_info = (sock_info) {
            .client_socket = client_socket,
            .client_address = client_connection.sin_addr,
        };
        if (pthread_create(&thread_id[thread_index++], NULL, client_handler,
                &socket_info) != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(errno));
            goto cleanup;
        }

        if (thread_index >= 50) {
            thread_index = 0;
            while (thread_index < 50) {
                if (pthread_join(thread_id[thread_index++], NULL) != 0) {
                    fprintf(stderr, "pthread_join: %s\n", strerror(errno));
                    goto cleanup;
                }
            }
            thread_index = 0;
        }
    }

cleanup:
    if (close(server_socket) < 0) {
        fprintf(stderr, "close: %s\n", strerror(errno));
    }
    return 0;
}
