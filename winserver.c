// Compiled with mingw-w64 on MSYS2: gcc -o winserver winserver.c -lws2_32
// This statically links libws2_32.a, which contains small stubs that
// redirect to ws2_32.dll (much like ws2_32.lib)

// Can also use pthread-style functions with mingw-w64 by installing
// winpthreads and statically linking it with -static [-pthread]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <WS2tcpip.h>
#include <errno.h>
#include <Windows.h>
#include <time.h>
#include <signal.h>

static const unsigned short PORT_NUMBER = 8080;
static SOCKET server_socket;
static HANDLE mutex, binary_semaphore;
static unsigned long long counter = 0;

static char error_message[1024];

typedef struct sock_info {
    SOCKET          client_socket;
    struct in_addr  client_address;
} sock_info;

static char *StrGetLastError(int error_code)
{
    LPSTR messageBuffer = NULL;

    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                   FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL,
                   error_code,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPSTR) &messageBuffer,
                   0,
                   NULL);

    strncpy(error_message, messageBuffer, sizeof(error_message) - 1);
    LocalFree(messageBuffer);

    return error_message;
}

/**
 * @brief Thread that handles each client connection.
 *
 * @param arg pointer to sock_info struct
 * @return 0
 */
static DWORD WINAPI client_handler(LPVOID arg)
{
    char server_message[4096], content[64], content_length[64], date[64];
    sock_info *socket_info;
    SOCKET client_socket;
    struct in_addr client_address;
    time_t curr_time;
    struct tm *time_info;

    // critical section
    WaitForSingleObject(mutex, INFINITE); // using a mutex
    // WaitForSingleObject(binary_semaphore, INFINITE); // using a binary semaphore
    ++counter;
    printf("Handling request #%llu\n", counter);
    // ReleaseSemaphore(binary_semaphore, 1, NULL);
    ReleaseMutex(mutex);

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
        fprintf(stderr, "send: %s\n", StrGetLastError(WSAGetLastError()));
        return 0;
    }
    if (shutdown(client_socket, SD_SEND) < 0) {
        fprintf(stderr, "shutdown: %s\n", StrGetLastError(WSAGetLastError()));
        return 0;
    }
    if (closesocket(client_socket) < 0) {
        fprintf(stderr, "closesocket: %s\n", StrGetLastError(WSAGetLastError()));
        return 0;
    }
    return 0;
}

/**
 * @brief Signal handler for Ctrl + C (SIGINT).
 *
 * @param signum unused
 */
static void signal_handler(int signum)
{
    if (closesocket(server_socket) < 0) {
        fprintf(stderr, "closesocket: %s\n", StrGetLastError(WSAGetLastError()));
    }
    WSACleanup();
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
    WSADATA wsaData;
    SOCKET client_socket;
    int err, thread_index;
    struct sockaddr_in server_connection, client_connection;
    socklen_t client_connection_size = (socklen_t) sizeof(client_connection);
    HANDLE thread_handle[60], new_thread;
    DWORD thread_id[60];
    sock_info socket_info;
    char client_message[4096], *headers_begin, *request_line;
    long request_line_length;

    if (argc == 2) {
        port_number = (unsigned short) atoi(argv[1]);
    }

    mutex = CreateMutexA(NULL, FALSE, NULL);
    if (mutex == NULL) {
        fprintf(stderr, "CreateMutexA: %s\n", StrGetLastError(GetLastError()));
        exit(0);
    }

    binary_semaphore = CreateSemaphoreA(NULL, 1, 1, NULL);
    if (binary_semaphore == NULL) {
        fprintf(stderr, "CreateSemaphoreA: %s\n", StrGetLastError(GetLastError()));
        exit(0);
    }

    if ((err = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        fprintf(stderr, "WSAStartup: %s\n", StrGetLastError(err));
        exit(0);
    }

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        fprintf(stderr, "socket: %s\n", StrGetLastError(WSAGetLastError()));
        exit(0);
    }
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, "1", sizeof(char *)) < 0) {
        fprintf(stderr, "setsockopt: %s\n", strerror(errno));
        goto cleanup;
    }

    server_connection.sin_family = AF_INET;
    if (port_number >= 10000) {
        server_connection.sin_port = htons(port_number);
    } else {
        server_connection.sin_port = htons(PORT_NUMBER);
    }
    server_connection.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(server_socket, (struct sockaddr *) &server_connection,
            (socklen_t) sizeof(server_connection)) < 0) {
        fprintf(stderr, "bind: %s\n", StrGetLastError(WSAGetLastError()));
        goto cleanup;
    }
    signal(SIGINT, signal_handler);

    if (listen(server_socket, INT_MAX) < 0) {
        fprintf(stderr, "listen: %s\n", StrGetLastError(WSAGetLastError()));
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
        // SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        // connect(sock, (struct sockaddr *) &connection, sizeof(connection));
        if ((client_socket = accept(server_socket,
                (struct sockaddr *) &client_connection,
                &client_connection_size)) == INVALID_SOCKET) {
            fprintf(stderr, "accept: %s\n", StrGetLastError(WSAGetLastError()));
            goto cleanup;
        }

        if (recv(client_socket, client_message, sizeof(client_message), 0) < 0) {
            fprintf(stderr, "recv: %s\n", StrGetLastError(WSAGetLastError()));
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

        socket_info = (sock_info) {
            .client_socket = client_socket,
            .client_address = client_connection.sin_addr,
        };
        if ((new_thread = CreateThread(NULL, 0, client_handler,
                &socket_info, 0, &thread_id[thread_index++])) == NULL) {
            fprintf(stderr, "CreateThread: %s\n", StrGetLastError(GetLastError()));
            goto cleanup;
        }
        thread_handle[thread_index] = new_thread;

        if (thread_index >= 50) {
            thread_index = 0;
            while (thread_index < 50) {
                if (CloseHandle(thread_handle[thread_index++]) == FALSE) {
                    fprintf(stderr, "CloseHandle: %s\n", StrGetLastError(GetLastError()));
                    goto cleanup;
                }
            }
            thread_index = 0;
        }
    }

cleanup:
    if (closesocket(server_socket) < 0) {
        fprintf(stderr, "closesocket: %s\n", StrGetLastError(WSAGetLastError()));
    }
    WSACleanup();
    return 0;
}
