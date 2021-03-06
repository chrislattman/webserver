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

typedef struct sock_info {
    int             client_socket_t;
    struct in_addr  sin_addr_t;
} sock_info;

static void *sock_thread(void *arg)
{
    char server_message[4096], content[64], content_length[32], date[64];
    sock_info *socket_info = (sock_info *) arg;
    int client_socket_t = socket_info->client_socket_t;
    struct in_addr sin_addr_t = socket_info->sin_addr_t;

    time_t result = time(NULL);
    struct tm *time_info = gmtime(&result);

    strcpy(server_message, "HTTP/1.1 200 OK\n");
    strftime(date, 64, "Date: %a, %d %b %Y %X GMT\n", time_info);
    strcat(server_message, date);
    strcat(server_message, "Server: Web Server\n");
    strcat(server_message, "Last-Modified: Fri, 08 Apr 2022 12:35:05 GMT\n");
    strcat(server_message, "Accept-Ranges: bytes\n");

    strcpy(content, "What's up? Your IP address is ");
    strncat(content, inet_ntoa(sin_addr_t), 15);
    strcat(content, "\n");

    sprintf(content_length, "Content-Length: %zu\n", strlen(content));
    strcat(server_message, content_length);
    strcat(server_message, "Content-Type: text/html\n\n");
    strcat(server_message, content);

    if (send(client_socket_t, server_message, strlen(server_message), 0) < 0) {
        fprintf(stderr, "send: %s\n", strerror(errno));
        goto cleanup;
    }
    if (shutdown(client_socket_t, SHUT_WR) < 0) {
        fprintf(stderr, "shutdown: %s\n", strerror(errno));
        goto cleanup;
    }
    if (close(client_socket_t) < 0) {
        fprintf(stderr, "close: %s\n", strerror(errno));
        goto cleanup;
    }

cleanup:
    free(socket_info);
    return NULL;
}

int main(void)
{
    int server_socket, client_socket, thread_index;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_size = (socklen_t) sizeof(client_address);
    pthread_t thread_id[60];
    sock_info *socket_info;

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "socket: %s\n", strerror(errno));
        exit(0);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(80);
    server_address.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_socket, (struct sockaddr *) &server_address,
            (socklen_t) sizeof(server_address)) < 0) {
        fprintf(stderr, "bind: %s\n", strerror(errno));
        goto cleanup;
    }

    if (listen(server_socket, INT_MAX) < 0) {
        fprintf(stderr, "listen: %s\n", strerror(errno));
        goto cleanup;
    }

    thread_index = 0;
    while (1) {
        memset(&client_address, 0, sizeof(client_address));
        if ((client_socket = accept(server_socket,
                (struct sockaddr *) &client_address,
                &client_address_size)) < 0) {
            fprintf(stderr, "accept: %s\n", strerror(errno));
            goto cleanup;
        }

        // the typecast is unnecessary for C, but allows this code to be
        // compiled as C++
        if ((socket_info = (sock_info *) malloc(sizeof(sock_info))) == NULL) {
            fprintf(stderr, "malloc: %s\n", strerror(errno));
            goto cleanup;
        }
        socket_info->client_socket_t = client_socket;
        socket_info->sin_addr_t = client_address.sin_addr;
        if (pthread_create(&thread_id[thread_index++], NULL, sock_thread,
                socket_info) != 0) {
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
