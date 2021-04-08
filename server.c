#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

typedef struct {
    int             client_socket_t;
    struct in_addr  sin_addr_t;
} sock_info;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *sock_thread(void *arg) {
    char server_message[64], client_ip_addr[16];
    sock_info socket_info = *((sock_info *) arg);
    int client_socket_t = socket_info.client_socket_t;
    struct in_addr sin_addr_t = socket_info.sin_addr_t;

    pthread_mutex_lock(&lock);
    strcpy(server_message, "What's up?\nYour IP address is ");
    strcpy(client_ip_addr, inet_ntoa(sin_addr_t));
    strcat(server_message, client_ip_addr);
    strcat(server_message, "\n");
    pthread_mutex_unlock(&lock);
    sleep(1);

    if (send(client_socket_t, server_message, 
            sizeof(server_message), 0) < 0) {
        fprintf(stderr, "send: %s\n", strerror(errno));
        exit(0);
    }
    if (close(client_socket_t) < 0) {
        fprintf(stderr, "close: %s\n", strerror(errno));
        exit(0);
    }

    pthread_exit(NULL);
}

int main() {
    int server_socket, client_socket, thread_index;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_size = sizeof(client_address);
    pthread_t thread_id[60];

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "socket: %s\n", strerror(errno));
        exit(0);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(9001);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *) &server_address, 
            sizeof(server_address)) < 0) {
        fprintf(stderr, "bind: %s\n", strerror(errno));
        exit(0);
    }

    if (listen(server_socket, 50) < 0) {
        fprintf(stderr, "listen: %s\n", strerror(errno));
        exit(0);
    }

    thread_index = 0;
    while (1) {
        if ((client_socket = accept(server_socket, 
                (struct sockaddr *) &client_address, 
                &client_address_size)) < 0) {
            fprintf(stderr, "accept: %s\n", strerror(errno));
            exit(0);
        }

        sock_info *socket_info = (sock_info*) malloc(sizeof(sock_info));
        socket_info->client_socket_t = client_socket;
        socket_info->sin_addr_t = client_address.sin_addr;
        if (pthread_create(&thread_id[thread_index++], NULL, sock_thread, socket_info) != 0) {
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
