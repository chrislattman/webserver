// Compiled with mingw-w64 on MSYS2: gcc -o winserver winserver.c -lws2_32
// This statically links libws2_32.a, which contains small stubs that
// redirect to ws2_32.dll (much like ws2_32.lib)

// Can also use pthread-style functions with mingw-w64 by installing
// winpthreads and statically linking it with -static [-pthread]
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <WS2tcpip.h>
#include <Windows.h>

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
    inet_ntop(AF_INET, &client_address, content + strlen(content), INET_ADDRSTRLEN);
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
    SECURITY_ATTRIBUTES sa;
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    unsigned short port_number = 0;
    WSADATA wsaData;
    SOCKET client_socket;
    int err, thread_index;
    struct sockaddr_in server_connection, client_connection;
    socklen_t client_connection_size = (socklen_t) sizeof(client_connection);
    HANDLE fildes[2], thread_handle[60], new_thread;
    DWORD thread_id[60];
    sock_info socket_info;
    char date[30], client_message[4096], *headers_begin, *request_line, addr[INET_ADDRSTRLEN];
    long request_line_length;
    struct addrinfo *ipaddrs, *res, hints = {0};

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    if (!CreatePipe(&fildes[0], &fildes[1], &sa, 0)) {
        fprintf(stderr, "CreatePipe: %s\n", StrGetLastError(GetLastError()));
        exit(1);
    }
    SetHandleInformation(fildes[0], HANDLE_FLAG_INHERIT, 0);

    si.cb = sizeof(STARTUPINFO);
    si.hStdOutput = fildes[1];
    si.dwFlags |= STARTF_USESTDHANDLES;
    if (!CreateProcessA(NULL, "date", NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "CreateProcessA: %s\n", StrGetLastError(GetLastError()));
        exit(1);
    }
    CloseHandle(fildes[1]);
    ReadFile(fildes[0], date, sizeof(date) - 1, NULL, NULL);
    printf("Current time: %s\n", date);
    CloseHandle(fildes[0]);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if ((err = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        fprintf(stderr, "WSAStartup: %s\n", StrGetLastError(err));
        exit(1);
    }

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = IPPROTO_TCP; // can be 0 since TCP is implied by SOCK_STREAM
    if ((err = getaddrinfo("www.google.com", NULL, &hints, &ipaddrs)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
    }
    printf("IPv4 addresses associated with www.google.com:\n");
    for (res = ipaddrs; res != NULL; res = res->ai_next) {
        inet_ntop(res->ai_family, &((struct sockaddr_in *)res->ai_addr)->sin_addr, addr, sizeof(addr));
        printf("%s\n", addr);
    }
    freeaddrinfo(ipaddrs);

    if (argc == 2) {
        port_number = (unsigned short) atoi(argv[1]);
    }

    mutex = CreateMutexA(NULL, FALSE, NULL);
    if (mutex == NULL) {
        fprintf(stderr, "CreateMutexA: %s\n", StrGetLastError(GetLastError()));
        exit(1);
    }

    binary_semaphore = CreateSemaphoreA(NULL, 1, 1, NULL);
    if (binary_semaphore == NULL) {
        fprintf(stderr, "CreateSemaphoreA: %s\n", StrGetLastError(GetLastError()));
        exit(1);
    }

    // To support IPv6 as well, you could create an AF_INET6 socket and
    // initially use FD_ZERO() and FD_SET(), and then select() and FD_ISSET()
    // to poll desired file descriptors (our sockets) to see which sockets are
    // currently readable.
    //
    // This happens synchronously, so maybe 2 separate threads are desired.
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        fprintf(stderr, "socket: %s\n", StrGetLastError(WSAGetLastError()));
        exit(1);
    }
    signal(SIGINT, signal_handler);
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
    // inet_pton(AF_INET6, "::1", &server_connection.sin6_addr.s6_addr) for IPv6
    server_connection.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(server_socket, (struct sockaddr *) &server_connection,
            (socklen_t) sizeof(server_connection)) < 0) {
        fprintf(stderr, "bind: %s\n", StrGetLastError(WSAGetLastError()));
        goto cleanup;
    }

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

        // pass MSG_WAITALL in 4th argument to recv to wait for client_message
        // to fill up with bytes
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

        // would need to deep copy sin6_addr to client_address for IPv6
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
    return 1;
}
