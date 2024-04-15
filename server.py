import signal
import socket
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

    content = f"What's up? Your IP address is {client_address}\n"

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
    threads = []

    port_number = 0
    if len(sys.argv) == 2:
        port_number = int(sys.argv[1]) & 65535

    global mutex, binary_semaphore
    mutex = Lock()
    binary_semaphore = Semaphore(1)

    global server_socket
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    localhost = socket.inet_ntoa(socket.INADDR_LOOPBACK.to_bytes(4, "big"))
    if port_number >= 10000:
        server_address = (localhost, port_number)
    else:
        server_address = (localhost, PORT_NUMBER)
    server_socket.bind(server_address)
    signal.signal(signal.SIGINT, signal_handler)
    # signal.signal(signal.SIGINT, lambda signum, frame: signal_handler) # For signal_handler with no arguments

    server_socket.listen(INT_MAX)

    try:
        while True:
            # To connect to a server:
            # sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
            # sock.connect(("127.0.0.1", 5000))
            client_socket, (client_address, _) = server_socket.accept()
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
                threads = []
    except Exception as e:
        print_exception(e)
        server_socket.close()

if __name__ == "__main__":
    main()
