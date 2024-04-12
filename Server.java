import java.io.BufferedReader;
import java.io.InputStreamReader;
// import java.io.BufferedInputStream;
// import java.nio.charset.StandardCharsets;
import java.io.PrintWriter;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TimeZone;

/**
 * Server class which contains the main server loop and client thread.
 */
public class Server {
    private static final int PORT_NUMBER = 8080;
    private static ServerSocket serverSocket;
    private static Socket clientSocket;
    // private static byte[] clientMessageBytes;
    // private static BufferedInputStream in;
    // private static String clientMessage;
    private static BufferedReader bufferedReader;
    private static String requestLine;
    private static ClientHandler[] threads;
    private static int threadIndex;

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
            try {
                out = new PrintWriter(this.clientSocket.getOutputStream(), true);

                currentTime = new SimpleDateFormat(
                        "'Date: 'EEE, d MMM yyyy HH:mm:ss z");
                currentTime.setTimeZone(TimeZone.getTimeZone("GMT"));

                content = new StringBuilder();

                out.println("HTTP/1.1 200 OK");
                out.println(currentTime.format(new Date()));
                out.println("Server: Web Server");
                out.println("Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT");
                out.println("Accept-Ranges: bytes");

                content.append("What's up? Your IP address is ");
                content.append(this.clientSocket.getInetAddress().getHostAddress());
                content.append("\n");

                out.println("Content-Length: " + content.length());
                out.println("Content-Type: text/html");
                out.println();
                out.print(content.toString());

                out.close(); // same effect as clientSocket.shutdownOutput()
                this.clientSocket.close();
            } catch (Exception e) {
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
        int port_number = 0;

        if (args.length == 1) {
            port_number = Integer.parseInt(args[0]) & 65535;
        }

        try {
            if (port_number >= 10000) {
                serverSocket = new ServerSocket(port_number, Integer.MAX_VALUE,
                    InetAddress.getLoopbackAddress());
            } else {
                serverSocket = new ServerSocket(PORT_NUMBER, Integer.MAX_VALUE,
                    InetAddress.getLoopbackAddress());
            }
            serverSocket.setReuseAddress(true);
            threads = new ClientHandler[60];
            threadIndex = 0;
            while (true) {
                // To connect to a server:
                // Socket sock = new Socket("127.0.0.1", 5000);
                clientSocket = serverSocket.accept();
                // clientMessageBytes = new byte[4096];
                // in = new BufferedInputStream(clientSocket.getInputStream());
                // in.read(clientMessageBytes);
                // clientMessage = new String(clientMessageBytes, StandardCharsets.UTF_8);
                // requestLine = clientMessage.split("\n", 2)[0];
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
        } catch (Exception e) {
            e.printStackTrace();
            try {
                serverSocket.close();
            } catch (Exception exc) {
                exc.printStackTrace();
            }
        }
    }
}
