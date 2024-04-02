# webserver
This is a simple multithreaded web server I made which simply returns the client's IP address. It currently only works on IPv4.

After running the server, you can connect to it at http://127.0.0.1:8080. You can specify a different listening port as a command line argument passed to the server, e.g. `./server 10023`. Only port numbers greater than 10000 are accepted.

Note: the threading (where applicable) used in these files is extremely basic. A real web server would use a thread pool.

Another thing to note is that a real web server would need to validate each HTTP request it receives.
