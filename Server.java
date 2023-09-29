import java.io.PrintWriter;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TimeZone;

public class Server {
    private static final int PORT_NUMBER = 8080;
    private static ServerSocket serverSocket;
    private static ClientHandler[] threads;
    private static int threadIndex;

    private static class ClientHandler extends Thread {
        private Socket clientSocket;
        private PrintWriter out;
        private SimpleDateFormat currentTime;
        private StringBuilder content;

        public ClientHandler(Socket socket) {
            clientSocket = socket;
        }

        public void run() {
            try {
                out = new PrintWriter(clientSocket.getOutputStream(), true);

                currentTime = new SimpleDateFormat(
                        "'Date: 'EEE, d MMM yyyy HH:mm:ss z");
                currentTime.setTimeZone(TimeZone.getTimeZone("GMT"));

                content = new StringBuilder();

                out.println("HTTP/1.1 200 OK");
                out.println(currentTime.format(new Date()));
                out.println("Server: Web Server");
                out.println("Last-Modified: Fri, 08 Apr 2022 12:35:05 GMT");
                out.println("Accept-Ranges: bytes");

                content.append("What's up? Your IP address is ");
                content.append(clientSocket.getInetAddress().getHostAddress());
                content.append("\n");

                out.println("Content-Length: " + content.length());
                out.println("Content-Type: text/html");
                out.println();
                out.print(content.toString());

                out.close(); // same effect as clientSocket.shutdownOutput()
                clientSocket.close();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    public static void main(String[] args) {
        try {
            serverSocket = new ServerSocket(
                PORT_NUMBER, Integer.MAX_VALUE, InetAddress.getLoopbackAddress());
            threads = new ClientHandler[60];
            threadIndex = 0;
            while (true) {
                threads[threadIndex] = new ClientHandler(serverSocket.accept());
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
