use std::{env, io::{BufRead, BufReader, Write}, net::{TcpListener, TcpStream}, thread};

const PORT_NUMBER: u16 = 8080;

/// Thread that handles each client connection.
fn client_handler(mut stream: TcpStream) {
    let mut server_message = "HTTP/1.1 200 OK\n".to_string();
    // get current timestamp with chrono
    server_message += "Server: Web Server\n";
    server_message += "Last-Modified: Fri, 08 Apr 2022 12:35:05 GMT\n";
    server_message += "Accept-Ranges: bytes\n";

    let client_address = stream.peer_addr().unwrap().ip();
    let content = format!("What's up? Your IP address is {}\n", client_address);

    server_message += &format!("Content-Length: {}\n", content.len());
    server_message += "Content-Type: text/html\n\n";
    server_message += &content;

    stream.write_all(server_message.as_bytes()).unwrap();
    stream.shutdown(std::net::Shutdown::Write).unwrap();
}

/// Main server loop for the web server.
fn main() {
    let mut port_number: u16 = 0;
    let listener: TcpListener;

    let args: Vec<String> = env::args().collect();
    if args.len() == 2 {
        let args1 = &args[1];
        port_number = args1.parse().unwrap();
    }

    if port_number >= 10000 {
        // SO_REUSEADDR is set by default
        listener = TcpListener::bind(
            "127.0.0.1:".to_owned() + &port_number.to_string()).unwrap()
    } else {
        // SO_REUSEADDR is set by default
        listener = TcpListener::bind(
            "127.0.0.1:".to_owned() + &PORT_NUMBER.to_string()).unwrap()
    }

    for stream in listener.incoming() {
        let stream = stream.unwrap();
        let buf_reader = BufReader::new(&stream);
        let request_line = buf_reader.lines().next().unwrap().unwrap();
        println!("{}", request_line);
        thread::spawn(|| {
            client_handler(stream);
        });
    }
}
