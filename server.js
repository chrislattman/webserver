"use strict";

const child_process = require("node:child_process");
const dns = require("node:dns");
const net = require("node:net");

const PORT_NUMBER = 8080;
const INT_MAX = 2147483647;
var counter = 0;

const Nettype = Object.freeze({
    IPv4: 0,
    IPv6: 1,
});

/**
 * Prints out error if it isn't undefined.
 *
 * @param {Error | undefined} err possible error
 */
function checkError(err) {
    if (err !== undefined) {
        console.error(err);
    }
}

/**
 * Signal handler for Ctrl + C (SIGINT).
 *
 * @param {net.Server} server server to close
 */
function signal_handler(server) {
    process.on("SIGINT", () => {
        server.close(checkError);
        process.exit(0);
    });
}

/**
 * Main server loop for the web server.
 */
function main() {
    const netType = Nettype.IPv4;
    switch (netType) {
        case Nettype.IPv4:
            console.log("Using IPv4");
            break;
        case Nettype.IPv6:
            console.log("Using IPv6");
            break;
        default:
            console.log("Unknown protocol.");
    }

    let date = child_process.spawnSync("date");
    process.stdout.write(`Current time: ${date.stdout}`);

    dns.lookup("www.google.com", {family: 4, all: true}, (_, ipaddrs) => {
        console.log("IPv4 addresses associated with www.google.com:");
        ipaddrs.forEach((res) => {
            console.log(res.address);
        });
    });

    let port_number = 0;
    if (process.argv.length == 3) {
        port_number = parseInt(process.argv[2]) & 65535;
    }

    // To connect to a server:
    // let client = net.createConnection(5000, "127.0.0.1", () => {});

    // let server = createServer((socket) => {
    let server = net.createServer((socket) => {
        // "critical section"
        ++counter;
        console.log("Handling request #" + counter);

        // use const client_message_bytes = Buffer.from(socket.read(4096)); to wait for all bytes
        socket.on("data", (client_message_bytes) => {
            let client_message = client_message_bytes.toString();
            let request_line = client_message.substring(0, client_message.indexOf("\n"));
            console.log(request_line);
        });
        let server_message = "HTTP/1.1 200 OK\n";
        let now = new Date();
        server_message += `Date: ${now.toUTCString()}\n`;
        server_message += "Server: Web Server\n";
        server_message += "Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\n";
        server_message += "Accept-Ranges: bytes\n"

        let content = `What's up? Your IP address is ${socket.remoteAddress}\n`;

        server_message += `Content-Length: ${content.length}\n`;
        server_message += "Content-Type: text/html\n\n";
        server_message += content;
        socket.end(server_message);
    });
    server.on("error", (err) => {
        console.error(err);
        server.close(checkError);
    });
    if (port_number >= 10000) {
        // SO_REUSEADDR is set by default
        server.listen(port_number, "127.0.0.1", INT_MAX, () => { // "::1" for IPv6
            signal_handler(server);
        });
    } else {
        // SO_REUSEADDR is set by default
        server.listen(PORT_NUMBER, "127.0.0.1", INT_MAX, () => { // "::1" for IPv6
            signal_handler(server);
        });
    }
}

if (require.main === module) {
    main();
}
