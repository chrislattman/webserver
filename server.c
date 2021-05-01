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

#define DEFAULT_PORT "80"

typedef struct {
    int             client_socket_t;
    struct in_addr  sin_addr_t;
} sock_info;

void *sock_thread(void *arg) {
    char server_message[4096], content[64], content_length[32], date[64];
    sock_info socket_info = *((sock_info *) arg);
    int client_socket_t = socket_info.client_socket_t;
    struct in_addr sin_addr_t = socket_info.sin_addr_t;

    time_t result = time(NULL);
    struct tm *time_info = gmtime(&result);

    strcpy(server_message, "HTTP/1.1 200 OK\n");
    strftime(date, 64, "Date: %a, %d %b %Y %X GMT\n", time_info);
    strcat(server_message, date);
    strcat(server_message, "Server: Web Server\n");
    strcat(server_message, "Last-Modified: Wed, 07 Apr 2021 02:50:30 GMT\n");
    strcat(server_message, "Accept-Ranges: bytes\n");

    strcpy(content, "What's up?\nYour IP address is ");
    strcat(content, inet_ntoa(sin_addr_t));
    strcat(content, "\n");

    // copying to server message from HTML file
    //
    // char html[4096], line[256];
    // FILE *fp = fopen("index.html", "r");
    // while (fgets(line, 256, fp) != NULL) {
    //     content_len += strlen(line);
    //     strcat(html, line);
    //     memset(line, 0, 256);
    // }

    sprintf(content_length, "Content-Length: %lu\n", strlen(content));
    strcat(server_message, content_length);
    strcat(server_message, "Content-Type: text/html\n\n");
    strcat(server_message, content);
    // strcat(server_message, html);

    if (send(client_socket_t, server_message, 
            strlen(server_message), 0) < 0) {
        fprintf(stderr, "send: %s\n", strerror(errno));
        exit(0);
    }
    if (shutdown(client_socket_t, SHUT_WR) < 0) {
        fprintf(stderr, "shutdown: %s\n", strerror(errno));
        exit(0);
    }
    if (close(client_socket_t) < 0) {
        fprintf(stderr, "close: %s\n", strerror(errno));
        exit(0);
    }

    pthread_exit(NULL);
}

int main() {
    int server_socket, client_socket, thread_index, code;
    struct sockaddr_in server_address, client_address;
    struct addrinfo *result, hints;
    socklen_t client_address_size = sizeof(client_address);
    pthread_t thread_id[60];
    sock_info *socket_info;

    /* Approach 1 */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    if ((code = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result)) < 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(code));
        exit(0);
    }

    if ((server_socket = socket(result->ai_family, result->ai_socktype, 
            result->ai_protocol)) < 0) {
        freeaddrinfo(result);
        fprintf(stderr, "socket: %s\n", strerror(errno));
        exit(0);
    }

    if (bind(server_socket, result->ai_addr, result->ai_addrlen) < 0) {
        freeaddrinfo(result);
        if (close(server_socket) < 0) {
            fprintf(stderr, "close: %s\n", strerror(errno));
            exit(0);
        }
        fprintf(stderr, "bind: %s\n", strerror(errno));
        exit(0);
    }

    freeaddrinfo(result);

    /* Approach 2 */
    // if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    //     fprintf(stderr, "socket: %s\n", strerror(errno));
    //     exit(0);
    // }

    // server_address.sin_family = AF_INET;
    // server_address.sin_port = htons(80);
    // server_address.sin_addr.s_addr = INADDR_ANY;

    // if (bind(server_socket, (struct sockaddr *) &server_address, 
    //         sizeof(server_address)) < 0) {
    //     if (close(server_socket) < 0) {
    //         fprintf(stderr, "close: %s\n", strerror(errno));
    //         exit(0);
    //     }
    //     fprintf(stderr, "bind: %s\n", strerror(errno));
    //     exit(0);
    // }

    if (listen(server_socket, INT_MAX) < 0) {
        if (close(server_socket) < 0) {
            fprintf(stderr, "close: %s\n", strerror(errno));
            exit(0);
        }
        fprintf(stderr, "listen: %s\n", strerror(errno));
        exit(0);
    }

    thread_index = 0;
    while (1) {
        if ((client_socket = accept(server_socket, 
                (struct sockaddr *) &client_address, 
                &client_address_size)) < 0) {
            if (close(server_socket) < 0) {
                fprintf(stderr, "close: %s\n", strerror(errno));
                exit(0);
            }
            fprintf(stderr, "accept: %s\n", strerror(errno));
            exit(0);
        }

        if ((socket_info = 
                (sock_info*) malloc(sizeof(sock_info))) == NULL) {
            fprintf(stderr, "malloc: %s\n", strerror(errno));
            exit(0);
        }
        socket_info->client_socket_t = client_socket;
        socket_info->sin_addr_t = client_address.sin_addr;
        if (pthread_create(&thread_id[thread_index++], NULL, sock_thread, 
                socket_info) != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(errno));
            exit(0);
        }
        free(socket_info);

        if (thread_index >= 50) {
            thread_index = 0;
            while (thread_index < 50) {
                if (pthread_join(thread_id[thread_index++], NULL) != 0) {
                    fprintf(stderr, "pthread_join: %s\n", strerror(errno));
                    exit(0);
                }
            }
            thread_index = 0;
        }
    }

    return 0;
}
