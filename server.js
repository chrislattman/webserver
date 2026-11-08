"use strict";

const child_process = require("node:child_process");
const dns = require("node:dns");
const net = require("node:net");

const INT_MAX = 2147483647;
const PORT_NUMBER = 8080;
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
function signalHandler(server) {
    process.on("SIGINT", () => {
        server.close(checkError);
        process.exit(0);
    });
}

/**
 * Sends the HTTP response to a client.
 *
 * @param {net.Socket} socket client socket
 */
function sendResponse(socket) {
    // "critical section"
    ++counter;
    console.log("Handling request #" + counter);

    let serverMessage = "HTTP/1.1 200 OK\r\n";

    let now = new Date();
    serverMessage += `Date: ${now.toUTCString()}\r\n`;

    serverMessage += "Server: Web Server\r\n";
    serverMessage += "Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\r\n";
    serverMessage += "Accept-Ranges: bytes\r\n"

    let content = `What's up? This server was written in JavaScript (Node.js). Your IP address is ${socket.remoteAddress}\r\n`;

    serverMessage += `Content-Length: ${content.length}\r\n`;
    serverMessage += "Content-Type: text/html\r\n\r\n";
    serverMessage += content;
    socket.end(serverMessage);
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
            break;
    }

    let date = child_process.spawnSync("date");
    process.stdout.write(`Current time: ${date.stdout}`);

    dns.lookup("www.google.com", {family: 4, all: true}, (err, ipaddrs) => {
        if (err) {
            // ipaddrs is undefined on failure
            console.error(`lookup: ${err.message}`);
            return;
        }
        console.log("IPv4 addresses associated with www.google.com:");
        ipaddrs.forEach((res) => {
            console.log(res.address);
        });
    });

    let portNumber = 0;
    if (process.argv.length == 3) {
        portNumber = parseInt(process.argv[2]) & 65535;
    }

    // To connect to a server:
    // let client = net.createConnection(5000, "127.0.0.1", () => {});

    // let server = createServer((socket) => {
    let server = net.createServer((socket) => {
        // A client resetting the connection emits "error" on the socket.
        // Without a listener, Node treats it as an uncaught exception and the
        // whole server exits.
        socket.on("error", (err) => {
            console.error(`socket: ${err.message}`);
        });
        // Bound how long an idle client can hold its connection open
        socket.setTimeout(10000, () => {
            socket.destroy();
        });

        // The request can arrive in several "data" events, so buffer it until
        // the whole request line is here (capped at 4096 bytes), and only
        // then decide how to respond
        // use const client_message_bytes = Buffer.from(socket.read(4096)); to wait for all bytes
        let client_message_bytes = Buffer.alloc(0);
        let handled = false;
        const handleRequest = () => {
            if (handled) {
                return;
            }
            handled = true;
            let client_message = client_message_bytes.toString();
            let end = client_message.indexOf("\r\n");
            let request_line = end === -1 ? client_message : client_message.substring(0, end);
            // Only accepting HTTP GET requests
            if (!request_line.startsWith("GET")) {
                socket.end();
                return;
            }
            console.log(request_line);
            sendResponse(socket);
        };
        socket.on("data", (chunk) => {
            client_message_bytes = Buffer.concat([client_message_bytes, chunk]);
            if (client_message_bytes.includes("\r\n") || client_message_bytes.length >= 4096) {
                handleRequest();
            }
        });
        // Client finished sending without a complete request line
        socket.on("end", handleRequest);
    });
    server.on("error", (err) => {
        console.error(err);
        server.close(checkError);
    });
    if (portNumber >= 10000) {
        // SO_REUSEADDR is set by default
        server.listen(portNumber, "127.0.0.1", INT_MAX, () => { // "::1" for IPv6
            signalHandler(server);
        });
    } else {
        // SO_REUSEADDR is set by default
        server.listen(PORT_NUMBER, "127.0.0.1", INT_MAX, () => { // "::1" for IPv6
            signalHandler(server);
        });
    }
}

if (require.main === module) {
    main();
}
