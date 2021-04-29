import java.io.PrintWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TimeZone;

public class Server {
    private ServerSocket serverSocket;

    public static void main(String[] args) {
        Server server = new Server();
        server.start();
    }

    public void start() {
        try {
            serverSocket = new ServerSocket(80);
            while (true) {
                new ClientHandler(serverSocket.accept()).start();
            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }
        finally {
            stop();
        }
    }

    public void stop() {
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
                out.println("Last-Modified: Sat, 10 Apr 2021 00:10:30 GMT");
                out.println("Accept-Ranges: bytes");

                content.append("What's up?\n");
                content.append("Your current IP address is ");
                content.append(clientSocket.getInetAddress().getHostAddress());

                out.println("Content-Length: " + content.length());
                out.println("Content-Type: text/html");
                out.println();
                out.println(content.toString());

                out.close(); // same effect as clientSocket.shutdownOutput()
                clientSocket.close();

            }
            catch (Exception e) {
                e.printStackTrace();
            }
        }
    }
}
