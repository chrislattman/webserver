package main

import (
    "fmt"
    "net"
    "strings"
    "time"
)

func handleConnection(conn net.Conn) {
    full_address := conn.RemoteAddr().String()
    address_split := strings.Split(full_address, ":")

    server_message := "HTTP/1.1 200 OK\n"
    date := time.Now().UTC().Format(time.RFC1123)
    date_split := strings.Split(date, "UTC")
    server_message += "Date: " + date_split[0] + "GMT\n"
    server_message += "Server: Web Server\n"
    server_message += "Last-Modified: Sun, 17 Apr 2022 01:58:09 GMT\n"
    server_message += "Accept-Ranges: bytes\n"

    content := "What's up? Your IP address is " + address_split[0] + "\n"

    server_message += "Content-Length: " + fmt.Sprintln(len(content))
    server_message += "Content-Type: text/html\n\n"
    server_message += content

    _, err := conn.Write([]byte(server_message))
    if err != nil {
        fmt.Println("net.Conn.Write:", err)
    }
    err = conn.Close()
    if err != nil {
        fmt.Println("net.Conn.Close:", err)
    }
}

func main() {
    ln, err := net.Listen("tcp", ":80")
    if err != nil {
        fmt.Println("net.Listen:", err)
        goto shutdown
    }

    for {
        conn, err := ln.Accept()
        if err != nil {
            fmt.Println("net.Listener.Accept:", err)
            goto shutdown
        }
        go handleConnection(conn)
    }

shutdown:
    err = ln.Close()
    if err != nil {
        fmt.Println("net.Listener.Close:", err)
    }
}
