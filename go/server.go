package main

import (
	"fmt"
	"net"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"
	"time"
)

const PORT_NUMBER int = 8080

var ln net.Listener

// Handles client connections.
func handleConnection(conn net.Conn) {
	full_address := conn.RemoteAddr().String()
	full_address = strings.ReplaceAll(full_address, "[", "")
	full_address = strings.ReplaceAll(full_address, "]", "")
	lastIndex := strings.LastIndex(full_address, ":")

	server_message := "HTTP/1.1 200 OK\n"
	date := time.Now().UTC().Format(time.RFC1123)
	date_split := strings.Split(date, "UTC")
	server_message += "Date: " + date_split[0] + "GMT\n"
	server_message += "Server: Web Server\n"
	server_message += "Last-Modified: Thu, 4 Apr 2024 16:45:18 GMT\n"
	server_message += "Accept-Ranges: bytes\n"

	content := "What's up? Your IP address is " + full_address[:lastIndex] + "\n"

	server_message += "Content-Length: " + fmt.Sprintln(len(content))
	server_message += "Content-Type: text/html\n\n"
	server_message += content

	_, err := conn.Write([]byte(server_message))
	if err != nil {
		fmt.Println("net.Conn.Write:", err)
		return
	}
	err = conn.Close()
	if err != nil {
		fmt.Println("net.Conn.Close:", err)
		return
	}
}

// Signal handler for Ctrl + C (SIGINT).
func signalHandler() {
	err := ln.Close()
	if err != nil {
		fmt.Println("net.Listener.Close:", err)
	}
	os.Exit(0)
}

// Main server loop for the web server.
func main() {
	var port_number = 0
	if len(os.Args) == 2 {
		port_number, _ = strconv.Atoi(os.Args[1])
		port_number &= 65535
	}

	signal_channel := make(chan os.Signal, 1)
	var err error
	// No default way of setting SO_REUSEADDR
	if port_number >= 10000 {
		ln, err = net.Listen("tcp", "127.0.0.1:"+fmt.Sprint(port_number))
	} else {
		ln, err = net.Listen("tcp", "127.0.0.1:"+fmt.Sprint(PORT_NUMBER))
	}
	if err != nil {
		fmt.Println("net.Listen:", err)
		goto shutdown
	}
	signal.Notify(signal_channel, syscall.SIGINT)
	go func() {
		<-signal_channel
		signalHandler()
	}()

	for {
		conn, err := ln.Accept()
		if err != nil {
			fmt.Println("net.Listener.Accept:", err)
			goto shutdown
		}
		clientMessageBytes := make([]byte, 4096)
		_, err = conn.Read(clientMessageBytes)
		if err != nil {
			fmt.Println("net.conn.Read:", err)
			goto shutdown
		}
		clientMessage := string(clientMessageBytes)
		request_line, _, _ := strings.Cut(clientMessage, "\n")
		// request_line := clientMessage[:strings.Index(clientMessage, "\n")]
		// request_line := clientMessage[:strings.IndexRune(clientMessage, '\n')]
		fmt.Println(request_line)
		go handleConnection(conn)
	}

shutdown:
	err = ln.Close()
	if err != nil {
		fmt.Println("net.Listener.Close:", err)
	}
}
