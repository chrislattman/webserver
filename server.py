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
        super().__init__()
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

    # critical section
    global mutex, binary_semaphore, counter
    mutex.acquire() # using a mutex
    # binary_semaphore.acquire() # using a binary semaphore
    counter += 1
    print("Handling request #" + str(counter))
    # binary_semaphore.release()
    mutex.release()

    server_message = "HTTP/1.1 200 OK\n"

    format_string = "Date: %a, %d %b %Y %X GMT\n"
    server_message += time.strftime(format_string, time.gmtime())

    server_message += "Server: Web Server\n"
    server_message += "Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\n"
    server_message += "Accept-Ranges: bytes\n"

    content = f"What's up? This server was written in Python. Your IP address is {client_address}\n"

    server_message += f"Content-Length: {len(content)}\n"
    server_message += "Content-Type: text/html\n\n"
    server_message += content

    client_socket.send(server_message.encode())
    client_socket.shutdown(socket.SHUT_WR)
    client_socket.close()

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
    threads: list[ClientHandler] = []

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

    ipaddrs = socket.getaddrinfo("www.google.com",
                                 None,
                                 family=socket.AF_INET,
                                 type=socket.SOCK_STREAM,
                                 proto=socket.IPPROTO_TCP)
    print("IPv4 addresses associated with www.google.com:")
    for res in ipaddrs:
        print(res[4][0])

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
            client_socket, (client_address, _) = server_socket.accept()
            # pass socket.MSG_WAITALL in 2nd argument of recv to wait for all bytes
            client_message_bytes = client_socket.recv(4096)
            client_message = client_message_bytes.decode()
            request_line = client_message[:client_message.find("\n")]
            print(request_line)
            newthread = ClientHandler(client_socket, client_address)
            # newthread = Thread(target=client_handler, args=(client_socket, client_address)) # To create a pthread-style thread
            newthread.start()
            threads.append(newthread)

            if len(threads) >= 50:
                for t in threads:
                    t.join()
                threads.clear()
    except Exception as e:
        print_exception(e)
        server_socket.close()

if __name__ == "__main__":
    main()
