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
    // critical section
    mutex.WaitOne();
    // binarySemaphore.WaitOne(); // using a binary semaphore
    ++counter;
    Console.WriteLine("Handling request #" + counter);
    // binarySemaphore.Release();
    mutex.ReleaseMutex();

    if (arg == null) { return; }
    using TcpClient client = (TcpClient)arg;
    NetworkStream stream = client.GetStream();

    string serverMessage = "HTTP/1.1 200 OK\n";

    serverMessage += "Date: " + DateTime.UtcNow.ToString("R") + "\n";

    serverMessage += "Server: Web Server\n";
    serverMessage += "Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\n";
    serverMessage += "Accept-Ranges: bytes\n";

    StringBuilder content = new();
    content.Append("What's up? This server was written in C#/.NET. Your IP address is ");
    if (client.Client.RemoteEndPoint == null) { return; }
    content.Append(((IPEndPoint)client.Client.RemoteEndPoint).Address.ToString());
    content.AppendLine();

    serverMessage += $"Content-Length: {content.Length}\n";
    serverMessage += "Content-Type: text/html\n\n";
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

using (Process process = new())
{
    process.StartInfo.FileName = "date";
    process.StartInfo.UseShellExecute = false;
    process.StartInfo.RedirectStandardOutput = true;
    process.Start();
    using StreamReader reader = process.StandardOutput;
    Console.Write("Current time: " + reader.ReadToEnd());
}

IPAddress[] ipaddrs = Dns.GetHostAddresses("www.google.com");
Console.WriteLine("IPv4 addresses associated with www.google.com:");
foreach (IPAddress res in ipaddrs)
{
    if (res.AddressFamily == AddressFamily.InterNetwork)
    {
        Console.WriteLine(res.ToString());
    }
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
    NetworkStream stream;
    Thread[] threads = new Thread[60];
    int threadIndex = 0;
    byte[] clientMessageBytes = new byte[4096];
    string clientMessage, requestLine;
    while (true)
    {
        // To connect to a server:
        // using TcpClient conn = new("127.0.0.1", 5000);
        client = listener.AcceptTcpClient();
        stream = client.GetStream();
        // use stream.ReadExactly(clientMessageBytes); to wait for all bytes
#pragma warning disable CA2022 // Avoid inexact read with 'Stream.Read'
        stream.Read(clientMessageBytes, 0, 4096);
#pragma warning restore CA2022 // Avoid inexact read with 'Stream.Read'
        clientMessage = Encoding.ASCII.GetString(clientMessageBytes);
        requestLine = clientMessage.Split('\n', 2)[0]; // this keeps the rest of the body in one string
        Console.WriteLine(requestLine);
        threads[threadIndex] = new Thread(new ParameterizedThreadStart(ClientHandler));
        threads[threadIndex++].Start(client);

        if (threadIndex >= 50)
        {
            threadIndex = 0;
            while (threadIndex < 50)
            {
                threads[threadIndex++].Join();
            }
            threadIndex = 0;
        }
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
