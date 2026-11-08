#define _GNU_SOURCE // needed for addrinfo-related symbols
#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
// C11 introduces threads.h which subsumes the need for libpthread with
// simpler functionality (no thread attributes) ONLY IF __STDC_NO_THREADS__ is
// undefined by the compiler.
// For all data types and functions, "thrd" replaces "pthread", except for
// mutexes, in which case "mtx" replaces "pthread_mutex" (there's also support
// for condition variables)
#include <pthread.h>
// #ifdef __linux__
// #include <semaphore.h>
// #elif __APPLE__
// #include <dispatch/dispatch.h>
// #endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static const unsigned short PORT_NUMBER = 8080;
static int server_socket;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// #ifdef __linux__
// static sem_t binary_semaphore;
// #elif __APPLE__
// static dispatch_semaphore_t binary_semaphore;
// #endif
static unsigned long long counter = 0;

typedef struct sock_info {
    int             client_socket;
    struct in_addr  client_address;
} sock_info;

enum nettype {
    IPv4,
    IPv6,
};

/**
 * @brief Thread that handles each client connection.
 *
 * @param arg pointer to a heap-allocated sock_info struct (freed here)
 * @return NULL
 */
static void *client_handler(void *arg)
{
    char server_message[4096], content[128], content_length[64], date[64];
    char client_message[4096], *headers_begin;
    sock_info *socket_info;
    int client_socket;
    struct in_addr client_address;
    time_t curr_time;
    ssize_t n;

    // Each thread gets its own copy of sock_info, so the main loop can't
    // overwrite it with the next connection's socket before we read it
    socket_info = (sock_info *) arg;
    client_socket = socket_info->client_socket;
    client_address = socket_info->client_address;
    free(socket_info);

    // Reading the request happens here rather than in main() so that a slow
    // or silent client only holds up its own thread, not the accept loop.
    // pass MSG_WAITALL in 4th argument to recv to wait for client_message to fill up entirely
    if ((n = recv(client_socket, client_message, sizeof(client_message) - 1, 0)) <= 0) {
        if (n < 0) {
            fprintf(stderr, "recv: %s\n", strerror(errno));
        }
        goto close_client;
    }
    client_message[n] = '\0';
    if ((headers_begin = strstr(client_message, "\r\n")) != NULL) {
        *headers_begin = '\0';
    }
    // Only accepting HTTP GET requests
    if (strncmp(client_message, "GET", 3) != 0) {
        goto close_client;
    }
    printf("%s\n", client_message);

    // critical section
    pthread_mutex_lock(&mutex); // using a mutex
// #ifdef __linux__
//     sem_wait(&binary_semaphore); // using a binary semaphore
// #elif __APPLE__
//     dispatch_semaphore_wait(binary_semaphore, DISPATCH_TIME_FOREVER);
// #endif
    ++counter;
    printf("Handling request #%llu\n", counter);
// #ifdef __linux__
//     sem_post(&binary_semaphore);
// #elif __APPLE__
//     dispatch_semaphore_signal(binary_semaphore);
// #endif
    pthread_mutex_unlock(&mutex);

    strcpy(server_message, "HTTP/1.1 200 OK\r\n");

    curr_time = time(NULL);
    strftime(date, 64, "Date: %a, %d %b %Y %X GMT\r\n", gmtime(&curr_time));
    strcat(server_message, date);

    strcat(server_message, "Server: Web Server\r\n");
    strcat(server_message, "Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\r\n");
    strcat(server_message, "Accept-Ranges: bytes\r\n");

    strcpy(content, "What's up? This server was written in POSIX C. Your IP address is ");
    inet_ntop(AF_INET, &client_address, content + strlen(content), INET_ADDRSTRLEN);
    strcat(content, "\r\n");

    sprintf(content_length, "Content-Length: %zu\r\n", strlen(content));
    strcat(server_message, content_length);
    strcat(server_message, "Content-Type: text/html\r\n\r\n");
    strcat(server_message, content);

    // MSG_NOSIGNAL: a client that already disconnected shouldn't kill the
    // whole server with SIGPIPE
    if (send(client_socket, server_message, strlen(server_message), MSG_NOSIGNAL) < 0) {
        fprintf(stderr, "send: %s\n", strerror(errno));
    }

close_client:
    // Always close the socket, even after an error, so file descriptors don't leak
    if (shutdown(client_socket, SHUT_WR) < 0 && errno != ENOTCONN) {
        fprintf(stderr, "shutdown: %s\n", strerror(errno));
    }
    if (close(client_socket) < 0) {
        fprintf(stderr, "close: %s\n", strerror(errno));
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
    int err, client_socket, reuseaddr = 1, fildes[2];
    struct sockaddr_in server_connection, client_connection;
    socklen_t client_connection_size = (socklen_t) sizeof(client_connection);
    pthread_t thread_id;
    sock_info *socket_info;
    char date[30] = {0}, addr[INET_ADDRSTRLEN];
    ssize_t date_length;
    struct timeval recv_timeout = { .tv_sec = 10, .tv_usec = 0 };
    struct addrinfo *ipaddrs, *res, hints = {0};

    enum nettype net_type = IPv4;
    switch (net_type)
    {
    case IPv4:
        puts("Using IPv4");
        break;
    case IPv6:
        puts("Using IPv6");
        break;
    default:
        puts("Unknown protocol.");
        break;
    }

    // FILE *fp = popen("date", "r");
    // fgets(date, sizeof(date), fp);
    // pclose(fp);
    // printf("Current time: %s\n", date);

    if (pipe(fildes) < 0) {
        fprintf(stderr, "pipe: %s\n", strerror(errno));
        exit(1);
    }

    switch (fork()) {
    case -1:
        fprintf(stderr, "fork: %s\n", strerror(errno));
        exit(1);
    case 0:
        close(fildes[0]);
        dup2(fildes[1], STDOUT_FILENO);
        execlp("date", "date", (char *) 0);
        return 1; // to prevent fallthrough in case of an execlp error
    default:
        close(fildes[1]);
        if ((date_length = read(fildes[0], date, sizeof(date) - 1)) > 0) {
            date[date_length] = '\0';
            printf("Current time: %s", date);
        }
        close(fildes[0]);
        wait(NULL);
    }

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = IPPROTO_TCP; // can be 0 since TCP is implied by SOCK_STREAM
    if ((err = getaddrinfo("www.google.com", NULL, &hints, &ipaddrs)) != 0) {
        // ipaddrs is not set on failure, so it must not be used or freed
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
    } else {
        puts("IPv4 addresses associated with www.google.com:");
        for (res = ipaddrs; res != NULL; res = res->ai_next) {
            inet_ntop(res->ai_family, &((struct sockaddr_in *)res->ai_addr)->sin_addr, addr, sizeof(addr));
            printf("%s\n", addr);
        }
        freeaddrinfo(ipaddrs);
    }

    if (argc == 2) {
        port_number = (unsigned short) atoi(argv[1]);
    }

// #ifdef __linux__
//     if (sem_init(&binary_semaphore, 0, 1) < 0) {
//         fprintf(stderr, "sem_init: %s\n", strerror(errno));
//         exit(0);
//     }
// #elif __APPLE__
//     binary_semaphore = dispatch_semaphore_create(1);
// #endif

    // To support IPv6 as well, one could create an AF_INET6 socket and
    // initially use FD_ZERO() and FD_SET(), and then select() and FD_ISSET()
    // to poll desired file descriptors (our sockets) to see which sockets are
    // currently readable. #include <sys/select.h>
    //
    // This happens synchronously, so maybe 2 separate threads are desired.
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "socket: %s\n", strerror(errno));
        exit(1);
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

    while (1) {
        memset(&client_connection, 0, sizeof(client_connection));
        client_connection_size = (socklen_t) sizeof(client_connection);
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
            // Errors like ECONNABORTED (client gave up) or EMFILE (out of
            // file descriptors) only affect one connection; keep serving
            fprintf(stderr, "accept: %s\n", strerror(errno));
            continue;
        }

        // Bound how long an idle client can tie up its thread
        if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) < 0) {
            fprintf(stderr, "setsockopt: %s\n", strerror(errno));
        }

        // would need to deep copy sin6_addr to client_address for IPv6
        // Allocated per connection; client_handler frees it
        if ((socket_info = malloc(sizeof(*socket_info))) == NULL) {
            fprintf(stderr, "malloc: %s\n", strerror(errno));
            close(client_socket);
            continue;
        }
        *socket_info = (sock_info) {
            .client_socket = client_socket,
            .client_address = client_connection.sin_addr,
        };
        if ((err = pthread_create(&thread_id, NULL, client_handler,
                socket_info)) != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(err));
            free(socket_info);
            close(client_socket);
            continue;
        }
        // Detached threads clean up after themselves. Joining them in batches
        // would make the accept loop wait on the slowest client in the batch.
        if ((err = pthread_detach(thread_id)) != 0) {
            fprintf(stderr, "pthread_detach: %s\n", strerror(err));
        }
    }

cleanup:
    if (close(server_socket) < 0) {
        fprintf(stderr, "close: %s\n", strerror(errno));
    }
    return 1;
}
