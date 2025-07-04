use std::{
    env::args,
    io::{BufRead, BufReader, Write},
    net::{Shutdown, TcpListener, TcpStream, ToSocketAddrs},
    process::Command,
    str::from_utf8,
    sync::Mutex,
    thread::spawn,
    // io::Read,
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
fn client_handler(mut stream: TcpStream) {
    // critical section
    {
        let mut counter = MUTEX.lock().unwrap();
        *counter += 1;
        println!("Handling request #{}", *counter);
    }

    let mut server_message = "HTTP/1.1 200 OK\n".to_string();

    let date = Utc::now().to_rfc2822().replace(" +0000", "");
    server_message += &format!("Date: {date} GMT\n");

    server_message += "Server: Web Server\n";
    server_message += "Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\n";
    server_message += "Accept-Ranges: bytes\n";

    let client_address = stream.peer_addr().unwrap().ip();
    let content = format!(
        "What's up? This server was written in Rust. Your IP address is {client_address}\n"
    );

    server_message += &format!("Content-Length: {}\n", content.len());
    server_message += "Content-Type: text/html\n\n";
    server_message += &content;

    stream.write_all(server_message.as_bytes()).unwrap();
    stream.shutdown(Shutdown::Write).unwrap();
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

    let mut threads = Vec::new();
    for incoming_stream in listener.incoming() {
        // let mut stream = incoming_stream.unwrap();
        // let mut client_message_bytes = [0u8; 4096];
        // use stream.read_exact(&mut client_message_bytes).unwrap(); to wait for client_message_bytes to fill up entirely
        // stream.read(&mut client_message_bytes).unwrap();
        // let client_message = from_utf8(&client_message_bytes).unwrap();
        // let (request_line, _) = client_message.split_once("\n").unwrap();
        let stream = incoming_stream.unwrap();
        let buf_reader = BufReader::new(&stream);
        let request_line = buf_reader.lines().next().unwrap().unwrap();
        println!("{request_line}");
        threads.push(spawn(|| {
            client_handler(stream);
        }));

        if threads.len() >= 50 {
            while !threads.is_empty() {
                let curr_thread = threads.remove(0);
                curr_thread.join().unwrap();
            }
        }
    }
}
