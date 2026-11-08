using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Text;

const int PORT_NUMBER = 8080;
Mutex mutex = new();
Semaphore binarySemaphore = new(0, 1);
int counter = 0;

void ClientHandler(object? arg)
{
    if (arg == null) { return; }
    // Disposing the TcpClient closes the socket, even after an error
    using TcpClient client = (TcpClient)arg;
    try
    {
        HandleRequest(client);
    }
    catch (Exception e) when (e is IOException || e is SocketException || e is ObjectDisposedException)
    {
        // Only affects this connection
        Console.WriteLine("ClientHandler: " + e.Message);
    }
}

void HandleRequest(TcpClient client)
{
    NetworkStream stream = client.GetStream();

    // Reading the request happens here rather than in the main loop so that a
    // slow or silent client only holds up its own thread, not the accept loop.
    // The timeout bounds how long an idle client can tie it up.
    stream.ReadTimeout = 10000;
    byte[] clientMessageBytes = new byte[4096];
    // use stream.ReadExactly(clientMessageBytes); to wait for all bytes
    int n = stream.Read(clientMessageBytes, 0, clientMessageBytes.Length);
    if (n <= 0) { return; } // client closed the connection without sending anything
    // Only look at the bytes received for THIS request
    string clientMessage = Encoding.ASCII.GetString(clientMessageBytes, 0, n);
    string requestLine = clientMessage.Split("\r\n", 2)[0]; // this keeps the rest of the body in one string
    // Only accepting HTTP GET requests
    if (!requestLine.StartsWith("GET"))
    {
        client.Client.Shutdown(SocketShutdown.Send);
        return;
    }
    Console.WriteLine(requestLine);

    // critical section
    mutex.WaitOne();
    // binarySemaphore.WaitOne(); // using a binary semaphore
    ++counter;
    Console.WriteLine("Handling request #" + counter);
    // binarySemaphore.Release();
    mutex.ReleaseMutex();

    string serverMessage = "HTTP/1.1 200 OK\r\n";

    serverMessage += "Date: " + DateTime.UtcNow.ToString("R") + "\r\n";

    serverMessage += "Server: Web Server\r\n";
    serverMessage += "Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\r\n";
    serverMessage += "Accept-Ranges: bytes\r\n";

    StringBuilder content = new();
    content.Append("What's up? This server was written in C#/.NET. Your IP address is ");
    if (client.Client.RemoteEndPoint == null) { return; }
    content.Append(((IPEndPoint)client.Client.RemoteEndPoint).Address.ToString() + "\r");
    content.AppendLine();

    serverMessage += $"Content-Length: {content.Length}\r\n";
    serverMessage += "Content-Type: text/html\r\n\r\n";
    serverMessage += content.ToString();

    stream.Write(Encoding.ASCII.GetBytes(serverMessage));
}

Nettype netType = Nettype.IPv4;
switch (netType)
{
    case Nettype.IPv4:
        Console.WriteLine("Using IPv4");
        break;
    case Nettype.IPv6:
        Console.WriteLine("Using IPv6");
        break;
    default:
        Console.WriteLine("Unknown protocol.");
        break;
}

try
{
    using Process process = new();
    process.StartInfo.FileName = "date";
    process.StartInfo.UseShellExecute = false;
    process.StartInfo.RedirectStandardOutput = true;
    process.Start();
    using StreamReader reader = process.StandardOutput;
    Console.Write("Current time: " + reader.ReadToEnd());
}
catch (System.ComponentModel.Win32Exception e)
{
    // e.g. there's no "date" executable on Windows
    Console.WriteLine("Process.Start: " + e.Message);
}

try
{
    IPAddress[] ipaddrs = Dns.GetHostAddresses("www.google.com");
    Console.WriteLine("IPv4 addresses associated with www.google.com:");
    foreach (IPAddress res in ipaddrs)
    {
        if (res.AddressFamily == AddressFamily.InterNetwork)
        {
            Console.WriteLine(res.ToString());
        }
    }
}
catch (SocketException e)
{
    Console.WriteLine("Dns.GetHostAddresses: " + e.Message);
}

int portNumber = 0;
if (args.Length == 1)
{
    portNumber = int.Parse(args[0]) & 65535;
}

TcpListener listener;
if (portNumber > 10000)
{
    listener = new(IPAddress.Loopback, portNumber); // IPAddress.IPv6Loopback for IPv6
}
else
{
    listener = new(IPAddress.Loopback, PORT_NUMBER); // IPAddress.IPv6Loopback for IPv6
}

try
{
    listener.Server.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
    listener.Start();

    TcpClient client;
    while (true)
    {
        // To connect to a server:
        // using TcpClient conn = new("127.0.0.1", 5000);
        try
        {
            client = listener.AcceptTcpClient();
        }
        catch (SocketException e) when (e.SocketErrorCode != SocketError.Interrupted && e.SocketErrorCode != SocketError.OperationAborted)
        {
            // e.g. ConnectionReset: only affects one connection
            Console.WriteLine("AcceptTcpClient: " + e.Message);
            continue;
        }
        // Joining threads in batches would make the accept loop wait on the
        // slowest client in the batch, so let each thread finish on its own.
        // Background threads don't keep the process alive after Ctrl + C.
        Thread thread = new(new ParameterizedThreadStart(ClientHandler)) { IsBackground = true };
        thread.Start(client);
    }
}
catch (SocketException e)
{
    Console.WriteLine("{0}", e.StackTrace);
    try
    {
        listener.Stop();
    }
    catch (SocketException exc)
    {
        Console.WriteLine("{0}", exc.StackTrace);
    }
}

enum Nettype
{
    IPv4,
    IPv6
}
