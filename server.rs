use std::{
    env::args,
    io::{self, BufRead, BufReader, Read, Write},
    net::{Shutdown, TcpListener, TcpStream, ToSocketAddrs},
    process::Command,
    str::from_utf8,
    sync::Mutex,
    thread::spawn,
    time::Duration,
};

use chrono::Utc;

const PORT_NUMBER: u16 = 8080;
static MUTEX: Mutex<u64> = Mutex::new(0);
// Rust supports multiple producer, single consumer (mpsc) channels (not exactly like Go channels)
// Rust std deprecated Semaphore since v1.7.0: https://doc.rust-lang.org/1.7.0/std/sync/struct.Semaphore.html
// Check out tokio: https://docs.rs/tokio/latest/tokio/sync/struct.Semaphore.html

#[derive(PartialEq)]
enum Nettype {
    IPv4,
    IPv6,
}

/// Thread that handles each client connection.
fn client_handler(stream: TcpStream) {
    // Errors only affect this connection; log them instead of panicking
    if let Err(e) = handle_request(&stream) {
        eprintln!("client_handler: {e}");
    }
    // The client may already be gone, so ignore errors here
    let _ = stream.shutdown(Shutdown::Write);
}

/// Reads one request from the client and answers it.
fn handle_request(mut stream: &TcpStream) -> io::Result<()> {
    // Reading the request happens here rather than in main() so that a slow
    // or silent client only holds up its own thread, not the accept loop.
    // The timeout bounds how long an idle client can tie it up.
    stream.set_read_timeout(Some(Duration::from_secs(10)))?;

    // let mut client_message_bytes = [0u8; 4096];
    // use stream.read_exact(&mut client_message_bytes) to wait for client_message_bytes to fill up entirely
    // let n = stream.read(&mut client_message_bytes)?;
    // let client_message = String::from_utf8_lossy(&client_message_bytes[..n]);
    // let request_line = client_message.split("\r\n").next().unwrap_or_default();
    //
    // take() caps how much a client can make us buffer while looking for the
    // end of the line, and from_utf8_lossy() tolerates invalid UTF-8
    let mut request_line_bytes = Vec::new();
    BufReader::new(stream.take(4096)).read_until(b'\n', &mut request_line_bytes)?;
    let request_line = String::from_utf8_lossy(&request_line_bytes);
    let request_line = request_line.trim_end_matches(['\r', '\n']);
    // Only accepting HTTP GET requests
    if !request_line.starts_with("GET") {
        return Ok(());
    }
    println!("{request_line}");

    // critical section
    {
        // A panic in another thread "poisons" the mutex; the counter is still usable
        let mut counter = MUTEX.lock().unwrap_or_else(|e| e.into_inner());
        *counter += 1;
        println!("Handling request #{}", *counter);
    }

    let mut server_message = "HTTP/1.1 200 OK\r\n".to_string();

    let date = Utc::now().to_rfc2822().replace(" +0000", "");
    server_message += &format!("Date: {date} GMT\r\n");

    server_message += "Server: Web Server\r\n";
    server_message += "Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\r\n";
    server_message += "Accept-Ranges: bytes\r\n";

    let client_address = stream.peer_addr()?.ip();
    let content = format!(
        "What's up? This server was written in Rust. Your IP address is {client_address}\r\n"
    );

    server_message += &format!("Content-Length: {}\r\n", content.len());
    server_message += "Content-Type: text/html\r\n\r\n";
    server_message += &content;

    stream.write_all(server_message.as_bytes())
}

/// Main server loop for the web server.
fn main() {
    let net_type = Nettype::IPv4;
    match net_type {
        Nettype::IPv4 => println!("Using IPv4"),
        Nettype::IPv6 => println!("Using IPv6"),
        // _ => println!("Unknown protocol."),
    }

    let output = Command::new("date").output().unwrap().stdout;
    let date = from_utf8(&output).unwrap();
    print!("Current time: {date}");

    let ipaddrs = "www.google.com:443".to_socket_addrs().unwrap();
    println!("IPv4 addresses associated with www.google.com:");
    for res in ipaddrs {
        if res.is_ipv4() {
            println!("{}", res.to_string().replace(":443", ""));
        }
    }

    let mut port_number: u16 = 0;
    let args: Vec<_> = args().collect();
    if args.len() == 2 {
        let args1 = &args[1];
        port_number = args1.parse().unwrap();
    }

    // To connect to a server:
    // let sock = TcpStream::connect("127.0.0.1:5000").unwrap();
    let listener = if port_number >= 10000 {
        // No default way of setting SO_REUSEADDR
        // See https://docs.rs/socket2/latest/socket2/
        // "[::1]:" for IPv6
        TcpListener::bind("127.0.0.1:".to_owned() + &port_number.to_string()).unwrap()
    } else {
        // No default way of setting SO_REUSEADDR
        // See https://docs.rs/socket2/latest/socket2/
        // "[::1]:" for IPv6
        TcpListener::bind("127.0.0.1:".to_owned() + &PORT_NUMBER.to_string()).unwrap()
    };

    for incoming_stream in listener.incoming() {
        let stream = match incoming_stream {
            Ok(stream) => stream,
            // e.g. ECONNABORTED or EMFILE: only affects one connection
            Err(e) => {
                eprintln!("accept: {e}");
                continue;
            }
        };
        // Dropping the JoinHandle detaches the thread. Joining threads in
        // batches would make the accept loop wait on the slowest client.
        spawn(|| {
            client_handler(stream);
        });
    }
}
