import java.io.BufferedReader;
// import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.UnknownHostException;
// import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
// import java.util.concurrent.Semaphore;
import java.util.concurrent.locks.ReentrantLock;
import java.util.Date;
import java.util.TimeZone;

/**
 * Server class which contains the main server loop and client thread.
 */
public class Server {
    enum Nettype {
        IPv4,
        IPv6
    }

    private static final int PORT_NUMBER = 8080;
    private static ReentrantLock mutex;
    // private static Semaphore binarySemaphore;
    private static long counter = 0;

    /**
     * Handles client connections.
     */
    private static class ClientHandler extends Thread {
        private Socket clientSocket;
        private PrintWriter out;
        private SimpleDateFormat currentTime;
        private StringBuilder content;

        /**
         * Constructor for client connection thread.
         *
         * @param socket socket of client
         */
        public ClientHandler(Socket socket) {
            this.clientSocket = socket;
        }

        /**
         * Called by the start() function asynchronously.
         */
        public void run() {
            // critical section
            mutex.lock(); // using a mutex
            // binarySemaphore.acquireUninterruptibly(); // using a binary semaphore
            ++counter;
            System.out.println("Handling request #" + counter);
            // binarySemaphore.release();
            mutex.unlock();

            try {
                // use out = new DataOutputStream(this.clientSocket.getOutputStream());
                // to write a byte array to a socket
                out = new PrintWriter(this.clientSocket.getOutputStream(), true);

                out.println("HTTP/1.1 200 OK\r");

                currentTime = new SimpleDateFormat("'Date: 'EEE, d MMM yyyy HH:mm:ss z");
                currentTime.setTimeZone(TimeZone.getTimeZone("GMT"));
                out.println(currentTime.format(new Date()) + "\r");

                out.println("Server: Web Server\r");
                out.println("Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\r");
                out.println("Accept-Ranges: bytes\r");

                content = new StringBuilder();
                content.append("What's up? This server was written in Java. Your IP address is ");
                content.append(this.clientSocket.getInetAddress().getHostAddress());
                content.append("\r\n");

                out.println("Content-Length: " + content.length() + "\r");
                out.println("Content-Type: text/html\r");
                out.println("\r");
                out.print(content.toString());

                out.close(); // same effect as clientSocket.shutdownOutput()
                this.clientSocket.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    /**
     * Main server loop for the web server.
     *
     * @param args unused
     */
    public static void main(String[] args) {
        Nettype netType = Nettype.IPv4;
        switch (netType) {
            case IPv4:
                System.out.println("Using IPv4");
                break;
            case IPv6:
                System.out.println("Using IPv6");
                break;
            default:
                System.out.println("Unknown protocol.");
                break;
        }

        String[] cmd = {"date"};
        try {
            Process process = Runtime.getRuntime().exec(cmd);
            BufferedReader output = new BufferedReader(new InputStreamReader(process.getInputStream()));
            System.out.println("Current time: " + output.readLine());
            output.close();
            process.waitFor();
        } catch (IOException | InterruptedException e) {
            e.printStackTrace();
        }

        try {
            InetAddress[] ipaddrs = InetAddress.getAllByName("www.google.com");
            System.out.println("IPv4 addresses associated with www.google.com:");
            for (InetAddress res : ipaddrs) {
                if (res instanceof Inet4Address) {
                    System.out.println(res.getHostAddress());
                }
            }
        } catch (UnknownHostException e) {
            e.printStackTrace();
        }

        int portNumber = 0;
        if (args.length == 1) {
            portNumber = Integer.parseInt(args[0]) & 65535;
        }

        mutex = new ReentrantLock();
        // binarySemaphore = new Semaphore(1);

        ServerSocket serverSocket = null;
        try {
            if (portNumber >= 10000) {
                serverSocket = new ServerSocket(portNumber, Integer.MAX_VALUE,
                    InetAddress.getLoopbackAddress()); // InetAddress.getByName("127.0.0.1") or InetAddress.getByName("::1") for IPv6
            } else {
                serverSocket = new ServerSocket(PORT_NUMBER, Integer.MAX_VALUE,
                    InetAddress.getLoopbackAddress()); // InetAddress.getByName("127.0.0.1") or InetAddress.getByName("::1") for IPv6
            }
            serverSocket.setReuseAddress(true);
            Socket clientSocket;
            ClientHandler[] threads = new ClientHandler[60];
            int threadIndex = 0;
            // byte[] clientMessageBytes;
            // BufferedInputStream in;
            // String clientMessage;
            BufferedReader bufferedReader;
            String requestLine;
            while (true) {
                // To connect to a server:
                // Socket sock = new Socket("127.0.0.1", 5000);
                clientSocket = serverSocket.accept();
                // clientMessageBytes = new byte[4096];
                // in = new BufferedInputStream(clientSocket.getInputStream());
                // use clientMessageBytes = in.readNBytes(4096); to wait for all bytes
                // in.read(clientMessageBytes);
                // clientMessage = new String(clientMessageBytes, StandardCharsets.UTF_8);
                // requestLine = clientMessage.split("\r\n", 2)[0]; // this keeps the rest of the body in one String
                bufferedReader = new BufferedReader(
                    new InputStreamReader(clientSocket.getInputStream()));
                requestLine = bufferedReader.readLine();
                System.out.println(requestLine);
                threads[threadIndex] = new ClientHandler(clientSocket);
                threads[threadIndex++].start();

                if (threadIndex >= 50) {
                    threadIndex = 0;
                    while (threadIndex < 50) {
                        threads[threadIndex++].join();
                    }
                    threadIndex = 0;
                }
            }
        } catch (IOException | InterruptedException e) {
            e.printStackTrace();
            try {
                serverSocket.close();
            } catch (IOException exc) {
                exc.printStackTrace();
            }
        }
    }
}
