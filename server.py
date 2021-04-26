import socket
import time
from threading import Thread


class ClientHandler(Thread):
    def __init__(self, client_socket, client_address):
        Thread.__init__(self)
        self.client_socket = client_socket
        self.client_address = client_address

    def run(self):
        server_message = 'HTTP/1.1 200 OK\n'
        server_message += time.strftime('Date: %a, %d %b %Y %X GMT\n', 
            time.gmtime())
        server_message += 'Server: Web Server\n'
        server_message += 'Last-Modified: Sun, 25 Apr 2021 17:03:30 GMT\n'
        server_message += 'Accept-Ranges: bytes\n'

        content = 'What\'s up?\nYour IP address is {}\n'.format(
            self.client_address)

        server_message += 'Content-Length: {}\n'.format(len(content))
        server_message += 'Content-Type: text/html\n\n'
        server_message += content

        self.client_socket.send(server_message.encode())
        self.client_socket.close()


threads = []
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

server_address = ('', 80)
server_socket.bind(server_address)
server_socket.listen(50)

while True:
    client_socket, (client_address, port) = server_socket.accept()
    newthread = ClientHandler(client_socket, client_address)
    newthread.start()
    threads.append(newthread)

    if len(threads) >= 50:
        for t in threads:
            t.join()
        threads = []
