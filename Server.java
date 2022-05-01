import java.io.PrintWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TimeZone;

public class Server {
    private static ServerSocket serverSocket;
    private static ClientHandler[] threads;
    private static int threadIndex;

    public static void main(String[] args) {
        start();
    }

    public static void start() {
        try {
            serverSocket = new ServerSocket(80);
            threads = new ClientHandler[60];
            threadIndex = 0;
            while (true) {
                threadIndex++;
                threads[threadIndex] = new ClientHandler(serverSocket.accept());
                threads[threadIndex].start();
                if (threadIndex >= 50) {
                    threadIndex = 0;
                    while (threadIndex < 50) {
                        threads[threadIndex++].join();
                    }
                    threadIndex = 0;
                }
            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }
        finally {
            stop();
        }
    }

    public static void stop() {
        try {
            serverSocket.close();
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }

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

            }
            catch (Exception e) {
                e.printStackTrace();
            }
        }
    }
}
