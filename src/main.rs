use std::{
    env,
    io::{BufRead, BufReader, Write},
    // io::Read,
    net::{Shutdown, TcpListener, TcpStream},
    thread,
    // str,
};

use chrono::Utc;

const PORT_NUMBER: u16 = 8080;

/// Thread that handles each client connection.
fn client_handler(mut stream: TcpStream) {
    let mut server_message = "HTTP/1.1 200 OK\n".to_string();
    let date = Utc::now().to_rfc2822().replace(" +0000", "");
    server_message += &format!("Date: {} GMT\n", date);
    server_message += "Server: Web Server\n";
    server_message += "Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\n";
    server_message += "Accept-Ranges: bytes\n";

    let client_address = stream.peer_addr().unwrap().ip();
    let content = format!("What's up? Your IP address is {}\n", client_address);

    server_message += &format!("Content-Length: {}\n", content.len());
    server_message += "Content-Type: text/html\n\n";
    server_message += &content;

    stream.write_all(server_message.as_bytes()).unwrap();
    stream.shutdown(Shutdown::Write).unwrap();
}

/// Main server loop for the web server.
fn main() {
    let mut port_number: u16 = 0;

    let args: Vec<String> = env::args().collect();
    if args.len() == 2 {
        let args1 = &args[1];
        port_number = args1.parse().unwrap();
    }

    let listener = if port_number >= 10000 {
        // No default way of setting SO_REUSEADDR
        TcpListener::bind("127.0.0.1:".to_owned() + &port_number.to_string()).unwrap()
    } else {
        // No default way of setting SO_REUSEADDR
        TcpListener::bind("127.0.0.1:".to_owned() + &PORT_NUMBER.to_string()).unwrap()
    };

    for incoming_stream in listener.incoming() {
        // let mut stream = incoming_stream.unwrap();
        // let mut buf = [0u8; 4096];
        // stream.read(&mut buf).unwrap();
        // let client_message = str::from_utf8(&buf).unwrap();
        // let (request_line, _) = client_message.split_once("\n").unwrap();
        // println!("{}", request_line);
        let stream = incoming_stream.unwrap();
        let buf_reader = BufReader::new(&stream);
        let request_line = buf_reader.lines().next().unwrap().unwrap();
        println!("{}", request_line);
        thread::spawn(|| {
            client_handler(stream);
        });
    }
}
