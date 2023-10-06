import signal
import socket
import time
from sys import exit
from threading import Thread
from traceback import print_exception
from typing_extensions import override

INT_MAX = 2147483647
PORT_NUMBER = 8080
server_socket: socket.socket

class ClientHandler(Thread):
    """Thread class for each client connection."""

    def __init__(self, client_socket_arg: socket.socket, client_address_arg: str):
        """Constructor for ClientHandler.

        Args:
            client_socket_arg (socket.socket): client socket
            client_address_arg (str): client host address
        """
        Thread.__init__(self)
        self.client_socket = client_socket_arg
        self.client_address = client_address_arg

    @override
    def run(self) -> None:
        """Called by the start() function asynchronously."""
        server_message = "HTTP/1.1 200 OK\n"
        format_string = "Date: %a, %d %b %Y %X GMT\n"
        server_message += time.strftime(format_string, time.gmtime())
        server_message += "Server: Web Server\n"
        server_message += "Last-Modified: Fri, 08 Apr 2022 12:35:05 GMT\n"
        server_message += "Accept-Ranges: bytes\n"

        content = f"What's up? Your IP address is {self.client_address}\n"

        server_message += f"Content-Length: {len(content)}\n"
        server_message += "Content-Type: text/html\n\n"
        server_message += content

        self.client_socket.send(server_message.encode())
        self.client_socket.shutdown(socket.SHUT_WR)
        self.client_socket.close()

def signal_handler(signum, frame) -> None:
    """Signal handler for Ctrl + C (SIGINT).

    Args:
        signum (int): unused
        frame (Frame): unused
    """
    server_socket.close()
    exit(0)

def main() -> None:
    """Main server loop."""
    threads = []

    global server_socket
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)

    localhost = socket.inet_ntoa(socket.INADDR_LOOPBACK.to_bytes(4))
    server_address = (localhost, PORT_NUMBER)
    server_socket.bind(server_address)
    signal.signal(signal.SIGINT, signal_handler)
    # signal.signal(signal.SIGINT, lambda signum, frame: signal_handler) # For signal_handler with no arguments

    server_socket.listen(INT_MAX)

    try:
        while True:
            client_socket, (client_address, _) = server_socket.accept()
            client_message = client_socket.recv(4096).decode()
            request_line = client_message[:client_message.find("\n")]
            print(request_line)
            newthread = ClientHandler(client_socket, client_address)
            newthread.start()
            threads.append(newthread)

            if len(threads) >= 50:
                for t in threads:
                    t.join()
                threads = []
    except Exception as e:
        print_exception(e)
        server_socket.close()

main()
