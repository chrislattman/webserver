from enum import Enum
import signal
import socket
import subprocess
import sys
import time
from threading import Lock, Semaphore, Thread
from traceback import print_exception
from typing_extensions import override

INT_MAX = 2147483647
PORT_NUMBER = 8080
server_socket: socket.socket
mutex: Lock
binary_semaphore: Semaphore
counter = 0


class Nettype(Enum):
    IPv4 = 0
    IPv6 = 1


class ClientHandler(Thread):
    """Thread class for each client connection."""

    def __init__(self, client_socket_arg: socket.socket, client_address_arg: str) -> None:
        """Constructor for ClientHandler.

        Args:
            client_socket_arg (socket.socket): client socket
            client_address_arg (str): client host address
        """
        # daemon threads don't keep the process alive after Ctrl + C
        super().__init__(daemon=True)
        self.client_socket = client_socket_arg
        self.client_address = client_address_arg

    @override
    def run(self) -> None:
        """Called by the start() function asynchronously."""
        client_handler(self.client_socket, self.client_address)


def client_handler(client_socket: socket.socket, client_address: str) -> None:
    """Thread that handles each client connection.

    Args:
        client_socket_arg (socket.socket): client socket
        client_address_arg (str): client host address
    """
    try:
        # Reading the request happens here rather than in main() so that a
        # slow or silent client only holds up its own thread, not the accept
        # loop. The timeout bounds how long an idle client can tie it up.
        client_socket.settimeout(10)
        # pass socket.MSG_WAITALL in 2nd argument of recv to wait for all bytes
        client_message_bytes = client_socket.recv(4096)
        # Don't let invalid UTF-8 from a client raise an exception
        client_message = client_message_bytes.decode(errors="replace")
        request_line = client_message.split("\r\n", 1)[0]
        # Only accepting HTTP GET requests
        if not request_line.startswith("GET"):
            return
        print(request_line)
        send_response(client_socket, client_address)
    except OSError as e:
        print("client_handler:", e)
    finally:
        # Always close the socket, even after an error
        try:
            client_socket.shutdown(socket.SHUT_WR)
        except OSError:
            pass  # client already disconnected
        client_socket.close()


def send_response(client_socket: socket.socket, client_address: str) -> None:
    """Sends the HTTP response to a client.

    Args:
        client_socket (socket.socket): client socket
        client_address (str): client host address
    """

    # critical section
    global mutex, binary_semaphore, counter
    mutex.acquire() # using a mutex
    # binary_semaphore.acquire() # using a binary semaphore
    counter += 1
    print("Handling request #" + str(counter))
    # binary_semaphore.release()
    mutex.release()

    server_message = "HTTP/1.1 200 OK\r\n"

    format_string = "Date: %a, %d %b %Y %X GMT\r\n"
    server_message += time.strftime(format_string, time.gmtime())

    server_message += "Server: Web Server\r\n"
    server_message += "Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\r\n"
    server_message += "Accept-Ranges: bytes\r\n"

    content = f"What's up? This server was written in Python. Your IP address is {client_address}\r\n"

    server_message += f"Content-Length: {len(content)}\r\n"
    server_message += "Content-Type: text/html\r\n\r\n"
    server_message += content

    # send() may only send part of the message; sendall() sends all of it
    client_socket.sendall(server_message.encode())

def signal_handler(signum, frame) -> None:
    """Signal handler for Ctrl + C (SIGINT).

    Args:
        signum (int): unused
        frame (Frame): unused
    """
    server_socket.close()
    sys.exit(0)

def main() -> None:
    """Main server loop for the web server."""
    net_type = Nettype.IPv4
    match net_type:
        case Nettype.IPv4:
            print("Using IPv4")
        case Nettype.IPv6:
            print("Using IPv6")
        case _:
            print("Unknown protocol.")

    date = subprocess.run(["date"], capture_output=True)
    print("Current time: " + date.stdout.decode(), end="")

    try:
        ipaddrs = socket.getaddrinfo("www.google.com",
                                     None,
                                     family=socket.AF_INET,
                                     type=socket.SOCK_STREAM,
                                     proto=socket.IPPROTO_TCP)
        print("IPv4 addresses associated with www.google.com:")
        for res in ipaddrs:
            print(res[4][0])
    except socket.gaierror as e:
        print("getaddrinfo:", e)

    port_number = 0
    if len(sys.argv) == 2:
        port_number = int(sys.argv[1]) & 65535

    global mutex, binary_semaphore
    mutex = Lock()
    binary_semaphore = Semaphore(1)

    # To support IPv6 as well, one could create an AF_INET6 socket and use the
    # first list in the tuple returned by select.select() to poll desired
    # file descriptors (our sockets) to see which sockets are currently
    # readable. import select
    #
    # This happens synchronously, so maybe 2 separate threads are desired.
    global server_socket
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
    signal.signal(signal.SIGINT, signal_handler)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    localhost = socket.inet_ntoa(socket.INADDR_LOOPBACK.to_bytes(4, "big")) # "127.0.0.1" or "::1" for IPv6
    if port_number >= 10000:
        server_address = (localhost, port_number)
    else:
        server_address = (localhost, PORT_NUMBER)
    server_socket.bind(server_address)
    # signal.signal(signal.SIGINT, lambda signum, frame: signal_handler) # For signal_handler with no arguments

    server_socket.listen(INT_MAX)

    try:
        while True:
            # To connect to a server:
            # sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
            # sock.connect(("127.0.0.1", 5000))
            # For IPv6, use (client_address, _, _, _) instead
            try:
                client_socket, (client_address, _) = server_socket.accept()
            except OSError as e:
                # e.g. ECONNABORTED or EMFILE: only affects one connection
                print("accept:", e)
                continue
            newthread = ClientHandler(client_socket, client_address)
            # newthread = Thread(target=client_handler, args=(client_socket, client_address), daemon=True) # To create a pthread-style thread
            # Joining threads in batches would make the accept loop wait on
            # the slowest client in the batch, so let each thread finish alone
            newthread.start()
    except Exception as e:
        print_exception(e)
        server_socket.close()

if __name__ == "__main__":
    main()
