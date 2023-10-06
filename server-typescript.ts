/**
 * To compile this TypeScript file into JavaScript, run
 * $ npm i -D typescript @types/node
 * $ npx tsc --strict server-typescript.ts
 */
import { Socket, createServer } from "node:net";
import { Buffer } from "node:buffer";

const PORT_NUMBER = 8080;
const INT_MAX = 2147483647;

/**
 * Prints out error if it isn't undefined.
 *
 * @param err possible error
 */
function checkError(err: Error | undefined) {
    if (err !== undefined) {
        console.error(err);
    }
}

/**
 * Main server loop.
 */
function main() {
    let server = createServer((socket: Socket) => {
        socket.on("data", (data: Buffer) => {
            let client_message = data.toString();
            let request_line = client_message.substring(0, client_message.indexOf("\n"));
            console.log(request_line);
        });
        let server_message = "HTTP/1.1 200 OK\n";
        let now = new Date();
        server_message += `Date: ${now.toUTCString()}\n`;
        server_message += "Server: Web Server\n";
        server_message += "Last-Modified: Fri, 08 Apr 2022 12:35:05 GMT\n";
        server_message += "Accept-Ranges: bytes\n"

        let content = `What's up? Your IP address is ${socket.remoteAddress}\n`;

        server_message += `Content-Length: ${content.length}\n`;
        server_message += "Content-Type: text/html\n\n";
        server_message += content;
        socket.end(server_message);
    });
    server.on("error", (err: Error) => {
        console.error(err);
        server.close(checkError);
    });
    server.listen(PORT_NUMBER, "127.0.0.1", INT_MAX, () => {
        process.on("SIGINT", () => {
            server.close(checkError);
            process.exit(0);
        });
    });
}

main();
